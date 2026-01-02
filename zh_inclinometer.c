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

#define ZH_ENCODER_DIRECTION_CW 0x10
#define ZH_ENCODER_DIRECTION_CCW 0x20

static const uint8_t _encoder_matrix[7][4] = {
    {0x03, 0x02, 0x01, 0x00},
    {0x23, 0x00, 0x01, 0x00},
    {0x13, 0x02, 0x00, 0x00},
    {0x03, 0x05, 0x04, 0x00},
    {0x03, 0x03, 0x04, 0x00},
    {0x03, 0x05, 0x03, 0x00},
};

static portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

static double _encoder_step;
volatile static double _encoder_position;
volatile static uint8_t _encoder_state;
static uint8_t _a_gpio_number;
static uint8_t _b_gpio_number;
static bool _is_initialized = false;
static bool _is_prev_gpio_isr_handler = false;

static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config);
static esp_err_t _zh_inclinometer_gpio_init(const zh_inclinometer_init_config_t *config);
static void _zh_inclinometer_isr_handler(void *arg);

esp_err_t zh_inclinometer_init(const zh_inclinometer_init_config_t *config)
{
    ZH_LOGI("Inclinometer initialization started.");
    esp_err_t err = _zh_inclinometer_validate_config(config);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "Inclinometer initialization failed. Initial configuration check failed.");
    err = _zh_inclinometer_gpio_init(config);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "Inclinometer initialization failed. GPIO initialization failed.");
    _encoder_position = 0;
    _encoder_step = 360.0 / (double)config->encoder_pulses;
    _a_gpio_number = config->a_gpio_number;
    _b_gpio_number = config->b_gpio_number;
    _is_initialized = true;
    ZH_LOGI("Inclinometer initialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_deinit(void)
{
    ZH_LOGI("Inclinometer deinitialization started.");
    ZH_ERROR_CHECK(_is_initialized == true, ESP_FAIL, NULL, "Inclinometer deinitialization failed. Inclinometer not initialized.");
    gpio_isr_handler_remove((gpio_num_t)_a_gpio_number);
    gpio_isr_handler_remove((gpio_num_t)_b_gpio_number);
    gpio_reset_pin((gpio_num_t)_a_gpio_number);
    gpio_reset_pin((gpio_num_t)_b_gpio_number);
    if (_is_prev_gpio_isr_handler == false)
    {
        gpio_uninstall_isr_service();
    }
    _is_initialized = false;
    ZH_LOGI("Inclinometer deinitialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_get(double *position)
{
    ZH_LOGI("Inclinometer get position started.");
    ZH_ERROR_CHECK(position != NULL, ESP_ERR_INVALID_ARG, NULL, "Inclinometer get position failed. Invalid argument.");
    ZH_ERROR_CHECK(_is_initialized == true, ESP_FAIL, NULL, "Inclinometer get position failed. Inclinometer not initialized.");
    *position = _encoder_position;
    ZH_LOGI("Inclinometer get position completed successfully.");
    return ESP_OK;
}

esp_err_t zh_inclinometer_reset(void)
{
    ZH_LOGI("Inclinometer reset started.");
    ZH_ERROR_CHECK(_is_initialized == true, ESP_FAIL, NULL, "Inclinometer reset failed. Inclinometer not initialized.");
    taskENTER_CRITICAL(&_spinlock);
    _encoder_position = 0;
    taskEXIT_CRITICAL(&_spinlock);
    ZH_LOGI("Inclinometer reset completed successfully.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_validate_config(const zh_inclinometer_init_config_t *config)
{
    ZH_ERROR_CHECK(config != NULL, ESP_ERR_INVALID_ARG, NULL, "Invalid configuration.");
    ZH_ERROR_CHECK(config->encoder_pulses > 0, ESP_ERR_INVALID_ARG, NULL, "Invalid encoder pulses.");
    return ESP_OK;
}

static esp_err_t _zh_inclinometer_gpio_init(const zh_inclinometer_init_config_t *config) // -V2008
{
    ZH_ERROR_CHECK(config->a_gpio_number < GPIO_NUM_MAX && config->b_gpio_number < GPIO_NUM_MAX, ESP_ERR_INVALID_ARG, NULL, "Invalid GPIO number.")
    ZH_ERROR_CHECK(config->a_gpio_number != config->b_gpio_number, ESP_ERR_INVALID_ARG, NULL, "Encoder A and B GPIO is same.")
    gpio_config_t pin_config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << config->a_gpio_number) | (1ULL << config->b_gpio_number),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    esp_err_t err = gpio_config(&pin_config);
    ZH_ERROR_CHECK(err == ESP_OK, err, NULL, "GPIO initialization failed.");
    err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    ZH_ERROR_CHECK(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, gpio_reset_pin((gpio_num_t)config->a_gpio_number); gpio_reset_pin((gpio_num_t)config->b_gpio_number), "Failed install isr service.");
    if (err == ESP_ERR_INVALID_STATE)
    {
        _is_prev_gpio_isr_handler = true;
    }
    err = gpio_isr_handler_add((gpio_num_t)config->a_gpio_number, _zh_inclinometer_isr_handler, NULL);
    ZH_ERROR_CHECK(err == ESP_OK, err, gpio_reset_pin((gpio_num_t)config->a_gpio_number); gpio_reset_pin((gpio_num_t)config->b_gpio_number), "Interrupt initialization failed.");
    err = gpio_isr_handler_add((gpio_num_t)config->b_gpio_number, _zh_inclinometer_isr_handler, NULL);
    if (_is_prev_gpio_isr_handler == true)
    {
        ZH_ERROR_CHECK(err == ESP_OK, err, gpio_isr_handler_remove((gpio_num_t)config->a_gpio_number); gpio_reset_pin((gpio_num_t)config->a_gpio_number); gpio_reset_pin((gpio_num_t)config->b_gpio_number), "Interrupt initialization failed.");
    }
    else
    {
        ZH_ERROR_CHECK(err == ESP_OK, err, gpio_isr_handler_remove((gpio_num_t)config->a_gpio_number); gpio_uninstall_isr_service(); gpio_reset_pin((gpio_num_t)config->a_gpio_number); gpio_reset_pin((gpio_num_t)config->b_gpio_number), "Interrupt initialization failed.");
    }
    return ESP_OK;
}

static void IRAM_ATTR _zh_inclinometer_isr_handler(void *arg)
{
    _encoder_state = _encoder_matrix[_encoder_state & 0x0F][(gpio_get_level((gpio_num_t)_b_gpio_number) << 1) | gpio_get_level((gpio_num_t)_a_gpio_number)];
    switch (_encoder_state & 0x30)
    {
    case ZH_ENCODER_DIRECTION_CW:
        _encoder_position = _encoder_position + _encoder_step;
        break;
    case ZH_ENCODER_DIRECTION_CCW:
        _encoder_position = _encoder_position - _encoder_step;
        break;
    default:
        break;
    }
}