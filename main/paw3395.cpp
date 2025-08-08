#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "paw3395.h"

// Timing constants
const uint8_t PAW3395_TIMINGS_SRAD     = 2;   // 2μs
const uint8_t PAW3395_TIMINGS_SRWSRR   = 2;   // 2μs
const uint8_t PAW3395_TIMINGS_SWW      = 5;   // 5μs
const uint8_t PAW3395_TIMINGS_SWR      = 5;   // 5μs
const uint8_t PAW3395_TIMINGS_NCS_SCLK = 1;   // 120ns
const uint8_t PAW3395_TIMINGS_BEXIT    = 4;   // 500ns

const uint8_t PAW3395_SPI_WRITE = 0x80;

// Register addresses
const uint8_t PAW3395_SPIREGISTER_MOTION        = 0x02;
const uint8_t PAW3395_SPIREGISTER_DELTA_X_L     = 0x03;
const uint8_t PAW3395_SPIREGISTER_DELTA_X_H     = 0x04;
const uint8_t PAW3395_SPIREGISTER_DELTA_Y_L     = 0x05;
const uint8_t PAW3395_SPIREGISTER_DELTA_Y_H     = 0x06;
const uint8_t PAW3395_SPIREGISTER_MotionBurst   = 0x16;
const uint8_t PAW3395_SPIREGISTER_POWERUPRESET  = 0x3A;
const uint8_t SET_RESOLUTION                    = 0x47;
const uint8_t RESOLUTION_X_LOW                  = 0x48;
const uint8_t RESOLUTION_X_HIGH                 = 0x49;
const uint8_t RESOLUTION_Y_LOW                  = 0x4A;
const uint8_t RESOLUTION_Y_HIGH                 = 0x4B;
const uint8_t RIPPLE_CONTROL                    = 0x5A;
const uint8_t MOTION_CTRL                       = 0x5C;

// Register values
const uint8_t PAW3395_POWERUPRESET_POWERON      = 0x5A;

// Register bits
const uint8_t PAW3395_OP_MODE0 = 0;
const uint8_t PAW3395_OP_MODE1 = 1;
const uint8_t PAW3395_PG_FIRST = 6;
const uint8_t PAW3395_PG_VALID = 7;


esp_err_t paw3395::init(spi_host_device_t host_id, gpio_num_t ncs_pin, uint16_t dpi)
{
    m_pin_ncs = ncs_pin;

    // Configure NCS pin
    gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << m_pin_ncs);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    cs_high();

    // Configure SPI device
    spi_device_interface_config_t devcfg = {};
        devcfg.clock_speed_hz = 5000000; // 10MHz
        devcfg.mode = 3;                  // SPI Mode 3 (CPOL=1, CPHA=1)
        devcfg.spics_io_num = -1;         // Manually control CS
        devcfg.queue_size = 7;
        devcfg.flags = SPI_DEVICE_NO_DUMMY;

    esp_err_t ret = spi_bus_add_device(host_id, &devcfg, &m_spi);
    if (ret != ESP_OK) 
    {
        printf("SPI device add failed: %d\n", ret);
        return ret;
    }

    cs_high();
    Power_up_sequence();
    DPI_Config(dpi);

    uint8_t product_id = read_register(0);
    printf("PAW3395 Product ID: 0x%02X\n", product_id);

    write_register(0x5A, 0x90);
    //office_mode();

    return ESP_OK;
}

void paw3395::delay_125_ns(uint8_t nns)
{
    uint32_t cycles = (125 * nns) / (1000000000 / 240000000); // Assuming 240MHz CPU
    uint32_t start = esp_cpu_get_cycle_count();
    while (esp_cpu_get_cycle_count() - start < cycles);
}

uint8_t paw3395::SPI_SendReceive(uint8_t data)
{
    spi_transaction_t t = {};
        t.length = 8;
        t.tx_buffer = &data;
        t.rxlength = 8;
        t.rx_buffer = &data;

    esp_err_t ret = spi_device_polling_transmit(m_spi, &t);
    if (ret != ESP_OK) 
    {
        printf("SPI transaction failed: %d\n", ret);
    }
    return data;
}

uint8_t paw3395::read_register(uint8_t address, bool apply_cs)
{
    if(apply_cs) cs_low();
    delay_125_ns(1);
    SPI_SendReceive(address);
    delay_us(5); // t_SRAD
    uint8_t temp = SPI_SendReceive(0xFF);
    if(apply_cs) cs_high();
    //delay_us(5); // t_SWW
    return temp;
}

void paw3395::write_register(uint8_t address, uint8_t value)
{
    cs_low();
    delay_125_ns(1);
    SPI_SendReceive(address | 0x80);
    SPI_SendReceive(value);
    cs_high();
    delay_us(5); // t_SWW
}

void paw3395::Power_up_sequence()
{
    uint8_t reg_it;
    delay_ms(50);
    cs_low();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);
    cs_high();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);
    cs_low();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);

    write_register(PAW3395_SPIREGISTER_POWERUPRESET, PAW3395_POWERUPRESET_POWERON);
    delay_ms(5);
    Power_Up_Initializaton_Register_Setting();
    cs_high();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);

    for (reg_it = 0x02; reg_it <= 0x06; reg_it++)
    {
        read_register(reg_it);
        delay_us(PAW3395_TIMINGS_SRWSRR);
    }
    cs_high();
    delay_125_ns(PAW3395_TIMINGS_BEXIT);
}

bool paw3395::read_motion(int16_t *dx, int16_t *dy)
{
    uint8_t buffer[12] = {0};
    cs_low();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);
    SPI_SendReceive(PAW3395_SPIREGISTER_MotionBurst);
    delay_us(PAW3395_TIMINGS_SRAD);
    for (uint8_t i = 0; i < 12; i++)
    {
        buffer[i] = SPI_SendReceive(0x00);
    }
    cs_high();
    delay_125_ns(PAW3395_TIMINGS_BEXIT);
    *dx = (int16_t)(buffer[2] + (buffer[3] << 8));
    *dy = (int16_t)(buffer[4] + (buffer[5] << 8));
    return *dx != 0 || *dy != 0;
}

void paw3395::Power_Up_Initializaton_Register_Setting()
{
    uint8_t read_tmp;
    uint8_t i;
    write_register(0x7F, 0x07);
    write_register(0x40, 0x41);
    write_register(0x7F, 0x00);
    write_register(0x40, 0x80);
    write_register(0x7F, 0x0E);
    write_register(0x55, 0x0D);
    write_register(0x56, 0x1B);
    write_register(0x57, 0xE8);
    write_register(0x58, 0xD5);
    write_register(0x7F, 0x14);
    write_register(0x42, 0xBC);
    write_register(0x43, 0x74);
    write_register(0x4B, 0x20);
    write_register(0x4D, 0x00);
    write_register(0x53, 0x0E);
    write_register(0x7F, 0x05);
    write_register(0x44, 0x04);
    write_register(0x4D, 0x06);
    write_register(0x51, 0x40);
    write_register(0x53, 0x40);
    write_register(0x55, 0xCA);
    write_register(0x5A, 0xE8);
    write_register(0x5B, 0xEA);
    write_register(0x61, 0x31);
    write_register(0x62, 0x64);
    write_register(0x6D, 0xB8);
    write_register(0x6E, 0x0F);

    write_register(0x70, 0x02);
    write_register(0x4A, 0x2A);
    write_register(0x60, 0x26);
    write_register(0x7F, 0x06);
    write_register(0x6D, 0x70);
    write_register(0x6E, 0x60);
    write_register(0x6F, 0x04);
    write_register(0x53, 0x02);
    write_register(0x55, 0x11);
    write_register(0x7A, 0x01);
    write_register(0x7D, 0x51);
    write_register(0x7F, 0x07);
    write_register(0x41, 0x10);
    write_register(0x42, 0x32);
    write_register(0x43, 0x00);
    write_register(0x7F, 0x08);
    write_register(0x71, 0x4F);
    write_register(0x7F, 0x09);
    write_register(0x62, 0x1F);
    write_register(0x63, 0x1F);
    write_register(0x65, 0x03);
    write_register(0x66, 0x03);
    write_register(0x67, 0x1F);
    write_register(0x68, 0x1F);
    write_register(0x69, 0x03);
    write_register(0x6A, 0x03);
    write_register(0x6C, 0x1F);

    write_register(0x6D, 0x1F);
    write_register(0x51, 0x04);
    write_register(0x53, 0x20);
    write_register(0x54, 0x20);
    write_register(0x71, 0x0C);
    write_register(0x72, 0x07);
    write_register(0x73, 0x07);
    write_register(0x7F, 0x0A);
    write_register(0x4A, 0x14);
    write_register(0x4C, 0x14);
    write_register(0x55, 0x19);
    write_register(0x7F, 0x14);
    write_register(0x4B, 0x30);
    write_register(0x4C, 0x03);
    write_register(0x61, 0x0B);
    write_register(0x62, 0x0A);
    write_register(0x63, 0x02);
    write_register(0x7F, 0x15);
    write_register(0x4C, 0x02);
    write_register(0x56, 0x02);
    write_register(0x41, 0x91);
    write_register(0x4D, 0x0A);
    write_register(0x7F, 0x0C);
    write_register(0x4A, 0x10);
    write_register(0x4B, 0x0C);
    write_register(0x4C, 0x40);
    write_register(0x41, 0x25);
    write_register(0x55, 0x18);
    write_register(0x56, 0x14);
    write_register(0x49, 0x0A);
    write_register(0x42, 0x00);
    write_register(0x43, 0x2D);
    write_register(0x44, 0x0C);
    write_register(0x54, 0x1A);
    write_register(0x5A, 0x0D);
    write_register(0x5F, 0x1E);
    write_register(0x5B, 0x05);
    write_register(0x5E, 0x0F);
    write_register(0x7F, 0x0D);
    write_register(0x48, 0xDD);
    write_register(0x4F, 0x03);
    write_register(0x52, 0x49);

    write_register(0x51, 0x00);
    write_register(0x54, 0x5B);
    write_register(0x53, 0x00);

    write_register(0x56, 0x64);
    write_register(0x55, 0x00);
    write_register(0x58, 0xA5);
    write_register(0x57, 0x02);
    write_register(0x5A, 0x29);
    write_register(0x5B, 0x47);
    write_register(0x5C, 0x81);
    write_register(0x5D, 0x40);
    write_register(0x71, 0xDC);
    write_register(0x70, 0x07);
    write_register(0x73, 0x00);
    write_register(0x72, 0x08);
    write_register(0x75, 0xDC);
    write_register(0x74, 0x07);
    write_register(0x77, 0x00);
    write_register(0x76, 0x08);
    write_register(0x7F, 0x10);
    write_register(0x4C, 0xD0);
    write_register(0x7F, 0x00);
    write_register(0x4F, 0x63);
    write_register(0x4E, 0x00);
    write_register(0x52, 0x63);
    write_register(0x51, 0x00);
    write_register(0x54, 0x54);
    write_register(0x5A, 0x10);
    write_register(0x77, 0x4F);
    write_register(0x47, 0x01);
    write_register(0x5B, 0x40);
    write_register(0x64, 0x60);
    write_register(0x65, 0x06);
    write_register(0x66, 0x13);
    write_register(0x67, 0x0F);
    write_register(0x78, 0x01);
    write_register(0x79, 0x9C);
    write_register(0x40, 0x00);
    write_register(0x55, 0x02);
    write_register(0x23, 0x70);
    write_register(0x22, 0x01);

    delay_ms(1);

    for (i = 0; i < 60; i++)
    {
        read_tmp = read_register(0x6C);
        if (read_tmp == 0x80)
            break;
        delay_ms(1);
    }
    if (i == 60)
    {
        write_register(0x7F, 0x14);
        write_register(0x6C, 0x00);
        write_register(0x7F, 0x00);
    }
    write_register(0x22, 0x00);
    write_register(0x55, 0x00);
    write_register(0x7F, 0x07);
    write_register(0x40, 0x40);
    write_register(0x7F, 0x00);
}

void paw3395::DPI_Config(uint16_t CPI_Num)
{
    uint8_t temp;
    cs_low();
    delay_125_ns(PAW3395_TIMINGS_NCS_SCLK);
    write_register(MOTION_CTRL, 0x00);
    temp = (uint8_t)(((CPI_Num / 50) << 8) >> 8);
    write_register(RESOLUTION_X_LOW, temp);
    temp = (uint8_t)((CPI_Num / 50) >> 8);
    write_register(RESOLUTION_X_HIGH, temp);
    write_register(SET_RESOLUTION, 0x01);
    cs_high();
    delay_125_ns(PAW3395_TIMINGS_BEXIT);
}

void paw3395::office_mode()
{
    write_register(0x7F, 0x05);
    write_register(0x51, 0x28);
    write_register(0x53, 0x30);
    write_register(0x61, 0x3B);
    write_register(0x6E, 0x1F);
    write_register(0x7F, 0x07);
    write_register(0x42, 0x32);
    write_register(0x43, 0x00);
    write_register(0x7F, 0x0D);
    write_register(0x51, 0x00);
    write_register(0x52, 0x49);
    write_register(0x53, 0x00);
    write_register(0x54, 0x5B);
    write_register(0x55, 0x00);
    write_register(0x56, 0x64);
    write_register(0x57, 0x02);
    write_register(0x58, 0xA5);
    write_register(0x7F, 0x00);
    write_register(0x54, 0x52);
    write_register(0x78, 0x0A);
    write_register(0x79, 0x0F);
    uint8_t tmp = read_register(0x40);
    tmp = (tmp & 0xFC) | 0x02;
}
