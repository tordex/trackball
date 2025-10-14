#ifndef BATTERY_H
#define BATTERY_H

#include "freertos/timers.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

class battery
{
private:
    adc_oneshot_unit_handle_t m_adc_handle = nullptr;
    adc_cali_handle_t m_adc_cali_handle = nullptr;
    bool m_adc_cali_enable = false;
    adc_channel_t m_channel = ADC_CHANNEL_3; // GPIO14 if ADC2
    adc_unit_t m_adc_unit = ADC_UNIT_2;

    constexpr static size_t BUFFER_SIZE = 20;
    int m_buffer[BUFFER_SIZE] = {};
    int m_buffer_count = 0;
    int m_last_voltage = 0;
    TimerHandle_t m_timer = nullptr;
public:
    battery(adc_channel_t channel = ADC_CHANNEL_3, adc_unit_t adc_unit = ADC_UNIT_2) : m_channel(channel), m_adc_unit(adc_unit) {}
    ~battery();

    esp_err_t init();
    esp_err_t deinit();
    void get_state(int& voltage, int& level) { voltage = m_last_voltage; level = _get_charge_level(m_last_voltage); }

private:
    int _get_charge_level(int voltage);
    static void battery_timer_callback(TimerHandle_t xTimer);
    esp_err_t read_voltage(int& voltage);
};

#endif // BATTERY_H