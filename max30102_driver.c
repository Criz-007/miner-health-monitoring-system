/**
 * @file max30102_driver.c
 * @brief MAX30102 Pulse Oximeter and Heart Rate Sensor Driver
 */

#include "max30102_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "nrf_log.h"

#define MAX30102_I2C_ADDR       0x57
#define MAX30102_REG_INT_STATUS 0x00
#define MAX30102_REG_INT_ENABLE 0x02
#define MAX30102_REG_FIFO_WR    0x04
#define MAX30102_REG_FIFO_RD    0x06
#define MAX30102_REG_FIFO_DATA  0x07
#define MAX30102_REG_MODE_CFG   0x09
#define MAX30102_REG_SPO2_CFG   0x0A
#define MAX30102_REG_LED1_PA    0x0C
#define MAX30102_REG_LED2_PA    0x0D

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);
static bool m_initialized = false;

/**
 * @brief Write register
 */
static ret_code_t write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return nrf_drv_twi_tx(&m_twi, MAX30102_I2C_ADDR, data, 2, false);
}

/**
 * @brief Read register
 */
static ret_code_t read_register(uint8_t reg, uint8_t *value) {
    ret_code_t err_code;
    err_code = nrf_drv_twi_tx(&m_twi, MAX30102_I2C_ADDR, &reg, 1, true);
    if (err_code != NRF_SUCCESS) return err_code;
    return nrf_drv_twi_rx(&m_twi, MAX30102_I2C_ADDR, value, 1);
}

/**
 * @brief Initialize MAX30102
 */
int max30102_init(void) {
    ret_code_t err_code;
    
    // Initialize TWI
    const nrf_drv_twi_config_t twi_config = {
       .scl                = 27,  // Adjust to your pin
       .sda                = 26,  // Adjust to your pin
       .frequency          = NRF_DRV_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };
    
    err_code = nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    if (err_code != NRF_SUCCESS) {
        return -1;
    }
    nrf_drv_twi_enable(&m_twi);
    
    // Reset sensor
    write_register(MAX30102_REG_MODE_CFG, 0x40);
    nrf_delay_ms(100);
    
    // Configure SpO2 mode
    write_register(MAX30102_REG_MODE_CFG, 0x03); // SpO2 mode
    write_register(MAX30102_REG_SPO2_CFG, 0x27); // ADC range 4096, SR 100Hz, PW 411us
    write_register(MAX30102_REG_LED1_PA, 0x24);  // Red LED current
    write_register(MAX30102_REG_LED2_PA, 0x24);  // IR LED current
    
    m_initialized = true;
    return 0;
}

/**
 * @brief Power on sensor
 */
void max30102_power_on(void) {
    if (m_initialized) {
        write_register(MAX30102_REG_MODE_CFG, 0x03); // SpO2 mode
    }
}

/**
 * @brief Power off sensor
 */
void max30102_power_off(void) {
    if (m_initialized) {
        write_register(MAX30102_REG_MODE_CFG, 0x80); // Shutdown mode
    }
}

/**
 * @brief Read SpO2 and Heart Rate (Simulated for demonstration)
 */
void max30102_read_data(uint8_t *spo2, uint16_t *heart_rate) {
    // In real implementation, read FIFO and process PPG signals
    // For simulation, generate realistic values with some variation
    
    static uint8_t simulation_counter = 0;
    simulation_counter++;
    
    // Simulate normal values with slight variations
    *spo2 = 95 + (simulation_counter % 5); // 95-99%
    *heart_rate = 70 + (simulation_counter % 20); // 70-89 BPM
    
    // Occasionally simulate anomalies for testing
    if (simulation_counter % 50 == 0) {
        *spo2 = 88; // Low SpO2
        NRF_LOG_WARNING("Simulating low SpO2 condition");
    }
    
    if (simulation_counter % 73 == 0) {
        *heart_rate = 125; // High heart rate
        NRF_LOG_WARNING("Simulating high heart rate");
    }
}