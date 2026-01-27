/**
 * @file zh_inclinometer.h
 */

#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Inclinometer initial default values.
 */
#define ZH_INCLINOMETER_INIT_CONFIG_DEFAULT() \
    {                                         \
        .a_gpio_number = GPIO_NUM_MAX,        \
        .b_gpio_number = GPIO_NUM_MAX,        \
        .pullup = true,                       \
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
        bool pullup;             /*!< Pullup GPIO enable/disable. */
        uint16_t encoder_pulses; /*!< Number of pulses per one rotation. */
    } zh_inclinometer_init_config_t;

    /**
     * @brief Inclinometer handle.
     */
    typedef struct
    {
        pcnt_unit_handle_t pcnt_unit_handle;         /*!< Inclinometer unique pcnt unit handle. */
        pcnt_channel_handle_t pcnt_channel_a_handle; /*!< Inclinometer unique pcnt channel handle. */
        pcnt_channel_handle_t pcnt_channel_b_handle; /*!< Inclinometer unique pcnt channel handle. */
        float degrees_per_pulse;                     /*!< Number of degrees per pulse. */
        bool is_initialized;                         /*!< Inclinometer initialization flag. */
    } zh_inclinometer_handle_t;

    /**
     * @brief Initialize inclinometer.
     *
     * @param[in] config Pointer to inclinometer initialized configuration structure. Can point to a temporary variable.
     * @param[out] handle Pointer to unique inclinometer handle.
     *
     * @note Before initialize the inclinometer recommend initialize zh_inclinometer_init_config_t structure with default values.
     *
     * @code zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT() @endcode
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle);

    /**
     * @brief Deinitialize inclinometer.
     *
     * @param[in, out] handle Pointer to unique inclinometer handle.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_deinit(zh_inclinometer_handle_t *handle);

    /**
     * @brief Get inclinometer position.
     *
     * @param[in] handle Pointer to unique inclinometer handle.
     * @param[out] position Inclinometer position.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_get(zh_inclinometer_handle_t *handle, float *angle);

    /**
     * @brief Reset inclinometer position.
     *
     * @note The inclinometer will be set to 0 position.
     *
     * @param[in] handle Pointer to unique inclinometer handle.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_inclinometer_reset(zh_inclinometer_handle_t *handle);

#ifdef __cplusplus
}
#endif