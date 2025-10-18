#ifndef __PAW3395_H__
#define __PAW3395_H__

#include <cstdint>
#include <functional>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

class paw3395
{
public:
	using OnMotionCallback_t = std::function<void(int16_t, int16_t)>;

	struct motion_burst_data
	{
		uint8_t motion;
		uint8_t observation;
		uint8_t delta_x_l;
		uint8_t delta_x_h;
		uint8_t delta_y_l;
		uint8_t delta_y_h;
		uint8_t squal;
		uint8_t raw_data_sum;
		uint8_t max_raw_data;
		uint8_t min_raw_data;
		uint8_t shutter_upper;
		uint8_t shutter_lower;
	} __attribute__((packed));

private:
	const char*			m_log_tag			 = "PAW3395";
	spi_device_handle_t m_spi				 = nullptr;
	gpio_num_t			m_pin_ncs			 = GPIO_NUM_NC;
	gpio_num_t			m_pin_motion		 = GPIO_NUM_NC;
	SemaphoreHandle_t	m_motion_semaphore	 = nullptr;
	TaskHandle_t		m_motion_task		 = nullptr;
	OnMotionCallback_t	m_on_motion_callback = nullptr;

public:
	paw3395() {}
	~paw3395() {}

	esp_err_t init(spi_host_device_t host_id, gpio_num_t ncs_pin, gpio_num_t pin_motion, uint16_t dpi,
				   const OnMotionCallback_t& on_motion);

	/// @brief Set the lift cut height
	/// @param lift_height The lift cut height: 0 - 1mm, 1 - 2mm
	void set_lift_cut(uint8_t lift_height);

	void DPI_Config(uint16_t CPI_Num);
	bool read_motion(int16_t* dx, int16_t* dy);
	void motion_burst(motion_burst_data * values);
	void office_mode();
	void gaming_mode();

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

	void		init_motion_pin();
	static void motion_task(void* param);

	void	delay_125_ns(uint8_t nns);
	uint8_t SPI_SendReceive(uint8_t data);
	uint8_t read_register(uint8_t address, bool apply_cs = true);
	void	write_register(uint8_t address, uint8_t value);

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
