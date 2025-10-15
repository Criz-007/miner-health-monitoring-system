/**
 * @file icm42688_driver.c
 * @brief ICM-42688-P 6-Axis IMU Driver for Fall Detection
 * @description High-performance 6-axis accelerometer and gyroscope
 */

#include "icm42688_driver.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include <math.h>

// ICM-42688 Pin Definitions
#define ICM42688_CS_PIN       20
#define ICM42688_INT1_PIN     21

// ICM-42688 Register Bank 0
#define ICM42688_REG_DEVICE_CONFIG      0x11
#define ICM42688_REG_DRIVE_CONFIG       0x13
#define ICM42688_REG_INT_CONFIG         0x14
#define ICM42688_REG_PWR_MGMT0          0x4E
#define ICM42688_REG_GYRO_CONFIG0       0x4F
#define ICM42688_REG_ACCEL_CONFIG0      0x50
#define ICM42688_REG_GYRO_CONFIG1       0x51
#define ICM42688_REG_GYRO_ACCEL_CONFIG0 0x52
#define ICM42688_REG_ACCEL_CONFIG1      0x53
#define ICM42688_REG_INT_CONFIG0        0x63
#define ICM42688_REG_INT_CONFIG1        0x64
#define ICM42688_REG_INT_SOURCE0        0x65
#define ICM42688_REG_WHO_AM_I           0x75
#define ICM42688_REG_SIGNAL_PATH_RESET  0x4B

// Data registers
#define ICM42688_REG_TEMP_DATA1         0x1D
#define ICM42688_REG_ACCEL_DATA_X1      0x1F
#define ICM42688_REG_ACCEL_DATA_X0      0x20
#define ICM42688_REG_ACCEL_DATA_Y1      0x21
#define ICM42688_REG_ACCEL_DATA_Y0      0x22
#define ICM42688_REG_ACCEL_DATA_Z1      0x23
#define ICM42688_REG_ACCEL_DATA_Z0      0x24
#define ICM42688_REG_GYRO_DATA_X1       0x25

// WHO_AM_I value
#define ICM42688_WHO_AM_I_VALUE         0x47

// Power management
#define ICM42688_PWR_MGMT0_TEMP_DIS     (1 << 5)
#define ICM42688_PWR_MGMT0_IDLE         (1 << 4)
#define ICM42688_PWR_MGMT0_GYRO_MODE_LN (3 << 2)
#define ICM42688_PWR_MGMT0_ACCEL_MODE_LN (3 << 0)

// Accelerometer full-scale ranges
#define ICM42688_ACCEL_FS_2G            0
#define ICM42688_ACCEL_FS_4G            1
#define ICM42688_ACCEL_FS_8G            2
#define ICM42688_ACCEL_FS_16G           3

// Accelerometer sensitivity (LSB/g) for ±16g
#define ICM42688_ACCEL_SENSITIVITY_16G  2048.0

static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(2);
static bool m_initialized = false;
static float accel_scale = 1.0 / ICM42688_ACCEL_SENSITIVITY_16G;

/**
 * @brief Write register
 */
static void icm42688_write_register(uint8_t reg, uint8_t value) {
    uint8_t tx_data[2];
    tx_data[0] = reg & 0x7F; // Write bit = 0
    tx_data[1] = value;
    
    nrf_gpio_pin_clear(ICM42688_CS_PIN);
    nrf_drv_spi_transfer(&m_spi, tx_data, 2, NULL, 0);
    nrf_gpio_pin_set(ICM42688_CS_PIN);
    nrf_delay_us(10);
}

/**
 * @brief Read register
 */
static uint8_t icm42688_read_register(uint8_t reg) {
    uint8_t tx_data[2];
    uint8_t rx_data[2];
    
    tx_data[0] = reg | 0x80; // Read bit = 1
    tx_data[1] = 0x00; // Dummy byte
    
    nrf_gpio_pin_clear(ICM42688_CS_PIN);
    nrf_drv_spi_transfer(&m_spi, tx_data, 2, rx_data, 2);
    nrf_gpio_pin_set(ICM42688_CS_PIN);
    
    return rx_data[1];
}

/**
 * @brief Read multiple registers
 */
static void icm42688_read_registers(uint8_t reg, uint8_t *buffer, uint8_t length) {
    uint8_t tx_data = reg | 0x80; // Read bit = 1
    
    nrf_gpio_pin_clear(ICM42688_CS_PIN);
    nrf_drv_spi_transfer(&m_spi, &tx_data, 1, NULL, 0);
    nrf_drv_spi_transfer(&m_spi, NULL, 0, buffer, length);
    nrf_gpio_pin_set(ICM42688_CS_PIN);
}

/**
 * @brief Initialize ICM-42688
 */
int icm42688_init(void) {
    ret_code_t err_code;
    
    // Configure CS pin
    nrf_gpio_cfg_output(ICM42688_CS_PIN);
    nrf_gpio_pin_set(ICM42688_CS_PIN);
    
    // Configure INT1 pin
    nrf_gpio_cfg_input(ICM42688_INT1_PIN, NRF_GPIO_PIN_PULLUP);
    
    // Initialize SPI
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.miso_pin = 8;   // Adjust to your circuit
    spi_config.mosi_pin = 9;   // Adjust to your circuit
    spi_config.sck_pin  = 10;  // Adjust to your circuit
    spi_config.frequency = NRF_DRV_SPI_FREQ_8M;
    spi_config.mode = NRF_DRV_SPI_MODE_0; // CPOL=0, CPHA=0
    
    err_code = nrf_drv_spi_init(&m_spi, &spi_config, NULL, NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("ICM42688 SPI init failed: %d", err_code);
        return -1;
    }
    
    nrf_delay_ms(100); // Power-up time
    
    // Read WHO_AM_I
    uint8_t who_am_i = icm42688_read_register(ICM42688_REG_WHO_AM_I);
    NRF_LOG_INFO("ICM-42688 WHO_AM_I: 0x%02X", who_am_i);
    
    if (who_am_i != ICM42688_WHO_AM_I_VALUE) {
        NRF_LOG_ERROR("ICM-42688 WHO_AM_I mismatch");
        return -1;
    }
    
    // Soft reset
    icm42688_write_register(ICM42688_REG_DEVICE_CONFIG, 0x01);
    nrf_delay_ms(100);
    
    // Configure power management: Accelerometer and Gyro in Low Noise mode
    icm42688_write_register(ICM42688_REG_PWR_MGMT0, 
                            ICM42688_PWR_MGMT0_ACCEL_MODE_LN | 
                            ICM42688_PWR_MGMT0_GYRO_MODE_LN);
    nrf_delay_ms(50);
    
    // Configure accelerometer: ±16g, 100Hz ODR
    icm42688_write_register(ICM42688_REG_ACCEL_CONFIG0, 
                            (ICM42688_ACCEL_FS_16G << 5) | 0x05); // 100Hz
    
    // Configure gyro: ±2000 dps, 100Hz ODR
    icm42688_write_register(ICM42688_REG_GYRO_CONFIG0, (3 << 5) | 0x05);
    
    // Set accel scale based on ±16g
    accel_scale = 16.0 / 32768.0;
    
    m_initialized = true;
    NRF_LOG_INFO("ICM-42688 initialized successfully");
    
    return 0;
}

/**
 * @brief Wake up ICM-42688
 */
void icm42688_wakeup(void) {
    if (m_initialized) {
        icm42688_write_register(ICM42688_REG_PWR_MGMT0, 
                                ICM42688_PWR_MGMT0_ACCEL_MODE_LN | 
                                ICM42688_PWR_MGMT0_GYRO_MODE_LN);
        nrf_delay_ms(50);
    }
}

/**
 * @brief Put ICM-42688 to sleep
 */
void icm42688_sleep(void) {
    if (m_initialized) {
        icm42688_write_register(ICM42688_REG_PWR_MGMT0, 0x00); // All sensors off
    }
}

/**
 * @brief Read accelerometer data
 */
void icm42688_read_accel(float *accel_x, float *accel_y, float *accel_z) {
    if (!m_initialized) {
        *accel_x = 0.0;
        *accel_y = 0.0;
        *accel_z = 1.0; // 1g in Z (standing)
        return;
    }
    
    uint8_t data[6];
    icm42688_read_registers(ICM42688_REG_ACCEL_DATA_X1, data, 6);
    
    // Combine bytes (big-endian)
    int16_t raw_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t raw_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t raw_z = (int16_t)((data[4] << 8) | data[5]);
    
    // Convert to g
    *accel_x = raw_x * accel_scale;
    *accel_y = raw_y * accel_scale;
    *accel_z = raw_z * accel_scale;
}

/**
 * @brief Read gyroscope data
 */
void icm42688_read_gyro(float *gyro_x, float *gyro_y, float *gyro_z) {
    if (!m_initialized) {
        *gyro_x = 0.0;
        *gyro_y = 0.0;
        *gyro_z = 0.0;
        return;
    }
    
    uint8_t data[6];
    icm42688_read_registers(ICM42688_REG_GYRO_DATA_X1, data, 6);
    
    // Combine bytes
    int16_t raw_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t raw_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t raw_z = (int16_t)((data[4] << 8) | data[5]);
    
    // Convert to dps (±2000 dps range)
    float gyro_scale = 2000.0 / 32768.0;
    *gyro_x = raw_x * gyro_scale;
    *gyro_y = raw_y * gyro_scale;
    *gyro_z = raw_z * gyro_scale;
}

/**
 * @brief Detect fall using accelerometer data
 * @return true if fall detected, false otherwise
 */
bool icm42688_detect_fall(void) {
    float ax, ay, az;
    icm42688_read_accel(&ax, &ay, &az);
    
    // Calculate acceleration magnitude
    float magnitude = sqrtf(ax*ax + ay*ay + az*az);
    
    // Free fall detection: magnitude < 0.5g for >100ms
    // Impact detection: magnitude > 3.5g
    
    static uint8_t freefall_count = 0;
    static bool in_freefall = false;
    
    if (magnitude < 0.5) {
        freefall_count++;
        if (freefall_count > 10) { // 100ms at 100Hz
            in_freefall = true;
        }
    } else if (magnitude > 3.5 && in_freefall) {
        // Impact after freefall = FALL DETECTED
        NRF_LOG_WARNING("FALL DETECTED! Impact: %.2f g", magnitude);
        freefall_count = 0;
        in_freefall = false;
        return true;
    } else {
        freefall_count = 0;
        in_freefall = false;
    }
    
    return false;
}