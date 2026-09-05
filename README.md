# ESP32 ESP-IDF component for inclinometer (via optical rotary encoder)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Features

1. Quadrature encoder support with configurable GPIO pins
2. Configurable pull-up resistor enablement for encoder channels
3. Support for both clockwise and counter-clockwise rotation directions
4. Configurable pulses per revolution for accurate angle calculation
5. Position tracking with get and reset operations
6. PCNT glitch filter for noise rejection (1000ns)
7. Multiple inclinometer instances support

## Note

Enable the following settings in menuconfig:

```text
PCNT_CTRL_FUNC_IN_IRAM
PCNT_ISR_IRAM_SAF
```

## Using

In an existing project, run the following command to install the components:

```bash
cd ../your_project/components
git clone https://github.com/aZholtikov/zh_inclinometer
```

In the application, add the component:

```c
#include "zh_inclinometer.h"
```

## Example

```c
#include "zh_inclinometer.h"

zh_inclinometer_handle_t *inclinometer_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_inclinometer", ESP_LOG_ERROR);
    zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT();
    config.a_gpio_number = GPIO_NUM_26;
    config.b_gpio_number = GPIO_NUM_27;
    config.encoder_pulses = 3600;
    zh_inclinometer_init(&config, &inclinometer_handle);
    for (;;)
    {
        float angle = 0;
        zh_inclinometer_get(&inclinometer_handle, &angle);
        printf("Inclinometer position is %0.2f degrees.\n", angle);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```
