/**
 * @file zh_inclinometer.h
 */

#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Inclinometer initial default values.
 */
#define ZH_INCLINOMETER_INIT_CONFIG_DEFAULT() \
    {                                         \
        .a_gpio_number = GPIO_NUM_MAX,        \
        .b_gpio_number = GPIO_NUM_MAX,        \
        .encoder_pulses = 0}

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Structure for initial initialization of inclinometer.
     */
    typedef struct
    {
        uint8_t a_gpio_number;   /*!< Encoder A GPIO number. */
        uint8_t b_gpio_number;   /*!< Encoder B GPIO number. */
        uint16_t encoder_pulses; /*!< Number of pulses per one rotation. */
    } zh_inclinometer_init_config_t;

    /**
     * @brief Initialize inclinometer.
     *
     * @param[in] config Pointer to inclinometer initialized configuration structure. Can point to a temporary variable.
     *
     * @note Before initialize the inclinometer recommend initialize zh_inclinometer_init_config_t structure with default values.
     *
     * @code zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT() @endcode
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config);

    /**
     * @brief Deinitialize inclinometer.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_deinit(void);

    /**
     * @brief Get inclinometer position.
     *
     * @param[out] position inclinometer position.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_get(double *position);

    /**
     * @brief Reset inclinometer position.
     *
     * @note The inclinometer will be set to 0 position.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_reset(void);

#ifdef __cplusplus
}
#endif