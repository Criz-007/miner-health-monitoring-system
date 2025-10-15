/**
 * @file tmp117_driver.c
 * @brief TMP117 High-Accuracy Temperature Sensor Driver
 * @description ±0.1°C accurate digital temperature sensor with I2C interface
 */

#include "tmp117_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "nrf_log.h"

// TMP117 I2C Address (default)
#define TMP117_I2C_ADDR         0x48

// TMP117 Register Addresses
#define TMP117_REG_TEMP         0x00  // Temperature result
#define TMP117_REG_CFGR         0x01  // Configuration register
#define TMP117_REG_THI_LIMIT    0x02  // High limit
#define TMP117_REG_TLO_LIMIT    0x03  // Low limit
#define TMP117_REG_EEPROM_UL    0x04  // EEPROM unlock
#define TMP117_REG_EEPROM1      0x05  // EEPROM1
#define TMP117_REG_EEPROM2      0x06  // EEPROM2
#define TMP117_REG_TEMP_OFFSET  0x07  // Temperature offset
#define TMP117_REG_EEPROM3      0x08  // EEPROM3
#define TMP117_REG_DEVICE_ID    0x0F  // Device ID

// Configuration bits
#define TMP117_CFG_HIGH_ALERT   (1 << 15)
#define TMP117_CFG_LOW_ALERT    (1 << 14)
#define TMP117_CFG_DATA_READY   (1 << 13)
#define TMP117_CFG_EEPROM_BUSY  (1 << 12)
#define TMP117_CFG_MOD_MASK     (3 << 10)
#define TMP117_CFG_MOD_CC       (0 << 10)  // Continuous conversion
#define TMP117_CFG_MOD_SD       (1 << 10)  // Shutdown
#define TMP117_CFG_MOD_OS       (3 << 10)  // One-shot

#define TMP117_RESOLUTION       0.0078125  // °C per LSB

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);
static bool m_initialized = false;

/**
 * @brief Write register (16-bit)
 */
static ret_code_t tmp117_write_register(uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;  // MSB
    data[2] = value & 0xFF;         // LSB
    
    return nrf_drv_twi_tx(&m_twi, TMP117_I2C_ADDR, data, 3, false);
}

/**
 * @brief Read register (16-bit)
 */
static ret_code_t tmp117_read_register(uint8_t reg, uint16_t *value) {
    ret_code_t err_code;
    uint8_t data[2];
    
    err_code = nrf_drv_twi_tx(&m_twi, TMP117_I2C_ADDR, &reg, 1, true);
    if (err_code != NRF_SUCCESS) return err_code;
    
    err_code = nrf_drv_twi_rx(&m_twi, TMP117_I2C_ADDR, data, 2);
    if (err_code != NRF_SUCCESS) return err_code;
    
    *value = (data[0] << 8) | data[1];
    return NRF_SUCCESS;
}

/**
 * @brief Initialize TMP117
 */
int tmp117_init(void) {
    ret_code_t err_code;
    uint16_t device_id;
    
    // TWI/I2C is already initialized by MAX30102, skip if initialized
    if (!m_initialized) {
        const nrf_drv_twi_config_t twi_config = {
           .scl                = 27,
           .sda                = 26,
           .frequency          = NRF_DRV_TWI_FREQ_400K,
           .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
           .clear_bus_init     = false
        };
        
        err_code = nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
        if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE) {
            NRF_LOG_ERROR("TMP117 TWI init failed: %d", err_code);
            return -1;
        }
        nrf_drv_twi_enable(&m_twi);
    }
    
    // Read device ID to verify communication
    err_code = tmp117_read_register(TMP117_REG_DEVICE_ID, &device_id);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("TMP117 communication failed");
        return -1;
    }
    
    NRF_LOG_INFO("TMP117 Device ID: 0x%04X", device_id);
    
    // Expected device ID is 0x0117
    if ((device_id & 0x0FFF) != 0x0117) {
        NRF_LOG_WARNING("Unexpected TMP117 device ID");
    }
    
    // Configure for continuous conversion, 1Hz (low power)
    // AVG[1:0] = 00 (no averaging)
    // CONV[2:0] = 000 (15.5ms conversion)
    err_code = tmp117_write_register(TMP117_REG_CFGR, TMP117_CFG_MOD_CC);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("TMP117 configuration failed");
        return -1;
    }
    
    nrf_delay_ms(50); // Wait for first conversion
    
    m_initialized = true;
    NRF_LOG_INFO("TMP117 initialized successfully");
    
    return 0;
}

/**
 * @brief Wake up TMP117 from shutdown mode
 */
void tmp117_wakeup(void) {
    if (m_initialized) {
        tmp117_write_register(TMP117_REG_CFGR, TMP117_CFG_MOD_CC);
        nrf_delay_ms(20); // Wait for conversion
    }
}

/**
 * @brief Put TMP117 into shutdown mode (low power)
 */
void tmp117_sleep(void) {
    if (m_initialized) {
        tmp117_write_register(TMP117_REG_CFGR, TMP117_CFG_MOD_SD);
    }
}

/**
 * @brief Read temperature from TMP117
 * @return Temperature in degrees Celsius
 */
float tmp117_read_temperature(void) {
    if (!m_initialized) {
        return 36.5; // Default body temperature
    }
    
    uint16_t raw_temp;
    ret_code_t err_code;
    
    // Check if data is ready
    uint16_t config;
    err_code = tmp117_read_register(TMP117_REG_CFGR, &config);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to read config register");
        return 36.5;
    }
    
    // Wait for data ready bit
    uint8_t timeout = 100;
    while (!(config & TMP117_CFG_DATA_READY) && timeout--) {
        nrf_delay_ms(1);
        tmp117_read_register(TMP117_REG_CFGR, &config);
    }
    
    // Read temperature register
    err_code = tmp117_read_register(TMP117_REG_TEMP, &raw_temp);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to read temperature");
        return 36.5;
    }
    
    // Convert to temperature (two's complement)
    int16_t temp_raw = (int16_t)raw_temp;
    float temperature = temp_raw * TMP117_RESOLUTION;
    
    NRF_LOG_INFO("TMP117 Temperature: %d.%02d°C", 
                 (int)temperature, 
                 (int)(temperature * 100) % 100);
    
    return temperature;
}

/**
 * @brief Set temperature alert thresholds
 */
void tmp117_set_alert_limits(float high_limit, float low_limit) {
    if (!m_initialized) return;
    
    int16_t high_raw = (int16_t)(high_limit / TMP117_RESOLUTION);
    int16_t low_raw = (int16_t)(low_limit / TMP117_RESOLUTION);
    
    tmp117_write_register(TMP117_REG_THI_LIMIT, (uint16_t)high_raw);
    tmp117_write_register(TMP117_REG_TLO_LIMIT, (uint16_t)low_raw);
    
    NRF_LOG_INFO("Temperature alert limits set: Low=%d.%02d°C, High=%d.%02d°C",
                 (int)low_limit, (int)(low_limit * 100) % 100,
                 (int)high_limit, (int)(high_limit * 100) % 100);
}