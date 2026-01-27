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

static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config);
static esp_err_t _zh_inclinometer_pcnt_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle);

esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle)
{
    ZH_LOGI("Inclinometer initialization started.");
    ZH_ERROR_CHECK(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer initialization failed. Invalid argument.");
    esp_err_t err = _zh_inclinometer_validate_config(config);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "Inclinometer initialization failed. Initial configuration check failed.");
    err = _zh_inclinometer_pcnt_init(config, handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "Inclinometer initialization failed. PCNT initialization failed.");
    handle->degrees_per_pulse = 360.0 / config->encoder_pulses;
    handle->is_initialized = true;
    ZH_LOGI("Inclinometer initialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_deinit(zh_inclinometer_handle_t *handle)
{
    ZH_LOGI("Inclinometer deinitialization started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer deinitialization failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Inclinometer deinitialization failed. Inclinometer not initialized.");
    pcnt_unit_stop(handle->pcnt_unit_handle);
    pcnt_unit_disable(handle->pcnt_unit_handle);
    pcnt_del_channel(handle->pcnt_channel_a_handle);
    pcnt_del_channel(handle->pcnt_channel_b_handle);
    pcnt_del_unit(handle->pcnt_unit_handle);
    handle->is_initialized = false;
    ZH_LOGI("Inclinometer deinitialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_get(zh_inclinometer_handle_t *handle, float *angle)
{
    ZH_LOGI("Inclinometer get position started.");
    ZH_ERROR_CHECK(handle != NULL && angle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer get position failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Inclinometer get position failed. Inclinometer not initialized.");
    int pcnt_count = 0;
    pcnt_unit_get_count(handle->pcnt_unit_handle, &pcnt_count);
    float angle_temp = pcnt_count * handle->degrees_per_pulse;
    *angle = (angle_temp < 0) ? -angle_temp : angle_temp;
    ZH_LOGI("Inclinometer get position completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_reset(zh_inclinometer_handle_t *handle)
{
    ZH_LOGI("Inclinometer reset started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer reset failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_FAIL, NULL, "Inclinometer reset failed. Inclinometer not initialized.");
    pcnt_unit_clear_count(handle->pcnt_unit_handle);
    ZH_LOGI("Inclinometer reset completed successfully.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config)
{
    ZH_ERROR_CHECK(config != NULL, ESP_ERR_INVALID_ARG, NULL, "Invalid configuration.");
    ZH_ERROR_CHECK(config->encoder_pulses > 0, ESP_ERR_INVALID_ARG, NULL, "Invalid encoder pulses.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_pcnt_init(const zh_inclinometer_init_config_t *config, zh_inclinometer_handle_t *handle)
{
    ZH_ERROR_CHECK(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Invalid argument.");
    ZH_ERROR_CHECK(config->a_gpio_number < GPIO_NUM_MAX && config->b_gpio_number < GPIO_NUM_MAX, ESP_ERR_INVALID_ARG, NULL, "Invalid GPIO number.")
    ZH_ERROR_CHECK(config->a_gpio_number != config->b_gpio_number, ESP_ERR_INVALID_ARG, NULL, "Encoder A and B GPIO is same.")
    pcnt_unit_config_t pcnt_unit_config = {
        .high_limit = config->encoder_pulses,
        .low_limit = -config->encoder_pulses,
    };
    pcnt_unit_handle_t pcnt_unit_handle = NULL;
    esp_err_t err = pcnt_new_unit(&pcnt_unit_config, &pcnt_unit_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "PCNT initialization failed.");
    pcnt_glitch_filter_config_t pcnt_glitch_filter_config = {
        .max_glitch_ns = 1000,
    };
    err = pcnt_unit_set_glitch_filter(pcnt_unit_handle, &pcnt_glitch_filter_config);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    pcnt_chan_config_t pcnt_chan_a_config = {
        .edge_gpio_num = config->a_gpio_number,
        .level_gpio_num = config->b_gpio_number,
    };
    pcnt_channel_handle_t pcnt_channel_a_handle = NULL;
    err = pcnt_new_channel(pcnt_unit_handle, &pcnt_chan_a_config, &pcnt_channel_a_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    pcnt_chan_config_t pcnt_chan_b_config = {
        .edge_gpio_num = config->b_gpio_number,
        .level_gpio_num = config->a_gpio_number,
    };
    pcnt_channel_handle_t pcnt_channel_b_handle = NULL;
    err = pcnt_new_channel(pcnt_unit_handle, &pcnt_chan_b_config, &pcnt_channel_b_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_channel_set_edge_action(pcnt_channel_a_handle, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle); pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_channel_set_level_action(pcnt_channel_a_handle, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle); pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_channel_set_edge_action(pcnt_channel_b_handle, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle); pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_channel_set_level_action(pcnt_channel_b_handle, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_HOLD);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle); pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_unit_enable(pcnt_unit_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle); pcnt_del_unit(pcnt_unit_handle),
                                                                                                                         "PCNT initialization failed.");
    err = pcnt_unit_clear_count(pcnt_unit_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_unit_disable(pcnt_unit_handle); pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle);
                   pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    err = pcnt_unit_start(pcnt_unit_handle);
    ZH_ERROR_CHECK(err == ESP_OK, err, pcnt_unit_disable(pcnt_unit_handle); pcnt_del_channel(pcnt_channel_a_handle); pcnt_del_channel(pcnt_channel_b_handle);
                   pcnt_del_unit(pcnt_unit_handle), "PCNT initialization failed.");
    if (config->pullup == false)
    {
        gpio_pullup_dis((gpio_num_t)config->a_gpio_number);
        gpio_pullup_dis((gpio_num_t)config->b_gpio_number);
    }
    handle->pcnt_unit_handle = pcnt_unit_handle;
    handle->pcnt_channel_a_handle = pcnt_channel_a_handle;
    handle->pcnt_channel_b_handle = pcnt_channel_b_handle;
    return ESP_OK;
}