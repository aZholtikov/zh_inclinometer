#include "zh_inclinometer.h"

#define TAG "zh_inclinometer"

#define ZH_LOGI(msg, ...) ESP_LOGI(TAG, msg, ##__VA_ARGS__)
#define ZH_LOGE(msg, err, ...) ESP_LOGE(TAG, "[%s:%d:%s] " msg, __FILE__, __LINE__, esp_err_to_name(err), ##__VA_ARGS__)

#define ZH_ERROR_CHECK(cond, err, cleanup, msg, ...) \
    if (!(cond))                                     \
    {                                                \
        ZH_LOGE(msg, err, ##__VA_ARGS__);            \
        cleanup;                                     \
        return err;                                  \
    }

/**
 * @brief Internal structure holding the inclinometer instance state.
 *
 * This structure contains all resources and configuration needed to operate
 * a single inclinometer instance, including PCNT unit and channel handles,
 * the calculated angle per pulse, and the configured rotation direction.
 */
struct _zh_inclinometer_handle_t
{
    pcnt_unit_handle_t pcnt_unit_handle;         /*!< PCNT unit handle for the encoder */
    pcnt_channel_handle_t pcnt_channel_a_handle; /*!< PCNT channel A handle (encoder A pin) */
    pcnt_channel_handle_t pcnt_channel_b_handle; /*!< PCNT channel B handle (encoder B pin) */
    float degrees_per_pulse;                     /*!< Degrees per single encoder pulse */
    bool rotation;                               /*!< Rotation direction flag (true - CW positive) */
};

/**
 * @brief Validate the inclinometer initialization configuration.
 *
 * Checks that all required configuration parameters are within valid ranges.
 *
 * @param config Pointer to the initialization configuration structure.
 *
 * @return ESP_OK if the configuration is valid
 * @return ESP_ERR_INVALID_ARG if encoder_pulses is zero or negative
 */
static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config);

/**
 * @brief Initialize PCNT unit and channels for the inclinometer.
 *
 * Configures the pulse counter unit with two channels for quadrature
 * decoding, sets up glitch filtering, and starts the counter.
 *
 * @param config Pointer to the initialization configuration structure
 * @param handle Pointer to the inclinometer handle to store PCNT resources
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if GPIO pins are invalid or identical
 * @return ESP_FAIL if PCNT resource allocation or configuration fails
 */
static esp_err_t _zh_inclinometer_pcnt_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle);

esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t **handle) // -V2008
{
    ZH_LOGI("Inclinometer initialization started.");
    ZH_ERROR_CHECK(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer initialization failed. Invalid argument.");
    ZH_ERROR_CHECK(*handle == NULL, ESP_ERR_INVALID_STATE, NULL, "Inclinometer initialization failed. Inclinometer is already initialized.");
    ZH_ERROR_CHECK(_zh_inclinometer_validate_config(config) == ESP_OK, ESP_FAIL, NULL, "Inclinometer initialization failed. Initial configuration check failed.");
    *handle = heap_caps_calloc(1, sizeof(zh_inclinometer_handle_t), MALLOC_CAP_8BIT);
    ZH_ERROR_CHECK(*handle != NULL, ESP_ERR_NO_MEM, NULL, "Inclinometer initialization failed. Failed to allocate inclinometer handle.");
    ZH_ERROR_CHECK(_zh_inclinometer_pcnt_init(config, *handle) == ESP_OK, ESP_FAIL, heap_caps_free(*handle); *handle = NULL, "Inclinometer initialization failed. PCNT initialization failed.");
    (*handle)->degrees_per_pulse = 360.0 / config->encoder_pulses;
    (*handle)->rotation = config->rotation;
    ZH_LOGI("Inclinometer initialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_deinit(zh_inclinometer_handle_t **handle) // -V2008
{
    ZH_LOGI("Inclinometer deinitialization started.");
    ZH_ERROR_CHECK(handle != NULL && *handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer deinitialization failed. Invalid argument.");
    ZH_ERROR_CHECK(pcnt_unit_stop((*handle)->pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer deinitialization failed. PCNT unit stop fail.");
    ZH_ERROR_CHECK(pcnt_unit_disable((*handle)->pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer deinitialization failed. PCNT unit disable fail.");
    ZH_ERROR_CHECK(pcnt_del_channel((*handle)->pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer deinitialization failed. PCNT delete channel fail.");
    ZH_ERROR_CHECK(pcnt_del_channel((*handle)->pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer deinitialization failed. PCNT delete channel fail.");
    ZH_ERROR_CHECK(pcnt_del_unit((*handle)->pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer deinitialization failed. PCNT delete unit fail.");
    heap_caps_free(*handle);
    *handle = NULL;
    ZH_LOGI("Inclinometer deinitialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_get(zh_inclinometer_handle_t **handle, float *angle)
{
    ZH_LOGI("Inclinometer get position started.");
    ZH_ERROR_CHECK(handle != NULL && *handle != NULL && angle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer get position failed. Invalid argument.");
    int pcnt_count = 0;
    ZH_ERROR_CHECK(pcnt_unit_get_count((*handle)->pcnt_unit_handle, &pcnt_count) == ESP_OK, ESP_FAIL, NULL, "Inclinometer get position failed. PCNT unit get count fail.");
    float angle_temp = pcnt_count * (*handle)->degrees_per_pulse;
    *angle = ((*handle)->rotation == true) ? angle_temp : ((angle_temp == 0) ? angle_temp : -angle_temp); // -V550
    ZH_LOGI("Inclinometer get position completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_reset(zh_inclinometer_handle_t **handle)
{
    ZH_LOGI("Inclinometer reset started.");
    ZH_ERROR_CHECK(handle != NULL && *handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer reset failed. Invalid argument.");
    ZH_ERROR_CHECK(pcnt_unit_clear_count((*handle)->pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "Inclinometer reset failed. PCNT unit clear count fail.");
    ZH_LOGI("Inclinometer reset completed successfully.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config)
{
    ZH_ERROR_CHECK(config->encoder_pulses > 0, ESP_ERR_INVALID_ARG, NULL, "Invalid encoder pulses.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_pcnt_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle) // -V2008
{
    ZH_ERROR_CHECK(config->a_gpio_number < GPIO_NUM_MAX && config->b_gpio_number < GPIO_NUM_MAX, ESP_ERR_INVALID_ARG, NULL, "Invalid GPIO number.")
    ZH_ERROR_CHECK(config->a_gpio_number != config->b_gpio_number, ESP_ERR_INVALID_ARG, NULL, "Encoder A and B GPIO is same.")
    pcnt_unit_config_t pcnt_unit_config = {
        .high_limit = config->encoder_pulses,
        .low_limit = -config->encoder_pulses,
    };
    pcnt_unit_handle_t pcnt_unit_handle = NULL;
    ZH_ERROR_CHECK(pcnt_new_unit(&pcnt_unit_config, &pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT initialization failed.");
    pcnt_glitch_filter_config_t pcnt_glitch_filter_config = {
        .max_glitch_ns = 1000,
    };
    ZH_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_handle, &pcnt_glitch_filter_config) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail."), "PCNT initialization failed.");
    pcnt_chan_config_t pcnt_chan_a_config = {
        .edge_gpio_num = config->a_gpio_number,
        .level_gpio_num = config->b_gpio_number,
    };
    pcnt_channel_handle_t pcnt_channel_a_handle = NULL;
    ZH_ERROR_CHECK(pcnt_new_channel(pcnt_unit_handle, &pcnt_chan_a_config, &pcnt_channel_a_handle) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail."), "PCNT initialization failed.");
    pcnt_chan_config_t pcnt_chan_b_config = {
        .edge_gpio_num = config->b_gpio_number,
        .level_gpio_num = config->a_gpio_number,
    };
    pcnt_channel_handle_t pcnt_channel_b_handle = NULL;
    ZH_ERROR_CHECK(pcnt_new_channel(pcnt_unit_handle, &pcnt_chan_b_config, &pcnt_channel_b_handle) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_channel_a_handle, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel_a_handle, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_channel_b_handle, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel_b_handle, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_handle) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_handle) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_unit_disable(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT unit disable fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    ZH_ERROR_CHECK(pcnt_unit_start(pcnt_unit_handle) == ESP_OK, ESP_FAIL,
                   {ZH_ERROR_CHECK(pcnt_unit_disable(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT unit disable fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_a_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_channel(pcnt_channel_b_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete channel fail.")};
                   {ZH_ERROR_CHECK(pcnt_del_unit(pcnt_unit_handle) == ESP_OK, ESP_FAIL, NULL, "PCNT delete unit fail.")}, "PCNT initialization failed.");
    if (config->pullup == false)
    {
        ZH_ERROR_CHECK(gpio_pullup_dis((gpio_num_t)config->a_gpio_number) == ESP_OK, ESP_FAIL, NULL, "GPIO pullup disable fail.");
        ZH_ERROR_CHECK(gpio_pullup_dis((gpio_num_t)config->b_gpio_number) == ESP_OK, ESP_FAIL, NULL, "GPIO pullup disable fail.");
    }
    handle->pcnt_unit_handle = pcnt_unit_handle;
    handle->pcnt_channel_a_handle = pcnt_channel_a_handle;
    handle->pcnt_channel_b_handle = pcnt_channel_b_handle;
    return ESP_OK;
}