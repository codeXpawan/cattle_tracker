#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <WebServer.h>
#include <Update.h>
#include "Arduino.h"
#include "freertos/semphr.h"

class Http_Server {
private:
    WebServer *server = nullptr;  // ✅ pointer — no construction at global scope
    SemaphoreHandle_t dataMutex = NULL;

    const char *serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'>"
                              "<input type='file' name='update'>"
                              "<input type='submit' value='Update'></form>";
    const char *rebootHTML = "<h2>Rebooting...Please Wait.</h2>";

    bool timesynced = false;

    float currentTemp = 0.0;
    int heartRate = 72;

    struct TempPoint {
        String time;
        float temp;
    };

    TempPoint history[50];
    int historyCount = 0;

public:
    Http_Server() {}  // ✅ does nothing — safe at global scope

    void init();
    void handleUpdate();
    void startServer();
    void syncTimeFromClient(String isoTime);

    void recordTemp(float temp, String time) {
        if (dataMutex == NULL) {
            Serial.println("WARN: recordTemp called before mutex created");
            return;
        }
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (historyCount < 50) {
                history[historyCount++] = {time, temp};
            } else {
                for (int i = 0; i < 49; i++) history[i] = history[i + 1];
                history[49] = {time, temp};
            }
            currentTemp = temp;
            xSemaphoreGive(dataMutex);
        }
    }
    // Add this private method to Http_Server.h
void sendJSON(int code, String body) {
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server->sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server->send(code, "application/json", body);
}
};

#endif