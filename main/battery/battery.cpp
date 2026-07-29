#include "freertos/FreeRTOS.h"
#include "battery.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <algorithm>
#include <array>

namespace
{
    constexpr int kVoltageDividerRatio = 2;
    constexpr int kLevelHysteresisMv   = 10;

    constexpr std::array<int, 101> battery_levels = {
        3200, // 0%
        3250, // 1%
        3300, // 2%
        3350, // 3%
        3400, // 4%
        3450, // 5%
        3500, // 6%
        3550, // 7%
        3600, // 8%
        3650, // 9%
        3700, // 10%
        3703, // 11%
        3706, // 12%
        3710, // 13%
        3713, // 14%
        3716, // 15%
        3719, // 16%
        3723, // 17%
        3726, // 18%
        3729, // 19%
        3732, // 20%
        3735, // 21%
        3739, // 22%
        3742, // 23%
        3745, // 24%
        3748, // 25%
        3752, // 26%
        3755, // 27%
        3758, // 28%
        3761, // 29%
        3765, // 30%
        3768, // 31%
        3771, // 32%
        3774, // 33%
        3777, // 34%
        3781, // 35%
        3784, // 36%
        3787, // 37%
        3790, // 38%
        3794, // 39%
        3797, // 40%
        3800, // 41%
        3805, // 42%
        3811, // 43%
        3816, // 44%
        3821, // 45%
        3826, // 46%
        3832, // 47%
        3837, // 48%
        3842, // 49%
        3847, // 50%
        3853, // 51%
        3858, // 52%
        3863, // 53%
        3868, // 54%
        3874, // 55%
        3879, // 56%
        3884, // 57%
        3889, // 58%
        3895, // 59%
        3900, // 60%
        3906, // 61%
        3911, // 62%
        3917, // 63%
        3922, // 64%
        3928, // 65%
        3933, // 66%
        3939, // 67%
        3944, // 68%
        3950, // 69%
        3956, // 70%
        3961, // 71%
        3967, // 72%
        3972, // 73%
        3978, // 74%
        3983, // 75%
        3989, // 76%
        3994, // 77%
        4000, // 78%
        4008, // 79%
        4015, // 80%
        4023, // 81%
        4031, // 82%
        4038, // 83%
        4046, // 84%
        4054, // 85%
        4062, // 86%
        4069, // 87%
        4077, // 88%
        4085, // 89%
        4092, // 90%
        4100, // 91%
        4111, // 92%
        4122, // 93%
        4133, // 94%
        4144, // 95%
        4156, // 96%
        4167, // 97%
        4178, // 98%
        4189, // 99%
        4200, // 100%
    };

    int apply_level_hysteresis(int voltage, int last_level, int suggested_level)
    {
        if(last_level < 0)
        {
            return std::clamp(suggested_level, 0, 100);
        }

        int level = std::clamp(last_level, 0, 100);
        if(suggested_level > level)
        {
            while(level < suggested_level)
            {
                const int next_level = level + 1;
                if(voltage >= (battery_levels[next_level] + kLevelHysteresisMv))
                {
                    level = next_level;
                } else
                {
                    break;
                }
            }
        } else if(suggested_level < level)
        {
            while(level > suggested_level)
            {
                if(voltage < (battery_levels[level] - kLevelHysteresisMv))
                {
                    level--;
                } else
                {
                    break;
                }
            }
        }

        return level;
    }
} // namespace

battery::~battery()
{
    deinit();
}

void battery::on_timer()
{
    int voltage = 0;
    if(read_voltage(voltage) != ESP_OK)
    {
        return;
    }

    m_buffer[m_buffer_count++] = voltage;
    if(m_buffer_count < BUFFER_SIZE)
    {
        return;
    }

    // Calculate average as soon as the buffer is full (no extra 1-second delay).
    int64_t sum = 0;
    for(size_t i = 0; i < BUFFER_SIZE; i++)
    {
        sum += m_buffer[i];
    }

    int avg = static_cast<int>(sum / static_cast<int64_t>(BUFFER_SIZE));
    if(m_last_voltage != avg)
    {
        m_last_voltage               = avg;
        const int interpolated_level = _get_charge_level(m_last_voltage);
        const int stable_level       = apply_level_hysteresis(m_last_voltage, m_last_level, interpolated_level);
        m_last_level                 = stable_level;
        if(m_callback)
        {
            m_callback(m_last_voltage, stable_level);
        }
    }
    m_buffer_count = 0;
}

esp_err_t battery::init()
{
    if(m_adc_handle != nullptr)
    {
        return ESP_ERR_INVALID_STATE; // Already initialized
    }

    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id                     = static_cast<adc_unit_t>(m_adc_unit);
    init_config1.ulp_mode                    = ADC_ULP_MODE_DISABLE;
    esp_err_t ret                            = adc_oneshot_new_unit(&init_config1, &m_adc_handle);
    if(ret != ESP_OK)
    {
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(m_adc_handle, static_cast<adc_channel_t>(m_channel), &config);
    if(ret != ESP_OK)
    {
        deinit();
        return ret;
    }

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id  = m_adc_unit,
        .chan     = m_channel,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &m_adc_cali_handle);
    if(ret != ESP_OK)
    {
        ESP_LOGI("battery", "Calibration was failed, no calibration scheme available");
    }

    // Read initial voltage
    for(size_t i = 0; i < 100; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(read_voltage(m_last_voltage) == ESP_OK && m_last_voltage > 0)
        {
            break;
        }
    }

    // report initial state
    const int interpolated_level = _get_charge_level(m_last_voltage);
    m_last_level                 = apply_level_hysteresis(m_last_voltage, m_last_level, interpolated_level);
    if(m_callback)
    {
        m_callback(m_last_voltage, m_last_level);
    }

    // Start battery check timer
    m_buffer_count = 0;
    m_timer.set_callback([this]() { on_timer(); });
    m_timer.start(1000, true);

    return ESP_OK;
}

esp_err_t battery::deinit()
{
    m_timer.stop();

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

    m_last_level   = -1;
    m_last_voltage = 0;
    m_buffer_count = 0;

    return ESP_OK;
}

esp_err_t battery::read_voltage(int& voltage)
{
    if(m_adc_handle == nullptr)
    {
        return ESP_ERR_INVALID_STATE; // Not initialized
    }

    int       raw = 0;
    esp_err_t ret = adc_oneshot_read(m_adc_handle, m_channel, &raw);
    if(ret != ESP_OK)
    {
        return ret;
    }

    if(m_adc_cali_handle)
    {
        int voltage_mv = 0;
        ret            = adc_cali_raw_to_voltage(m_adc_cali_handle, raw, &voltage_mv);
        if(ret == ESP_OK)
        {
            voltage = voltage_mv * kVoltageDividerRatio; // voltage divider 1:2
            return ESP_OK;
        }
    }

    // If calibration is not available or failed, use a rough estimation
    // Assuming 11dB attenuation and 12-bit width
    // Vref = 1100mV, max ADC value = 4095
    // Voltage range = Vref * (1 + attenuation) = 1100mV * (1 + 3.548) ≈ 4903mV
    // voltage = raw / 4095 * 4903
    voltage =
        static_cast<int>((static_cast<uint64_t>(raw) * 4903) / 4095) * kVoltageDividerRatio; // voltage divider 1:2
    return ESP_OK;
}

int battery::_get_charge_level(int voltage)
{
    if(voltage <= battery_levels.front())
    {
        return 0;
    }
    if(voltage >= battery_levels.back())
    {
        return 100;
    }

    // Interpolate between adjacent 1% thresholds and round to nearest integer percent.
    auto      it        = std::upper_bound(battery_levels.begin(), battery_levels.end(), voltage);
    const int upper_idx = static_cast<int>(std::distance(battery_levels.begin(), it));
    const int lower_idx = upper_idx - 1;

    const int lower_mv = battery_levels[lower_idx];
    const int upper_mv = battery_levels[upper_idx];
    const int span_mv  = upper_mv - lower_mv;
    if(span_mv <= 0)
    {
        return lower_idx;
    }

    const int64_t num       = static_cast<int64_t>(voltage - lower_mv) * 100;
    const int     frac_x100 = static_cast<int>((num + (span_mv / 2)) / span_mv); // 0..100 within this 1% step

    const int level = lower_idx + ((frac_x100 >= 50) ? 1 : 0);
    return std::clamp(level, 0, 100);
}
