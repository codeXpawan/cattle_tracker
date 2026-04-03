/*
 * DS18B20 1-Wire Temperature Sensor Driver
 * Corrected against DS18B20 datasheet (Dallas Semiconductor)
 * Zephyr RTOS, nRF52840, Pin P1.15
 *
 * Key timing references (AC Electrical Characteristics, Figures 11-14, 26):
 *   tRSTL  (reset low)        : 480 µs min
 *   tRSTH  (reset high/RX)    : 480 µs min
 *   tPDHIGH (presence wait)   : 15–60 µs
 *   tPDLOW  (presence pulse)  : 60–240 µs
 *   tSLOT  (time slot)        : 60–120 µs
 *   tLOW0  (write 0 low)      : 60–120 µs
 *   tLOW1  (write 1 low)      : 1–15 µs
 *   tREC   (recovery)         : 1 µs min
 *   tRDV   (read data valid)  : 15 µs from falling edge
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "ds18b20.h"

/* ── Pin definition ─────────────────────────────────────────────────────── */
#define DS_GPIO_NODE    DT_NODELABEL(gpio1)
#define DS_PIN          15

/* ── 1-Wire commands (DS18B20 datasheet Table 4) ────────────────────────── */
#define CMD_SKIP_ROM        0xCC
#define CMD_START_CONVO     0x44    /* Convert T */
#define CMD_READ_SCRATCH    0xBE    /* Read Scratchpad */
#define CMD_WRITE_SCRATCH   0x4E    /* Write Scratchpad */

/* ── Scratchpad byte indices (Figure 8) ─────────────────────────────────── */
#define TEMP_LSB    0
#define TEMP_MSB    1

/* ── Resolution register values (Figure 7, Table 3) ─────────────────────── */
#define TEMP_9_BIT  0x1F    /*  9-bit,  93.75 ms */
#define TEMP_10_BIT 0x3F    /* 10-bit, 187.5  ms */
#define TEMP_11_BIT 0x5F    /* 11-bit, 375    ms */
#define TEMP_12_BIT 0x7F    /* 12-bit, 750    ms (factory default) */

/* ── Max conversion times in ms per resolution (Table 3) ────────────────── */
#define TCONV_9BIT   94U
#define TCONV_10BIT  188U
#define TCONV_11BIT  375U
#define TCONV_12BIT  750U

typedef uint8_t ScratchPad[9];

/* ── Static state ────────────────────────────────────────────────────────── */
static const struct device *ds_gpio_dev;
static uint32_t conv_wait_ms = TCONV_12BIT;   /* tracks current resolution */

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal GPIO helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline void pin_output_low(void)
{
    gpio_pin_configure(ds_gpio_dev, DS_PIN, GPIO_OUTPUT_INACTIVE);
}

static inline void pin_output_high(void)
{
    gpio_pin_configure(ds_gpio_dev, DS_PIN, GPIO_OUTPUT_ACTIVE);
}

static inline void pin_input(void)
{
    /* External 4.7k pullup on DQ line — no internal pull needed */
    gpio_pin_configure(ds_gpio_dev, DS_PIN, GPIO_INPUT);
}

static inline int pin_read(void)
{
    return gpio_pin_get(ds_gpio_dev, DS_PIN);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API: init
 * ═══════════════════════════════════════════════════════════════════════════ */

int ds18b20_init(void)
{
    ds_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
    if (!device_is_ready(ds_gpio_dev)) {
        printk("DS18B20: GPIO device not ready\n");
        return -ENODEV;
    }
    /* Idle state: bus high (held by external 4.7k pullup) */
    pin_output_high();
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1-Wire primitives — timed against datasheet Figure 11, 12, 26
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Reset pulse + presence detect (Figure 11)
 *
 * Sequence:
 *   1. Pull bus low >= 480 µs  (tRSTL)
 *   2. Release, wait 15–60 µs  (tPDHIGH) for device to respond
 *   3. Sample — device pulls low 60–240 µs (tPDLOW) = presence
 *   4. Wait remainder of tRSTH (480 µs total release time)
 *
 * @return 1 = device present, 0 = no device
 */
int ds18b20_reset(void)
{
    int presence;

    /* Step 1: drive bus low for 500 µs (> 480 µs tRSTL minimum) */
    pin_output_low();
    k_busy_wait(500);

    /* Step 2: release bus, wait 70 µs (within 15–60 µs tPDHIGH window,
     * biased toward 60 µs end per Figure 14 recommendation) */
    pin_input();
    k_busy_wait(70);

    /* Step 3: sample — DS18B20 holds bus low during tPDLOW (60–240 µs)
     * so at 70 µs we are inside the presence window */
    presence = (pin_read() == 0) ? 1 : 0;

    /* Step 4: wait remainder to satisfy tRSTH >= 480 µs total.
     * 70 µs already elapsed + 410 µs = 480 µs minimum */
    k_busy_wait(410);

    return presence;
}

/**
 * @brief Write one bit (Figure 12, Figure 26)
 *
 * Write 1: pull low 1–15 µs (tLOW1), release, slot total >= 60 µs (tSLOT)
 * Write 0: pull low 60–120 µs (tLOW0), slot total >= 60 µs
 * Recovery: >= 1 µs (tREC) between slots
 */
static void onewire_write_bit(uint8_t bit)
{
    if (bit) {
        /* Write 1: short low pulse (<= 15 µs), then release for remainder */
        pin_output_low();
        k_busy_wait(6);         /* tLOW1: 1–15 µs, use 6 µs */
        pin_input();            /* release — pullup drives high */
        k_busy_wait(55);        /* complete 61 µs slot (>= 60 µs tSLOT) */
    } else {
        /* Write 0: hold low for full slot duration */
        pin_output_low();
        k_busy_wait(65);        /* tLOW0: 60–120 µs */
        pin_input();            /* release */
    }
    k_busy_wait(1);             /* tREC: >= 1 µs recovery between slots */
}

/**
 * @brief Read one bit (Figure 12, 13, 14)
 *
 * Master pulls low >= 1 µs, releases.
 * Device drives data valid within 15 µs of falling edge (tRDV).
 * Sample must occur before 15 µs from start of slot.
 * Full slot must be >= 60 µs.
 */
static uint8_t onewire_read_bit(void)
{
    uint8_t r;

    /* Initiate read slot: pull low >= 1 µs */
    pin_output_low();
    k_busy_wait(3);             /* tINIT: > 1 µs */

    /* Release and wait — total from falling edge to sample < 15 µs */
    pin_input();
    k_busy_wait(9);             /* tRC: ~9 µs; total = 3+9 = 12 µs < 15 µs */

    /* Sample the bit */
    r = (uint8_t)pin_read();

    /* Complete the 60 µs minimum slot: 3+9+sample+remainder = >= 60 µs */
    k_busy_wait(50);

    k_busy_wait(1);             /* tREC recovery */

    return r;
}

/**
 * @brief Write one byte, LSB first (datasheet: "All data is read/written LSB first")
 */
static void onewire_write_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        onewire_write_bit((data >> i) & 0x01);
    }
}

/**
 * @brief Read one byte, LSB first
 */
static uint8_t onewire_read_byte(void)
{
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (onewire_read_bit()) {
            data |= (0x01 << i);
        }
    }
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

void ds18b20_request_temperatures(void)
{
    ds18b20_reset();
    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_START_CONVO);
    /* Bus must remain idle (high via pullup) during conversion.
     * Caller must wait conv_wait_ms before reading. */
}

void ds18b20_read_scratchpad(uint8_t *scratch_pad, uint8_t fields)
{
    ds18b20_reset();
    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_READ_SCRATCH);

    for (uint8_t i = 0; i < fields; i++) {
        scratch_pad[i] = onewire_read_byte();
    }
    ds18b20_reset();
}

/**
 * @brief Set resolution and update internal conversion wait time.
 * Datasheet Note 3: TH, TL, and config must ALL be written before reset.
 */
void ds18b20_set_resolution(uint8_t resolution)
{
    uint8_t config_byte;

    switch (resolution) {
        case 12: config_byte = TEMP_12_BIT; conv_wait_ms = TCONV_12BIT; break;
        case 11: config_byte = TEMP_11_BIT; conv_wait_ms = TCONV_11BIT; break;
        case 10: config_byte = TEMP_10_BIT; conv_wait_ms = TCONV_10BIT; break;
        case 9:
        default: config_byte = TEMP_9_BIT;  conv_wait_ms = TCONV_9BIT;  break;
    }

    ds18b20_reset();
    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_WRITE_SCRATCH);
    /* Must write TH, TL, and config consecutively before issuing reset
     * (datasheet Table 4, Note 3) */
    onewire_write_byte(0);          /* TH alarm: 0°C (unused) */
    onewire_write_byte(100);        /* TL alarm: 100°C (unused) */
    onewire_write_byte(config_byte);
    /* Reset is now safe to issue */
    ds18b20_reset();
}

/**
 * @brief Blocking temperature read, method 1.
 * Correct two's complement int16_t decode per Table 2.
 */
float ds18b20_get_temp(void)
{
    if (!ds18b20_reset()) {
        return 0.0f;
    }

    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_START_CONVO);

    /* Wait for conversion (750 ms max for 12-bit, Table 3) */
    k_msleep(conv_wait_ms);

    if (!ds18b20_reset()) {
        return 0.0f;
    }

    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_READ_SCRATCH);

    uint8_t lsb = onewire_read_byte();
    uint8_t msb = onewire_read_byte();

    ds18b20_reset();

    /*
     * Combine into signed 16-bit two's complement value (Table 2).
     * Each LSB = 0.0625°C at 12-bit resolution.
     * e.g. +125°C = 0x07D0 = 2000 * 0.0625
     *      -55°C  = 0xFC90 = -880 * 0.0625
     */
    int16_t raw = (int16_t)(((uint16_t)msb << 8) | lsb);
    return (float)raw * 0.0625f;
}

/**
 * @brief Non-blocking trigger + scratchpad read, method 2.
 * Uses conv_wait_ms set by ds18b20_set_resolution().
 */
float ds18b20_get_temp_method2(void)
{
    ds18b20_request_temperatures();

    /* ── MUST wait here for conversion to complete ──────────────────────── 
     * 12-bit conversion = 750 ms max (datasheet Table 3).
     * Without this wait, scratchpad still holds the previous/reset value.
     * ───────────────────────────────────────────────────────────────────── */
    k_msleep(conv_wait_ms);     /* set by ds18b20_set_resolution() */

    ScratchPad scratch_pad;
    ds18b20_read_scratchpad(scratch_pad, 2);

    int16_t raw = (int16_t)(((uint16_t)scratch_pad[TEMP_MSB] << 8)
                             | scratch_pad[TEMP_LSB]);
    return (float)raw * 0.0625f;
}