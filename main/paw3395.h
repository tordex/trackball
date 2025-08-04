#ifndef __PAW3395_H__
#define __PAW3395_H__

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

class paw3395
{
private:
    const char*         m_log_tag = "PAW3395";
    spi_device_handle_t m_spi;
    gpio_num_t          m_pin_ncs;

public:
    paw3395() {}
    ~paw3395() {}

    esp_err_t init(spi_host_device_t host_id, gpio_num_t ncs_pin, uint16_t dpi = 26000);
    void DPI_Config(uint16_t CPI_Num);
    bool read_motion(int16_t *dx, int16_t *dy);
    void office_mode();

private:
    void cs_high()
    {
        gpio_set_level(m_pin_ncs, 1);
    }

    void cs_low()
    {
        gpio_set_level(m_pin_ncs, 0);
    }

    void delay_ms(uint16_t nms)
    {
        vTaskDelay(pdMS_TO_TICKS(nms));
    }

    void delay_us(uint32_t nus)
    {
        esp_rom_delay_us(nus);
    }

    void delay_125_ns(uint8_t nns);
    uint8_t SPI_SendReceive(uint8_t data);
    uint8_t read_register(uint8_t address, bool apply_cs = true);
    void write_register(uint8_t address, uint8_t value);

    void SPI_SendData(uint8_t data)
    {
        SPI_SendReceive(data);
    }

    uint8_t SPI_ReceiveData()
    {
        return SPI_SendReceive(0xFF);
    }
    
    void Power_up_sequence();
    void Power_Up_Initializaton_Register_Setting();

};


#endif // __PAW3395_H__