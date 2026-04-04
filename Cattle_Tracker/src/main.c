#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include "ds18b20.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_DBG);

/* ── BLE Manufacturer payload ────────────────────────────────────────────
 *  0–1  : Company ID  0x05 0x05
 *  2–5  : Temperature IEEE 754 float little-endian
 *  Total: 6 bytes
 * ───────────────────────────────────────────────────────────────────────── */
#define PAYLOAD_LEN       6
#define OFFSET_COMPANY_ID 0
#define OFFSET_TEMP       2

static uint8_t manuf_payload[PAYLOAD_LEN] = {
    0x05, 0x05,
    0x00, 0x00, 0x00, 0x00,
};

static struct bt_data ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, manuf_payload, PAYLOAD_LEN),
};

static struct bt_le_ext_adv *adv_set;

/* ── Float packer ───────────────────────────────────────────────────────── */
static void pack_float_le(uint8_t *dst, float val)
{
    union { float f; uint8_t b[4]; } u;
    u.f    = val;
    dst[0] = u.b[0];
    dst[1] = u.b[1];
    dst[2] = u.b[2];
    dst[3] = u.b[3];
}

/* ── Update BLE payload ─────────────────────────────────────────────────── */
static int update_adv_payload(float temp_c)
{
    pack_float_le(&manuf_payload[OFFSET_TEMP], temp_c);

    int err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("ext_adv_set_data failed (%d)", err);
    }
    return err;
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    int err;
    float temp;

    /* ── DS18B20 ────────────────────────────────────────────────────────── */
    err = ds18b20_init();
    if (err < 0) {
        LOG_ERR("DS18B20 init failed (%d)", err);
        return err;
    }
    ds18b20_set_resolution(12);
    LOG_INF("DS18B20 ready");

    /* ── Bluetooth enable ───────────────────────────────────────────────── */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BT enable failed (%d)", err);
        return err;
    }
    LOG_INF("BT enabled");

    /* ── Create advertiser FIRST ────────────────────────────────────────── */
    err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, NULL, &adv_set);
    if (err) {
        LOG_ERR("ext_adv_create failed (%d)", err);
        return err;
    }
    LOG_INF("Ext adv set created (Primary 1M / Secondary 1M)");

    /* ── Read temperature AFTER adv_set exists ──────────────────────────── */
    temp = ds18b20_get_temp();
    LOG_INF("Initial temperature: %.4f C", (double)temp);

    /* ── Now safe to set payload ────────────────────────────────────────── */
    err = update_adv_payload(temp);
    if (err) {
        return err;
    }

    /* ── Start advertising ──────────────────────────────────────────────── */
    err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        LOG_ERR("ext_adv_start failed (%d)", err);
        return err;
    }
    LOG_INF("Advertising started");

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1) {
        k_msleep(3000);
        temp = ds18b20_get_temp();
        LOG_INF("Temperature: %.4f C", (double)temp);
        update_adv_payload(temp);
    }

    return 0;
}