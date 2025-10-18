#include "freertos/FreeRTOS.h"
#include "battery.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "app_events.h"

static int battery_levels[] = {
    3200,	// 0%
    3250,	// 1%
    3300,	// 2%
    3350,	// 3%
    3400,	// 4%
    3450,	// 5%
    3500,	// 6%
    3550,	// 7%
    3600,	// 8%
    3650,	// 9%
    3700,	// 10%
    3703,	// 11%
    3706,	// 12%
    3710,	// 13%
    3713,	// 14%
    3716,	// 15%
    3719,	// 16%
    3723,	// 17%
    3726,	// 18%
    3729,	// 19%
    3732,	// 20%
    3735,	// 21%
    3739,	// 22%
    3742,	// 23%
    3745,	// 24%
    3748,	// 25%
    3752,	// 26%
    3755,	// 27%
    3758,	// 28%
    3761,	// 29%
    3765,	// 30%
    3768,	// 31%
    3771,	// 32%
    3774,	// 33%
    3777,	// 34%
    3781,	// 35%
    3784,	// 36%
    3787,	// 37%
    3790,	// 38%
    3794,	// 39%
    3797,	// 40%
    3800,	// 41%
    3805,	// 42%
    3811,	// 43%
    3816,	// 44%
    3821,	// 45%
    3826,	// 46%
    3832,	// 47%
    3837,	// 48%
    3842,	// 49%
    3847,	// 50%
    3853,	// 51%
    3858,	// 52%
    3863,	// 53%
    3868,	// 54%
    3874,	// 55%
    3879,	// 56%
    3884,	// 57%
    3889,	// 58%
    3895,	// 59%
    3900,	// 60%
    3906,	// 61%
    3911,	// 62%
    3917,	// 63%
    3922,	// 64%
    3928,	// 65%
    3933,	// 66%
    3939,	// 67%
    3944,	// 68%
    3950,	// 69%
    3956,	// 70%
    3961,	// 71%
    3967,	// 72%
    3972,	// 73%
    3978,	// 74%
    3983,	// 75%
    3989,	// 76%
    3994,	// 77%
    4000,	// 78%
    4008,	// 79%
    4015,	// 80%
    4023,	// 81%
    4031,	// 82%
    4038,	// 83%
    4046,	// 84%
    4054,	// 85%
    4062,	// 86%
    4069,	// 87%
    4077,	// 88%
    4085,	// 89%
    4092,	// 90%
    4100,	// 91%
    4111,	// 92%
    4122,	// 93%
    4133,	// 94%
    4144,	// 95%
    4156,	// 96%
    4167,	// 97%
    4178,	// 98%
    4189,	// 99%
    4200,	// 100%
};

battery::~battery()
{
    deinit();
}

void battery::battery_timer_callback(TimerHandle_t xTimer)
{
    battery* pThis = (battery*) pvTimerGetTimerID(xTimer);
    if (pThis)
    {
        if(pThis->m_buffer_count < BUFFER_SIZE)
        {
            int voltage = 0;
            if (pThis->read_voltage(voltage) == ESP_OK)
            {
                pThis->m_buffer[pThis->m_buffer_count++] = voltage;
            }
        } else
        {
            // Calculate average
            int sum = 0;
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                sum += pThis->m_buffer[i];
            }
            int avg = sum / BUFFER_SIZE;
            if(pThis->m_last_voltage != avg)
            {
                pThis->m_last_voltage = avg;
                send_app_event(APP_EVENT_BATTERY);
            }
            pThis->m_buffer_count = 0;
        }
    }
}

esp_err_t battery::init()
{
    if (m_adc_handle != nullptr) {
        return ESP_ERR_INVALID_STATE; // Already initialized
    }

    adc_oneshot_unit_init_cfg_t init_config1 = {};
        init_config1.unit_id = static_cast<adc_unit_t>(m_adc_unit);
        init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &m_adc_handle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(m_adc_handle, static_cast<adc_channel_t>(m_channel), &config);
    if (ret != ESP_OK)
    {
        deinit();
        return ret;
    }

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = m_adc_unit,
        .chan = m_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &m_adc_cali_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGI("battery", "Calibration was failed, no calibration scheme available");
    }

    for (size_t i = 0; i < 100; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(read_voltage(m_last_voltage) == ESP_OK && m_last_voltage > 0)
        {
            break;
        }
    }

    m_buffer_count = 0;
    m_timer = xTimerCreate("battery_state", pdMS_TO_TICKS(1000), pdTRUE, this, battery_timer_callback);
    xTimerStart(m_timer, 0);

    send_app_event(APP_EVENT_BATTERY);

    return ESP_OK;
}

esp_err_t battery::deinit()
{
    if(m_adc_cali_handle)
    {
        adc_cali_delete_scheme_curve_fitting(m_adc_cali_handle);
        m_adc_cali_handle = nullptr;
    }
    if(m_adc_handle)
    {
        adc_oneshot_del_unit(m_adc_handle);
        m_adc_handle = nullptr;
    }
    if(m_timer)
    {
        xTimerStop(m_timer, 0);
        xTimerDelete(m_timer, 0);
        m_timer = nullptr;
    }
    return ESP_OK;
}

esp_err_t battery::read_voltage(int& voltage)
{
    if(m_adc_handle == nullptr)
    {
        return ESP_ERR_INVALID_STATE; // Not initialized
    }

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(m_adc_handle, m_channel, &raw);
    if (ret != ESP_OK)
    {
        return ret;
    }

    if (m_adc_cali_handle)
    {
        int voltage_mv = 0;
        ret = adc_cali_raw_to_voltage(m_adc_cali_handle, raw, &voltage_mv);
        if (ret == ESP_OK)
        {
            voltage = voltage_mv * 2; // voltage divider 1:2
            return ESP_OK;
        }
    }

    // If calibration is not available or failed, use a rough estimation
    // Assuming 11dB attenuation and 12-bit width
    // Vref = 1100mV, max ADC value = 4095
    // Voltage range = Vref * (1 + attenuation) = 1100mV * (1 + 3.548) ≈ 4903mV
    // voltage = raw / 4095 * 4903
    voltage = static_cast<int>((static_cast<uint64_t>(raw) * 4903) / 4095) * 2; // voltage divider 1:2
    return ESP_OK;
}

int battery::_get_charge_level(int voltage)
{
    int idx = 50;
    int prev = 0;
    int half = 0;
    if (voltage >= 4200)
    {
        return 100;
    }
    if (voltage <= 3200)
    {
        return 0;
    }
    while(true)
    {
        half = abs(idx - prev) / 2;
        prev = idx;
        if(voltage >= battery_levels[idx])
        {
            idx = idx + half;
        } else
        {
            idx = idx - half;
        }
        if (prev == idx)
        {
            break;
        }
    }
    return idx;
}
