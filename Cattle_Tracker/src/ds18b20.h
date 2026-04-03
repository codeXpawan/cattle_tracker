#ifndef DS18B20_H
#define DS18B20_H

#include <zephyr/kernel.h>
#include <stdint.h>

/**
 * @brief Initialize the DS18B20 driver.
 * Must be called before any other ds18b20 function.
 * @return 0 on success, negative errno on failure.
 */
int ds18b20_init(void);

/**
 * @brief Send reset pulse and check for device presence.
 * @return 1 if device present, 0 if not.
 */
int ds18b20_reset(void);

/**
 * @brief Request a temperature conversion (non-blocking start).
 */
void ds18b20_request_temperatures(void);

/**
 * @brief Read scratchpad bytes.
 * @param scratch_pad  Output buffer (at least `fields` bytes).
 * @param fields       Number of bytes to read (max 9).
 */
void ds18b20_read_scratchpad(uint8_t *scratch_pad, uint8_t fields);

/**
 * @brief Set ADC resolution (9–12 bits).
 * @param resolution  9, 10, 11, or 12.
 */
void ds18b20_set_resolution(uint8_t resolution);

/**
 * @brief Read temperature using method 1 (blocking, ~600 ms wait).
 * @return Temperature in °C, or 0.0f on error.
 */
float ds18b20_get_temp(void);

/**
 * @brief Read temperature using method 2 (scratchpad read).
 * Call ds18b20_request_temperatures() first and wait ≥750 ms.
 * @return Temperature in °C.
 */
float ds18b20_get_temp_method2(void);

#endif /* DS18B20_H */