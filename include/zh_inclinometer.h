/**
 * @file zh_inclinometer.h
 *
 * @brief Header file for the zh_inclinometer library providing encoder-based
 *        angle measurement and position tracking functionality.
 *
 * This library implements an inclinometer interface based on a quadrature
 * encoder connected to two GPIO pins. It uses the ESP-IDF pulse counter
 * (pcnt) driver to count encoder pulses and calculate the current angular
 * position. The library supports configurable GPIO pins, pull-up enablement,
 * rotation direction, and pulses per revolution.
 *
 * Key features:
 * - Quadrature encoder support with configurable GPIO pins
 * - Configurable pull-up resistor enablement
 * - Support for both clockwise and counter-clockwise rotation directions
 * - Configurable pulses per revolution for accurate angle calculation
 * - Position tracking with get and reset operations
 */

#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ZH_INCLINOMETER_INIT_CONFIG_DEFAULT() \
    {                                         \
        .a_gpio_number = GPIO_NUM_MAX,        \
        .b_gpio_number = GPIO_NUM_MAX,        \
        .pullup = true,                       \
        .rotation = true,                     \
        .encoder_pulses = 0}

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Opaque handle to the inclinometer internal state.
     *
     * This handle is returned by zh_inclinometer_init() and must be passed
     * to all subsequent API functions to operate on the same inclinometer
     * instance. The handle is freed by zh_inclinometer_deinit().
     */
    typedef struct _zh_inclinometer_handle_t zh_inclinometer_handle_t;

    /**
     * @brief Structure for initial configuration of the inclinometer.
     *
     * This structure holds all parameters required to initialize the
     * inclinometer, including GPIO pin assignments, electrical configuration,
     * rotation direction, and encoder resolution.
     *
     * @note Both GPIO pins must be assigned before initialization.
     *       Use GPIO_NUM_MAX to indicate an unassigned pin.
     * @warning The encoder_pulses field must be set to a non-zero value
     *          representing the actual number of pulses per one full rotation
     *          of the encoder shaft.
     */
    typedef struct
    {
        uint8_t a_gpio_number;   /*!< Encoder A channel GPIO number */
        uint8_t b_gpio_number;   /*!< Encoder B channel GPIO number */
        bool pullup;             /*!< GPIO pull-up resistor enable (true) or disable (false) */
        bool rotation;           /*!< Rotation direction: true - positive for CW, false - positive for CCW */
        uint16_t encoder_pulses; /*!< Number of encoder pulses per one full rotation */
    } zh_inclinometer_init_config_t;

    /**
     * @brief Initialize the inclinometer.
     *
     * Allocates memory for the inclinometer handle, validates the configuration,
     * initializes the PCNT unit with two channels for quadrature decoding,
     * and starts the pulse counter.
     *
     * @param[in] config Pointer to the inclinometer initialization configuration
     *                   structure. Can point to a temporary variable (must not be NULL)
     * @param[out] handle Pointer to the unique inclinometer handle (must be NULL)
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if config or handle is NULL
     * @return ESP_ERR_INVALID_STATE if the inclinometer is already initialized
     * @return ESP_ERR_NO_MEM if memory allocation failed
     * @return ESP_FAIL if PCNT initialization or configuration failed
     */
    esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t **handle);

    /**
     * @brief Deinitialize the inclinometer.
     *
     * Stops and disables the PCNT unit, deletes both encoder channels and the
     * PCNT unit, frees the inclinometer handle memory, and sets the handle to
     * NULL.
     *
     * @param[in, out] handle Pointer to the unique inclinometer handle (must not be NULL)
     *
     * @note After deinitialization, the handle is invalidated and must not be
     *       used. Call zh_inclinometer_init() again to reinitialize.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if handle or *handle is NULL
     * @return ESP_FAIL if PCNT stop, disable, or deletion failed
     */
    esp_err_t zh_inclinometer_deinit(zh_inclinometer_handle_t **handle);

    /**
     * @brief Get the current inclinometer position.
     *
     * Reads the PCNT counter value and converts it to an angle in degrees
     * using the configured degrees-per-pulse factor. The sign of the angle
     * depends on the configured rotation direction.
     *
     * @param[in] handle Pointer to the unique inclinometer handle (must not be NULL)
     * @param[out] angle Pointer to a float variable that receives the current
     *                   angle in degrees (must not be NULL)
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if handle, *handle, or angle is NULL
     * @return ESP_FAIL if reading the PCNT counter failed
     */
    esp_err_t zh_inclinometer_get(zh_inclinometer_handle_t **handle, float *angle);

    /**
     * @brief Reset the inclinometer position to zero.
     *
     * Clears the PCNT counter, setting the current position to 0 degrees.
     * The inclinometer continues counting from the new zero position.
     *
     * @param[in] handle Pointer to the unique inclinometer handle (must not be NULL)
     *
     * @note This does not affect the PCNT unit state (enabled/disabled).
     *       The unit remains running after reset.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if handle or *handle is NULL
     * @return ESP_FAIL if clearing the PCNT counter failed
     */
    esp_err_t zh_inclinometer_reset(zh_inclinometer_handle_t **handle);

#ifdef __cplusplus
}
#endif