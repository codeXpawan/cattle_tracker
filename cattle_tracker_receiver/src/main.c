/*
 * ble_temp_receiver.c — nRF52840 (NCS v3.2.1)
 * Scans for DS18B20 temperature beacons (1 Mbps PHY)
 * Maintains device table + forwards data to ESP32 via SPI slave
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdio.h>

/* ── Constants ──────────────────────────────────────────────────────────── */

/* Company ID we look for in manufacturer data (0x0505 little-endian) */
static const uint8_t COMPANY_ID[2] = { 0x05, 0x05 };

/* Manufacturer data AD type length: 2 (company ID) + 4 (float) = 6 bytes   */
#define MANUF_DATA_LEN      6
#define OFFSET_COMPANY_ID   0
#define OFFSET_TEMP         2   /* IEEE 754 float, little-endian             */

#define MAX_DEVICES         50
#define PRESENCE_TIMEOUT_S  30  /* remove device after 30 s of silence       */

/* ── SPI slave ──────────────────────────────────────────────────────────── */
#define INT_PIN     31          /* P0.31 → ESP32 GPIO4, HIGH = data ready    */
#define SPI_PKT_SIZE 64

static const struct device *spis_dev;
static const struct device *gpio_dev;

static const struct spi_config spis_cfg = {
    .operation = SPI_OP_MODE_SLAVE |
                 SPI_WORD_SET(8)   |
                 SPI_TRANSFER_MSB,
    .frequency = 0,
    .slave     = 0,
};

/* ── SPI packet ─────────────────────────────────────────────────────────── */
/*
 * Layout (64 bytes, packed):
 *   type     1 B   PKT_NEW / PKT_UPDATE / PKT_GONE
 *   slot     1 B   device table index
 *   mac_str 28 B   "XX:XX:XX:XX:XX:XX (random)" null-terminated
 *   temp    4  B   IEEE 754 float, little-endian
 *   rssi    1  B   signed dBm
 *   pad     29 B   zeroed
 */
#define PKT_NEW     0x01
#define PKT_UPDATE  0x02
#define PKT_GONE    0x03

struct spi_packet {
    uint8_t  type;
    uint8_t  slot;
    uint8_t  mac_str[28];
    uint8_t  temp_bytes[4];     /* unpack with memcpy → float on receiver   */
    int8_t   rssi;
    uint8_t  pad[29];
} __packed;

BUILD_ASSERT(sizeof(struct spi_packet) == SPI_PKT_SIZE,
             "spi_packet must be 64 bytes");

/* ── SPI TX infrastructure (mirrors your example) ───────────────────────── */
static struct spi_packet tx_buf[2];
static uint8_t           rx_buf[SPI_PKT_SIZE];
static uint8_t           active_buf = 0;
static K_MUTEX_DEFINE(buf_mutex);
static K_SEM_DEFINE(spis_sem, 0, 1);
static volatile int spis_result;

K_MSGQ_DEFINE(spi_msgq, sizeof(struct spi_packet), 64, 4);

#define SPI_TX_STACK 2048
K_THREAD_STACK_DEFINE(spi_tx_stack, SPI_TX_STACK);
static struct k_thread spi_tx_thread_data;

static void spis_cb(const struct device *dev, int result, void *userdata)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(userdata);
    spis_result = result;
    gpio_pin_set(gpio_dev, INT_PIN, 0);   /* deassert INT */
    k_sem_give(&spis_sem);
}

static void spi_tx_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    struct spi_packet pkt;

    while (1) {
        k_msgq_get(&spi_msgq, &pkt, K_FOREVER);

        k_mutex_lock(&buf_mutex, K_FOREVER);
        memcpy(&tx_buf[active_buf], &pkt, sizeof(pkt));
        k_mutex_unlock(&buf_mutex);

        struct spi_buf tx_spi_buf = {
            .buf = &tx_buf[active_buf],
            .len = SPI_PKT_SIZE
        };
        struct spi_buf rx_spi_buf = {
            .buf = rx_buf,
            .len = SPI_PKT_SIZE
        };
        struct spi_buf_set tx_set = { .buffers = &tx_spi_buf, .count = 1 };
        struct spi_buf_set rx_set = { .buffers = &rx_spi_buf, .count = 1 };

        int ret = spi_transceive_cb(spis_dev, &spis_cfg,
                                    &tx_set, &rx_set,
                                    spis_cb, NULL);
        if (ret < 0) {
            printk("spi_transceive_cb failed (%d)\n", ret);
            k_sleep(K_MSEC(100));
            continue;
        }

        /* Arm DMA first, THEN assert INT so ESP32 sees stable MISO */
        gpio_pin_set(gpio_dev, INT_PIN, 1);
        printk("INT asserted slot=%u type=0x%02x\n", pkt.slot, pkt.type);

        int sem_ret = k_sem_take(&spis_sem, K_SECONDS(5));

        if (sem_ret == -EAGAIN) {
            gpio_pin_set(gpio_dev, INT_PIN, 0);
            printk("WARN: SPI timeout slot=%u — requeueing\n", pkt.slot);
            k_msgq_put(&spi_msgq, &pkt, K_NO_WAIT);
        } else if (spis_result < 0) {
            printk("SPIS error: %d slot=%u — requeueing\n",
                   spis_result, pkt.slot);
            k_msgq_put(&spi_msgq, &pkt, K_NO_WAIT);
            k_sleep(K_MSEC(50));
        } else {
            printk("SPI sent type=0x%02x slot=%u\n", pkt.type, pkt.slot);
            active_buf ^= 1;
        }

        k_sleep(K_MSEC(10));
    }
}

static int spi_transport_init(void)
{
    spis_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(spis_dev)) {
        printk("SPIS device not ready\n");
        return -ENODEV;
    }

    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio_dev)) {
        printk("GPIO device not ready\n");
        return -ENODEV;
    }

    int ret = gpio_pin_configure(gpio_dev, INT_PIN, GPIO_OUTPUT_INACTIVE);
    if (ret) {
        printk("GPIO configure failed: %d\n", ret);
        return ret;
    }

    printk("SPI slave transport ready\n");
    return 0;
}

/* ── SPI enqueue helpers ─────────────────────────────────────────────────── */

static void pack_float_le(uint8_t *dst, float val)
{
    union { float f; uint8_t b[4]; } u;
    u.f    = val;
    dst[0] = u.b[0];
    dst[1] = u.b[1];
    dst[2] = u.b[2];
    dst[3] = u.b[3];
}

static void spi_enqueue(uint8_t type, uint8_t slot,
                        const char *mac, float temp, int8_t rssi)
{
    struct spi_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.type = type;
    pkt.slot = slot;
    pkt.rssi = rssi;
    pack_float_le(pkt.temp_bytes, temp);

    if (mac) {
        strncpy((char *)pkt.mac_str, mac, sizeof(pkt.mac_str) - 1);
    }

    if (k_msgq_put(&spi_msgq, &pkt, K_NO_WAIT) != 0) {
        printk("WARN: SPI queue full, dropping slot=%u\n", slot);
    }
}

/* ── Device table ────────────────────────────────────────────────────────── */

struct device_entry {
    bool                    active;
    bt_addr_le_t            addr;
    uint8_t                 slot;
    float                   last_temp;
    int8_t                  last_rssi;
    struct k_work_delayable timeout_work;
};

static struct device_entry devices[MAX_DEVICES];
static K_MUTEX_DEFINE(dev_mutex);

static void device_timeout_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct device_entry *e =
        CONTAINER_OF(dwork, struct device_entry, timeout_work);

    k_mutex_lock(&dev_mutex, K_FOREVER);
    if (e->active) {
        e->active = false;

        char mac_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&e->addr, mac_str, sizeof(mac_str));

        printk("GONE  slot=%02u mac=%s\n", e->slot, mac_str);
        spi_enqueue(PKT_GONE, e->slot, mac_str, 0.0f, 0);
    }
    k_mutex_unlock(&dev_mutex);
}

static void table_init(void)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        devices[i].active = false;
        devices[i].slot   = (uint8_t)i;
        k_work_init_delayable(&devices[i].timeout_work,
                              device_timeout_handler);
    }
}

/*
 * Returns existing entry for this address, or allocates a free slot.
 * Must be called with dev_mutex held.
 */
static struct device_entry *find_or_alloc(const bt_addr_le_t *addr)
{
    struct device_entry *free_slot = NULL;

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active) {
            if (bt_addr_le_eq(&devices[i].addr, addr)) {
                return &devices[i];     /* known device */
            }
        } else if (!free_slot) {
            free_slot = &devices[i];    /* remember first free slot */
        }
    }
    return free_slot;                   /* new device or NULL if table full  */
}

/* ── BLE scan callback ───────────────────────────────────────────────────── */

/*
 * Parse manufacturer data from the raw AD buffer.
 * Returns true and writes temperature to *temp_out if our payload is found.
 *
 * AD structure in raw buffer:
 *   [length][type=0xFF][company_lo][company_hi][temp 4 bytes] ...
 */
static bool parse_temperature(struct net_buf_simple *ad_buf, float *temp_out)
{
    struct net_buf_simple_state state;
    net_buf_simple_save(ad_buf, &state);

    while (ad_buf->len > 1) {
        uint8_t len  = net_buf_simple_pull_u8(ad_buf);
        if (len == 0 || len > ad_buf->len) break;

        uint8_t type = net_buf_simple_pull_u8(ad_buf);
        uint8_t data_len = len - 1;

        if (type == BT_DATA_MANUFACTURER_DATA &&
            data_len >= MANUF_DATA_LEN)
        {
            const uint8_t *d = ad_buf->data;

            /* Check company ID matches 0x0505 */
            if (d[OFFSET_COMPANY_ID]     == COMPANY_ID[0] &&
                d[OFFSET_COMPANY_ID + 1] == COMPANY_ID[1])
            {
                /* Unpack IEEE 754 float from bytes 2–5 */
                union { float f; uint8_t b[4]; } u;
                u.b[0] = d[OFFSET_TEMP];
                u.b[1] = d[OFFSET_TEMP + 1];
                u.b[2] = d[OFFSET_TEMP + 2];
                u.b[3] = d[OFFSET_TEMP + 3];
                *temp_out = u.f;

                net_buf_simple_restore(ad_buf, &state);
                return true;
            }
        }

        /* Advance past this AD structure's data */
        if (data_len > 0) {
            net_buf_simple_pull_mem(ad_buf, data_len);
        }
    }

    net_buf_simple_restore(ad_buf, &state);
    return false;
}

static void device_found(const bt_addr_le_t *addr,
                          int8_t rssi, uint8_t type,
                          struct net_buf_simple *ad)
{
    float temp;
    if (!parse_temperature(ad, &temp)) {
        return;     /* not our beacon */
    }

    char mac_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, mac_str, sizeof(mac_str));

    k_mutex_lock(&dev_mutex, K_FOREVER);

    struct device_entry *e = find_or_alloc(addr);
    if (!e) {
        k_mutex_unlock(&dev_mutex);
        printk("WARN: device table full\n");
        return;
    }

    if (!e->active) {
        /* ── New device ─────────────────────────────────────────────────── */
        e->active    = true;
        e->last_temp = temp;
        e->last_rssi = rssi;
        bt_addr_le_copy(&e->addr, addr);

        printk("NEW   slot=%02u mac=%-28s temp=%.4f C  rssi=%d\n",
               e->slot, mac_str, (double)temp, (int)rssi);

        spi_enqueue(PKT_NEW, e->slot, mac_str, temp, rssi);
    } else {
        /* ── Known device — temperature update ──────────────────────────── */
        e->last_temp = temp;
        e->last_rssi = rssi;

        printk("UPDATE slot=%02u mac=%-28s temp=%.4f C  rssi=%d\n",
               e->slot, mac_str, (double)temp, (int)rssi);

        spi_enqueue(PKT_UPDATE, e->slot, mac_str, temp, rssi);
    }

    /* Refresh presence timeout */
    k_work_reschedule(&e->timeout_work, K_SECONDS(PRESENCE_TIMEOUT_S));

    k_mutex_unlock(&dev_mutex);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    int err;

    /* Device table */
    table_init();

    /* SPI slave transport */
    err = spi_transport_init();
    if (err) {
        printk("SPI transport init failed: %d\n", err);
        return 1;
    }

    /* SPI TX thread */
    k_thread_create(&spi_tx_thread_data, spi_tx_stack,
                    K_THREAD_STACK_SIZEOF(spi_tx_stack),
                    spi_tx_thread, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&spi_tx_thread_data, "spi_tx");

    /* Bluetooth */
    err = bt_enable(NULL);
    if (err) {
        printk("BT enable failed (%d)\n", err);
        return 1;
    }
    printk("BT enabled\n");

    /*
     * 1 Mbps passive scan — no duplicate filter so we get every
     * advertisement and can forward temperature updates in real time.
     * interval == window → 100% scan duty cycle.
     */
    struct bt_le_scan_param scan_param = {
        .type     = BT_LE_SCAN_TYPE_PASSIVE,
        .options  = BT_LE_SCAN_OPT_CODED
             | BT_LE_SCAN_OPT_FILTER_DUPLICATE,       /* no duplicate filtering */
        .interval = 160,    /* 100 ms in 0.625 ms units */
        .window   = 160,    /* = interval → 100% duty   */
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Scan start failed (%d)\n", err);
        return 1;
    }
    printk("Scanning on 1 Mbps PHY — waiting for temperature beacons\n");

    while (1) {
        k_sleep(K_SECONDS(10));
    }

    return 0;
}