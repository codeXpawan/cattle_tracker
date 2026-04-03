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

    /* ── Bluetooth ──────────────────────────────────────────────────────── */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BT enable failed (%d)", err);
        return err;
    }
    LOG_INF("BT enabled");

    /* ── Extended advertiser: Primary 1M, Secondary 1M ──────────────────── 
     * BT_LE_ADV_OPT_EXT_ADV alone picks 2M secondary on nRF52840.
     * BT_LE_ADV_OPT_NO_2M forces secondary PHY to 1M as well.
     * ───────────────────────────────────────────────────────────────────── */
    struct bt_le_adv_param adv_param = {
        .id                 = BT_ID_DEFAULT,
        .sid                = 0,
        .secondary_max_skip = 0,
        .options            = BT_LE_ADV_OPT_EXT_ADV |
                              BT_LE_ADV_OPT_CODED,   
        .interval_min       = BT_GAP_ADV_SLOW_INT_MIN,
        .interval_max       = BT_GAP_ADV_SLOW_INT_MAX,
        .peer               = NULL,
    };

    err = bt_le_ext_adv_create(&adv_param, NULL, &adv_set);
    if (err) {
        LOG_ERR("ext_adv_create failed (%d)", err);
        return err;
    }
    LOG_INF("Ext adv set created (Primary 1M / Secondary 1M)");

    /* ── First temperature read ─────────────────────────────────────────── 
     * Must do a full blocking read BEFORE starting advertising so the
     * initial payload is never 0x00000000.
     * ───────────────────────────────────────────────────────────────────── */
    temp = ds18b20_get_temp();   /* blocking: requests + waits 750ms + reads */
    LOG_INF("Initial temperature: %.4f C", (double)temp);

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
    LOG_INF("Advertising — Primary PHY: 1M  Secondary PHY: 1M");

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1) {
        k_msleep(3000);

        /*
         * Use get_temp() (blocking method 1) rather than method2() here.
         * method2() calls request_temperatures() then returns immediately —
         * the 750ms wait must happen before the scratchpad read or you
         * get stale/zero data. get_temp() handles this internally.
         */
        temp = ds18b20_get_temp();
        LOG_INF("Temperature: %.4f C", (double)temp);

        update_adv_payload(temp);
    }

    return 0;
}