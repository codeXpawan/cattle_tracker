#include "http_parser.h"
#include "Http_Server.h"
#include <WiFi.h>

void Http_Server::handleUpdate() {
    HTTPUpload& upload = server->upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Start updating: %s\n", upload.filename.c_str());
        if (!Update.begin()) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true))
            Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
        else
            Update.printError(Serial);
    }
}

void Http_Server::syncTimeFromClient(String isoTime) {
    struct tm t = {};
    sscanf(isoTime.c_str(), "%d-%d-%dT%d:%d:%d",
           &t.tm_year, &t.tm_mon, &t.tm_mday,
           &t.tm_hour, &t.tm_min, &t.tm_sec);
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    time_t epoch = mktime(&t);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    timesynced = true;
    Serial.println("Time synced from client: " + isoTime);
}

void Http_Server::init() {
    // ✅ Step 1: mutex
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("ERROR: Failed to create mutex!");
        return;
    }

    // ✅ Step 2: Start WiFi FIRST — TCP/IP stack must be up before WebServer
    WiFi.mode(WIFI_AP);  // or WIFI_STA if connecting to a router
    WiFi.softAP("CattleTracker", "password123");  // change as needed
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Give the TCP/IP stack time to fully initialize
    delay(500);

    // ✅ Step 3: NOW it's safe to create WebServer
    server = new WebServer(80);

    // Handle CORS preflight for ALL routes
server->onNotFound([this]() {
    if (server->method() == HTTP_OPTIONS) {
        server->sendHeader("Access-Control-Allow-Origin", "*");
        server->sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server->sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server->send(204);
    } else {
        server->sendHeader("Access-Control-Allow-Origin", "*");
        server->send(404, "application/json", "{\"error\":\"Not found\"}");
    }
});
// 1. GET /api/cattle — list of all cattle
server->on("/api/cattle", HTTP_GET, [this]() {
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json",
        "[{\"id\":\"CTL-001\",\"name\":\"Cattle 001\"}]"
    );
});

// 2. GET /api/cattle/CTL-001/stream — SSE stream
// WebServer doesn't support true SSE, so send current data and let
// Next.js poll instead (see frontend fix below)
server->on("/api/cattle/CTL-001/stream", HTTP_GET, [this]() {
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server->send(200, "application/json", "{\"error\":\"Use polling instead\"}");
});

    // ── Routes ──────────────────────────────────────────────────
    server->on("/updateFirmware", HTTP_GET, [this]() {
        server->sendHeader("Connection", "close");
        server->send(200, "text/html", serverIndex);
    });

    server->on("/reboot", HTTP_GET, [this]() {
        server->sendHeader("Connection", "close");
        server->send(200, "text/html", rebootHTML);
        delay(1000);
        ESP.restart();
    });

    server->on("/update", HTTP_POST,
        [this]() {
            server->sendHeader("Connection", "close");
            server->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            delay(500);
            ESP.restart();
        },
        [this]() { this->handleUpdate(); }
    );

    server->on("/api/cattle/time", HTTP_POST, [this]() {
        if (!server->hasArg("plain")) {
            server->send(400, "application/json", "{\"error\":\"No body\"}");
            return;
        }
        String body = server->arg("plain");
        int idx = body.indexOf("\"time\":");
        if (idx == -1) {
            server->send(400, "application/json", "{\"error\":\"Missing time field\"}");
            return;
        }
        int start = body.indexOf("\"", idx + 7) + 1;
        int end   = body.indexOf("\"", start);
        String isoTime = body.substring(start, end);
        syncTimeFromClient(isoTime);
        server->sendHeader("Access-Control-Allow-Origin", "*");
        server->send(200, "application/json", "{\"status\":\"ok\",\"time\":\"" + isoTime + "\"}");
    });

    // Example — temperature route
server->on("/api/cattle/CTL-001/temperature", HTTP_GET, [this]() {
    String json;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        json  = "{";
        json += "\"name\":\"CTL-001\",";
        json += "\"current\":"   + String(currentTemp, 1) + ",";
        json += "\"heartRate\":" + String(heartRate) + ",";
        json += "\"activity\":\"Normal\",";
        json += "\"uptime\":99.8,";
        json += "\"history\":[";
        for (int i = 0; i < historyCount; i++) {
            json += "{\"time\":\"" + history[i].time + "\",";
            json += "\"temp\":"    + String(history[i].temp, 1) + "}";
            if (i < historyCount - 1) json += ",";
        }
        json += "]}";
        xSemaphoreGive(dataMutex);
    } else {
        json = "{\"error\":\"busy\"}";
    }
    sendJSON(200, json);  // ✅ CORS included
});

    server->begin();
    Serial.println("HTTP Server Started");
}

void Http_Server::startServer() {
    while (1) {
        server->handleClient();
        delay(2);
    }
}