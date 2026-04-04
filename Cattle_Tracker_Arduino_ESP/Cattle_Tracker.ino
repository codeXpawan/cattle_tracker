#include <Arduino.h>
#include <SPI.h>
#include <time.h>
#include "Http_Server.h"

Http_Server httpServer;

/* ------------------------------------------------------------------ */
/*  Pin definitions                                                     */
/* ------------------------------------------------------------------ */
#define SPI_SCK     18
#define SPI_MISO    19
#define SPI_MOSI    23
#define SPI_CS       5
#define INT_PIN     14

/* ------------------------------------------------------------------ */
/*  SPI settings                                                        */
/* ------------------------------------------------------------------ */
#define SPI_FREQ    4000000
static const SPISettings spiSettings(SPI_FREQ, MSBFIRST, SPI_MODE0);

/* ------------------------------------------------------------------ */
/*  Packet definition                                                   */
/* ------------------------------------------------------------------ */
#define BUF_SIZE    64
#define PKT_NEW     0x01
#define PKT_UPDATE  0x02
#define PKT_GONE    0x03

float tempServer;

struct __attribute__((packed)) SpiPacket {
    uint8_t  type;
    uint8_t  slot;
    uint8_t  mac_str[28];
    uint8_t  temp_bytes[4];
    int8_t   rssi;
    uint8_t  pad[29];
};

static_assert(sizeof(SpiPacket) == 64, "SpiPacket size mismatch");

/* ------------------------------------------------------------------ */
/*  Unpack IEEE 754 float                                               */
/* ------------------------------------------------------------------ */
float unpackFloat(const uint8_t *b)
{
    union { float f; uint8_t bytes[4]; } u;
    u.bytes[0] = b[0]; u.bytes[1] = b[1];
    u.bytes[2] = b[2]; u.bytes[3] = b[3];
    return u.f;
}

/* ------------------------------------------------------------------ */
/*  Get ISO 8601 timestamp string  e.g. "2025-04-04T10:32:05"          */
/* ------------------------------------------------------------------ */
String getTimestamp()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        // Fallback: use millis if NTP not synced yet
        unsigned long ms = millis();
        unsigned long s  = ms / 1000;
        unsigned long m  = s / 60;
        unsigned long h  = m / 60;
        char buf[32];
        snprintf(buf, sizeof(buf), "00:00:%02lu:%02lu:%02lu",
                 h % 24, m % 60, s % 60);
        return String(buf);
    }
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(buf);
}

/* ------------------------------------------------------------------ */
/*  Buffers & SPI                                                       */
/* ------------------------------------------------------------------ */
static uint8_t tx_buf[BUF_SIZE];
static uint8_t rx_buf[BUF_SIZE];
SPIClass *vspi = NULL;

void spiTransaction()
{
    vspi->beginTransaction(spiSettings);
    digitalWrite(SPI_CS, LOW);
    vspi->transferBytes(tx_buf, rx_buf, BUF_SIZE);
    digitalWrite(SPI_CS, HIGH);
    vspi->endTransaction();
}

void printBuffer(const char *label, const uint8_t *buf, size_t len)
{
    Serial.print(label);
    for (size_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

/* ------------------------------------------------------------------ */
/*  Parse and process received packet                                   */
/* ------------------------------------------------------------------ */
void processPacket()
{
    const SpiPacket *pkt = reinterpret_cast<const SpiPacket *>(rx_buf);

    if (pkt->type != PKT_NEW    &&
        pkt->type != PKT_UPDATE &&
        pkt->type != PKT_GONE)
    {
        Serial.printf("Ignored: unknown type 0x%02x\n", pkt->type);
        return;
    }

    char mac[29] = {0};
    memcpy(mac, pkt->mac_str, sizeof(pkt->mac_str));
    mac[28] = '\0';

    if (pkt->type == PKT_NEW || pkt->type == PKT_UPDATE)
    {
        float temp = unpackFloat(pkt->temp_bytes);
        tempServer = temp;

        // ✅ Correct call — timestamp generated here
        String timestamp = getTimestamp();
        httpServer.recordTemp(temp, timestamp);

        const char *label = (pkt->type == PKT_NEW) ? "NEW   " : "UPDATE";
        Serial.printf("%s slot=%02u  mac=%-28s  temp=%6.4f C  rssi=%d dBm  time=%s\n",
                      label, pkt->slot, mac, temp, (int)pkt->rssi, timestamp.c_str());
    }
    else if (pkt->type == PKT_GONE)
    {
        Serial.printf("GONE  slot=%02u  mac=%s\n", pkt->slot, mac);
    }
}

/* ------------------------------------------------------------------ */
/*  FreeRTOS task — runs HTTP server on Core 0                          */
/*  (Arduino loop() runs on Core 1 by default)                         */
/* ------------------------------------------------------------------ */
void httpServerTask(void *pvParameters)
{
    httpServer.startServer();   // blocking: calls server.handleClient() in a loop
    vTaskDelete(NULL);          // should never reach here
}

/* ------------------------------------------------------------------ */
/*  Setup                                                               */
/* ------------------------------------------------------------------ */
void setup()
{
    pinMode(12, OUTPUT); pinMode(32, OUTPUT); pinMode(13, OUTPUT);
    digitalWrite(12, HIGH); digitalWrite(32, HIGH); digitalWrite(13, HIGH);

    Serial.begin(115200);
    pinMode(INT_PIN, INPUT);
    pinMode(SPI_CS,  OUTPUT);
    digitalWrite(SPI_CS, HIGH);

    vspi = new SPIClass(VSPI);
    vspi->begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS);

    // Init WiFi, server routes, etc.
    httpServer.init();

    // Sync time via NTP (requires WiFi to be connected inside init())

    // ✅ Start HTTP server in a separate FreeRTOS task on Core 0
    xTaskCreatePinnedToCore(
    httpServerTask,
    "HTTP_Server",
    12288,        // ⬆ increased from 8192
    NULL,
    1,
    NULL,
    0
);

    Serial.println("ESP32 SPI Master ready");
    Serial.println("Waiting for nRF52840 packets...");
    Serial.println("----------------------------------------------------");
}

/* ------------------------------------------------------------------ */
/*  Loop — SPI polling runs on Core 1                                   */
/* ------------------------------------------------------------------ */
void loop()
{
    if (digitalRead(INT_PIN) == HIGH)
    {
        memset(tx_buf, 0, sizeof(tx_buf));
        memset(rx_buf, 0, sizeof(rx_buf));

        spiTransaction();
        printBuffer("RX raw: ", rx_buf, BUF_SIZE);
        processPacket();

        Serial.println("----------------------------------------------------");
    }

    delay(1);
}