# Описание компонента zh_inclinometer

## Общие сведения

**zh_inclinometer** — это компонент ESP-IDF для ESP32, реализующий измерение углового положения с помощью квадратурного оптического энкодера. Компонент использует аппаратный модуль счётчика импульсов (PCNT) из ESP-IDF для подсчёта импульсов с двух каналов энкодера и преобразования их в угол в градусах.

Компонент предоставляет простой API для инициализации, получения текущего угла и сброса счётчика. Поддерживается настройка направлений вращения, режима подтяжки GPIO и разрешения энкодера в импульсах на оборот.

## Основные возможности

1. **Поддержка квадратурного энкодера** — аппаратное декодирование каналов A и B через PCNT.
2. **Настраиваемые GPIO-пины** — задание любых свободных GPIO для каналов A и B.
3. **Настраиваемая подтяжка** — включение или отключение внутренних подтягивающих резисторов.
4. **Настраиваемое направление вращения** — положительный угол для вращения по часовой или против часовой стрелки.
5. **Настраиваемое разрешение** — указание количества импульсов на полный оборот энкодера.
6. **Фильтр глитчей** — аппаратное отсечение помех длительностью менее 1000 нс.
7. **Поддержка нескольких экземпляров** — одновременная работа с несколькими энкодерами.

## Внутреннее устройство

### Структура `zh_inclinometer_handle_t`

Публичный тип — непрозрачный указатель на внутреннюю структуру:

| Поле | Тип | Описание |
| ------ | ----- | ---------- |
| `pcnt_unit_handle` | `pcnt_unit_handle_t` | Указатель на экземпляр PCNT для данного энкодера |
| `pcnt_channel_a_handle` | `pcnt_channel_handle_t` | Указатель на канал A PCNT (подключён к пину энкодера A) |
| `pcnt_channel_b_handle` | `pcnt_channel_handle_t` | Указатель на канал B PCNT (подключён к пину энкодера B) |
| `degrees_per_pulse` | `float` | Угол в градусах, приходящийся на один подсчитанный импульс |
| `rotation` | `bool` | Флаг направления вращения: `true` — положительный угол при вращении по часовой стрелке |

### Структура конфигурации `zh_inclinometer_init_config_t`

Структура передаётся в функцию `zh_inclinometer_init()` при создании экземпляра:

| Поле | Тип | Описание |
| ------ | ----- | ---------- |
| `a_gpio_number` | `gpio_num_t` | Номер GPIO для канала A энкодера |
| `b_gpio_number` | `gpio_num_t` | Номер GPIO для канала B энкодера |
| `pullup` | `bool` | Включить подтягивающий резистор (true) или отключить (false) |
| `rotation` | `bool` | Направление: true — положительный угол при вращении по часовой стрелке |
| `encoder_pulses` | `uint16_t` | Количество импульсов энкодера на один полный оборот |

### Управление памятью

Экземпляр inclinometer выделяется в функции `zh_inclinometer_init()` через `heap_caps_calloc(1, sizeof(zh_inclinometer_handle_t), MALLOC_CAP_8BIT)` и полностью очищается в `zh_inclinometer_deinit()` через `heap_caps_free()`. Ресурсы PCNT (unit и каналы) освобождаются до освобождения самого handle.

## API (интерфейс)

### Инициализация и освобождение

| Функция | Описание |
| --------- | ---------- |
| `zh_inclinometer_init()` | Выделяет память, проверяет конфигурацию, инициализирует PCNT с двумя каналами для квадратурного декодирования и запускает счётчик. |
| `zh_inclinometer_deinit()` | Останавливает и отключает PCNT, удаляет каналы и unit, освобождает память handle и устанавливает его в NULL. |
| `zh_inclinometer_reset()` | Очищает счётчик PCNT, устанавливая текущую позицию в 0 градусов. |

### Получение данных

| Функция | Описание |
|---------|----------|
| `zh_inclinometer_get()` | Считывает текущее значение счётчика PCNT и преобразует его в угол в градусах с учётом направления вращения. |

## Алгоритмы

### Расчёт угла

При инициализации вычисляется коэффициент градусов на импульс:

```text
degrees_per_pulse = 360.0 / (4 * encoder_pulses)
```

Множитель 4 обусловлен квадратурным декодированием: один полный шаг квадратурного энкодера содержит 4 изменения сигнала (по фронту и спаду на обоих каналах A и B), что увеличивает эффективное разрешение в 4 раза.

Текущий угол вычисляется при каждом вызове `zh_inclinometer_get()`:

```text
angle = pcnt_count * degrees_per_pulse
```

Если `rotation == false`, угол инвертируется:

```text
angle = -angle
```

Отрицательный ноль приводится к положительному нулю (`0.0`).

### Декодирование квадратурного сигнала

Инициализация PCNT настраивает два канала для двунаправленного подсчёта. Каждый канал реагирует на фронты своего сигнала (edge action), а направление счёта определяется текущим уровнем второго сигнала (level action):

| Канал | Edge action | Level action |
|-------|-------------|--------------|
| `A` (edge — пин A, level — пин B) | фронт — `DECREASE`, спад — `INCREASE` | `B == 1` — `KEEP`, `B == 0` — `INVERSE` |
| `B` (edge — пин B, level — пин A) | фронт — `INCREASE`, спад — `DECREASE` | `A == 1` — `KEEP`, `A == 0` — `INVERSE` |

`INVERSE` меняет действие счётчика на противоположное, поэтому при одном направлении вращения все четыре перехода сигналов дают `+1`, при противоположном — `-1`. Так реализуется четырёхкратное увеличение разрешения относительно шага энкодера.

Знак результата зависит от физически подключённой последовательности каналов. Параметр `rotation` компенсирует эту разницу: при `rotation == false` знак угла инвертируется при чтении, а конфигурация PCNT остаётся неизменной.

### Фильтр глитчей

Встроенный аппаратный фильтр отбрасывает импульсы длительностью менее 1000 нс (1 мкс). Фильтр настраивается функцией `pcnt_unit_set_glitch_filter()` и работает на уровне периферии, не создавая нагрузки на процессор.

### Очистка при ошибках

При инициализации PCNT реализована каскадная очистка ресурсов: при ошибке создания или настройки каждого последующего ресурса предыдущие удаляются в обратном порядке. Это гарантирует отсутствие утечек ресурсов при неудачной инициализации.

## Примеры использования

### Базовое чтение угла

Пример чтения текущего угла энкодера с периодом 100 мс.

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
    config.pullup = true;
    config.rotation = true;

    esp_err_t ret = zh_inclinometer_init(&config, &inclinometer_handle);
    if (ret != ESP_OK)
    {
        printf("Inclinometer initialization error: %s\n", esp_err_to_name(ret));
        return;
    }

    for (int i = 0; i < 100; i++)
    {
        float angle = 0;
        ret = zh_inclinometer_get(&inclinometer_handle, &angle);
        if (ret == ESP_OK)
        {
            printf("Angle: %.2f deg\n", angle);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    zh_inclinometer_deinit(&inclinometer_handle);
}
```

### Сброс позиции и работа с разными конфигурациями

Пример инициализации с отключённой подтяжкой, обратным направлением и периодическим сбросом позиции.

```c
#include "zh_inclinometer.h"

zh_inclinometer_handle_t *inclinometer_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_inclinometer", ESP_LOG_ERROR);

    zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT();
    config.a_gpio_number = GPIO_NUM_26;
    config.b_gpio_number = GPIO_NUM_27;
    config.encoder_pulses = 1800;
    config.pullup = false;
    config.rotation = false;

    esp_err_t ret = zh_inclinometer_init(&config, &inclinometer_handle);
    if (ret != ESP_OK)
    {
        printf("Initialization error: %s\n", esp_err_to_name(ret));
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        float angle = 0;
        ret = zh_inclinometer_get(&inclinometer_handle, &angle);
        if (ret == ESP_OK)
        {
            printf("Angle before reset: %.2f deg\n", angle);
        }

        ret = zh_inclinometer_reset(&inclinometer_handle);
        if (ret == ESP_OK)
        {
            printf("Position reset to zero.\n");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    zh_inclinometer_deinit(&inclinometer_handle);
}
```

### Ограничение на количество импульсов

Пример с максимальным разрешением (encoder_pulses = 8191) и демонстрация обработки ошибок.

```c
#include "zh_inclinometer.h"

zh_inclinometer_handle_t *inclinometer_handle = NULL;

void app_main(void)
{
    esp_log_level_set("zh_inclinometer", ESP_LOG_ERROR);

    zh_inclinometer_init_config_t config = ZH_INCLINOMETER_INIT_CONFIG_DEFAULT();
    config.a_gpio_number = GPIO_NUM_26;
    config.b_gpio_number = GPIO_NUM_27;
    config.encoder_pulses = 8191;
    config.pullup = true;
    config.rotation = true;

    esp_err_t ret = zh_inclinometer_init(&config, &inclinometer_handle);
    if (ret != ESP_OK)
    {
        printf("Inclinometer initialization error: %s\n", esp_err_to_name(ret));
        return;
    }

    float angle = 0;
    ret = zh_inclinometer_get(&inclinometer_handle, &angle);
    if (ret == ESP_OK)
    {
        printf("Current angle: %.2f deg\n", angle);
    }

    ret = zh_inclinometer_reset(&inclinometer_handle);
    if (ret == ESP_OK)
    {
        printf("Position reset.\n");
    }

    zh_inclinometer_deinit(&inclinometer_handle);
}
```

## Логирование

Компонент использует ESP-IDF логирование с тегом `"zh_inclinometer"`. Уровень логирования настраивается через `esp_log_level_set()`:

```c
esp_log_level_set("zh_inclinometer", ESP_LOG_ERROR);
```

Макросы логирования определены внутри компонента и не требуют отдельного подключения. Доступные уровни: `ESP_LOG_ERROR` и `ESP_LOGI`.

## Ошибки

| Код | Описание |
| ----- | ---------- |
| `ESP_OK` | Успешное выполнение операции |
| `ESP_ERR_INVALID_ARG` | Неверный аргумент: NULL-указатель, невалидный GPIO (>= GPIO_NUM_MAX), одинаковые пины A и B, `encoder_pulses` равно 0 |
| `ESP_ERR_INVALID_STATE` | Повторная инициализация: `*handle` уже не равен NULL |
| `ESP_ERR_NO_MEM` | Не удалось выделить память для handle |
| `ESP_FAIL` | Ошибка инициализации PCNT, GPIO, фильтра глитчей или других ресурсов |

## Ограничения

1. **Максимум 8191 импульс на оборот** — ограничено диапазоном счётчика PCNT (±32767), так как внутренний счётчик умножается на 4 при квадратурном декодировании (`4 * encoder_pulses <= 32767`).
2. **Сброс счётчика при достижении пределов** — аппаратный PCNT автоматически обнуляет счётчик при достижении high_limit или low_limit. Угол «зацикливается» в диапазоне приблизительно от -360° до 360°. При вращении за пределы одного оборота счётчик сбрасывается и угол начинается заново.
3. **Не потокобезопасен** — компонент не использует мьютексы или другие механизмы синхронизации. Одновременный доступ к handle из нескольких задач не поддерживается.
4. **Глитч-фильтр зафиксирован на 1000 нс** — значение не выносится в конфигурацию и не может быть изменено пользователем.
5. **GPIO_NUM_MAX не является допустимым значением** — оба GPIO должны быть явно заданы перед инициализацией. Значение `GPIO_NUM_MAX` в `ZH_INCLINOMETER_INIT_CONFIG_DEFAULT()` служит маркером «не задано».

## Требования для сборки

- ESP-IDF версии 5.0 или выше
- Компонент `driver` (ESP-IDF < 6) или `esp_driver_gpio` + `esp_driver_pcnt` (ESP-IDF 6)
- FreeRTOS (входит в состав ESP-IDF)
- Два свободных GPIO-пина для подключения энкодера
- Модуль PCNT периферии ESP32 (доступен на всех моделях ESP32)

## Установка

```bash
cd ваш_проект/components
git clone https://github.com/aZholtikov/zh_inclinometer
```

```c
#include "zh_inclinometer.h"
```
