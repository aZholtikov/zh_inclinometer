# ESP32 ESP-IDF component for inclinometer (via rotary encoder)

## Tested on

1. [ESP32 ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Attention

1. For correct operation, please enable the following settings in the menuconfig:

```text
GPIO_CTRL_FUNC_IN_IRAM
```

## Using

In an existing project, run the following command to install the components:

```text
cd ../your_project/components
git clone http://git.zh.com.ru/esp_components/zh_inclinometer
```

In the application, add the component:

```c
#include "zh_inclinometer.h"
```

## Examples

```c
#include "zh_inclinometer.h"

double inclinometer_position = 0;

void app_main(void)
{
    esp_log_level_set("zh_inclinometer", ESP_LOG_ERROR);
    zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT();
    config.a_gpio_number = GPIO_NUM_26;
    config.b_gpio_number = GPIO_NUM_27;
    config.encoder_pulses = 3600;
    zh_inclinometer_init(&config);
    for (;;)
    {
        zh_inclinometer_get(&inclinometer_position);
        printf("Inclinometer position is %0.2f degrees.\n", inclinometer_position);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```
