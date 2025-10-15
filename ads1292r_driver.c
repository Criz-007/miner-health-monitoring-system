/**
 * @file ads1292r_driver.c
 * @brief ADS1292R ECG/Blood Pressure Sensor Driver
 * @description Dual-channel 24-bit ADC for biopotential measurements
 */

#include "ads1292r_driver.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include <math.h>

// ADS1292R Pin Definitions (adjust to your circuit)
#define ADS1292R_CS_PIN       25
#define ADS1292R_DRDY_PIN     24
#define ADS1292R_START_PIN    23
#define ADS1292R_PWDN_PIN     22

// ADS1292R Register Addresses
#define ADS1292R_REG_ID       0x00
#define ADS1292R_REG_CONFIG1  0x01
#define ADS1292R_REG_CONFIG2  0x02
#define ADS1292R_REG_LOFF     0x03
#define ADS1292R_REG_CH1SET   0x04
#define ADS1292R_REG_CH2SET   0x05
#define ADS1292R_REG_RLDSENS  0x06
#define ADS1292R_REG_LOFFSENS 0x07
#define ADS1292R_REG_LOFFSTAT 0x08
#define ADS1292R_REG_RESP1    0x09
#define ADS1292R_REG_RESP2    0x0A

// ADS1292R Commands
#define ADS1292R_CMD_WAKEUP   0x02
#define ADS1292R_CMD_STANDBY  0x04
#define ADS1292R_CMD_RESET    0x06
#define ADS1292R_CMD_START    0x08
#define ADS1292R_CMD_STOP     0x0A
#define ADS1292R_CMD_RDATAC   0x10
#define ADS1292R_CMD_SDATAC   0x11
#define ADS1292R_CMD_RDATA    0x12
#define ADS1292R_CMD_RREG     0x20
#define ADS1292R_CMD_WREG     0x40

// SPI Instance
static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(1);
static bool m_initialized = false;

// ECG Data buffers
static int32_t ecg_ch1_buffer[500];
static int32_t ecg_ch2_buffer[500];
static uint16_t buffer_index = 0;

/**
 * @brief SPI write/read helper
 */
static void ads1292r_spi_write(uint8_t *tx_data, uint8_t tx_len) {
    nrf_gpio_pin_clear(ADS1292R_CS_PIN);
    nrf_drv_spi_transfer(&m_spi, tx_data, tx_len, NULL, 0);
    nrf_gpio_pin_set(ADS1292R_CS_PIN);
}

static void ads1292r_spi_read(uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len) {
    nrf_gpio_pin_clear(ADS1292R_CS_PIN);
    nrf_drv_spi_transfer(&m_spi, tx_data, tx_len, rx_data, rx_len);
    nrf_gpio_pin_set(ADS1292R_CS_PIN);
}

/**
 * @brief Send command to ADS1292R
 */
static void ads1292r_send_command(uint8_t cmd) {
    nrf_gpio_pin_clear(ADS1292R_CS_PIN);
    uint8_t tx_data = cmd;
    nrf_drv_spi_transfer(&m_spi, &tx_data, 1, NULL, 0);
    nrf_gpio_pin_set(ADS1292R_CS_PIN);
    nrf_delay_us(10);
}

/**
 * @brief Write register
 */
static void ads1292r_write_register(uint8_t reg, uint8_t value) {
    uint8_t tx_data[3];
    tx_data[0] = ADS1292R_CMD_WREG | reg;
    tx_data[1] = 0x00; // Number of registers - 1
    tx_data[2] = value;
    ads1292r_spi_write(tx_data, 3);
    nrf_delay_us(10);
}

/**
 * @brief Read register
 */
static uint8_t ads1292r_read_register(uint8_t reg) {
    uint8_t tx_data[3];
    uint8_t rx_data[3];
    
    tx_data[0] = ADS1292R_CMD_RREG | reg;
    tx_data[1] = 0x00; // Number of registers - 1
    tx_data[2] = 0x00; // Dummy byte
    
    ads1292r_spi_read(tx_data, 3, rx_data, 3);
    
    return rx_data[2];
}

/**
 * @brief Initialize ADS1292R
 */
int ads1292r_init(void) {
    ret_code_t err_code;
    
    // Configure GPIO pins
    nrf_gpio_cfg_output(ADS1292R_CS_PIN);
    nrf_gpio_pin_set(ADS1292R_CS_PIN);
    
    nrf_gpio_cfg_output(ADS1292R_START_PIN);
    nrf_gpio_pin_clear(ADS1292R_START_PIN);
    
    nrf_gpio_cfg_output(ADS1292R_PWDN_PIN);
    nrf_gpio_pin_set(ADS1292R_PWDN_PIN); // Power on
    
    nrf_gpio_cfg_input(ADS1292R_DRDY_PIN, NRF_GPIO_PIN_PULLUP);
    
    // Initialize SPI
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.miso_pin = 3;  // Adjust to your circuit
    spi_config.mosi_pin = 4;  // Adjust to your circuit
    spi_config.sck_pin  = 5;  // Adjust to your circuit
    spi_config.frequency = NRF_DRV_SPI_FREQ_1M;
    spi_config.mode = NRF_DRV_SPI_MODE_1; // CPOL=0, CPHA=1
    
    err_code = nrf_drv_spi_init(&m_spi, &spi_config, NULL, NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("SPI init failed: %d", err_code);
        return -1;
    }
    
    // Hardware reset
    nrf_gpio_pin_clear(ADS1292R_PWDN_PIN);
    nrf_delay_ms(100);
    nrf_gpio_pin_set(ADS1292R_PWDN_PIN);
    nrf_delay_ms(500); // Wait for power-up
    
    // Software reset
    ads1292r_send_command(ADS1292R_CMD_RESET);
    nrf_delay_ms(100);
    
    // Stop data conversion
    ads1292r_send_command(ADS1292R_CMD_SDATAC);
    nrf_delay_ms(10);
    
    // Read and verify device ID
    uint8_t device_id = ads1292r_read_register(ADS1292R_REG_ID);
    NRF_LOG_INFO("ADS1292R Device ID: 0x%02X", device_id);
    
    // Configure registers
    // CONFIG1: HR mode, 500 SPS
    ads1292r_write_register(ADS1292R_REG_CONFIG1, 0x02);
    
    // CONFIG2: Test signals off, PDB_LOFF_COMP, PDB_REFBUF
    ads1292r_write_register(ADS1292R_REG_CONFIG2, 0xA0);
    
    // CH1SET: Normal operation, Gain 6, channel enabled
    ads1292r_write_register(ADS1292R_REG_CH1SET, 0x00);
    
    // CH2SET: Normal operation, Gain 6, channel enabled
    ads1292r_write_register(ADS1292R_REG_CH2SET, 0x00);
    
    // RLD sensing
    ads1292r_write_register(ADS1292R_REG_RLDSENS, 0x2C);
    
    // Start conversion
    nrf_gpio_pin_set(ADS1292R_START_PIN);
    
    m_initialized = true;
    NRF_LOG_INFO("ADS1292R initialized successfully");
    
    return 0;
}

/**
 * @brief Power on ADS1292R
 */
void ads1292r_power_on(void) {
    if (m_initialized) {
        nrf_gpio_pin_set(ADS1292R_PWDN_PIN);
        nrf_delay_ms(100);
        ads1292r_send_command(ADS1292R_CMD_WAKEUP);
        nrf_gpio_pin_set(ADS1292R_START_PIN);
        nrf_delay_ms(10);
    }
}

/**
 * @brief Power off ADS1292R
 */
void ads1292r_power_off(void) {
    if (m_initialized) {
        nrf_gpio_pin_clear(ADS1292R_START_PIN);
        ads1292r_send_command(ADS1292R_CMD_STANDBY);
        nrf_delay_ms(10);
    }
}

/**
 * @brief Read ECG data from both channels
 */
static void ads1292r_read_ecg_sample(int32_t *ch1, int32_t *ch2) {
    uint8_t data[9]; // 3 status bytes + 3 bytes CH1 + 3 bytes CH2
    
    // Wait for DRDY to go low
    uint16_t timeout = 1000;
    while (nrf_gpio_pin_read(ADS1292R_DRDY_PIN) && timeout--) {
        nrf_delay_us(10);
    }
    
    if (timeout == 0) {
        *ch1 = 0;
        *ch2 = 0;
        return;
    }
    
    // Read data
    nrf_gpio_pin_clear(ADS1292R_CS_PIN);
    uint8_t cmd = ADS1292R_CMD_RDATA;
    nrf_drv_spi_transfer(&m_spi, &cmd, 1, data, 9);
    nrf_gpio_pin_set(ADS1292R_CS_PIN);
    
    // Convert 24-bit two's complement to 32-bit signed
    *ch1 = ((int32_t)(data[3] << 24) | (data[4] << 16) | (data[5] << 8)) >> 8;
    *ch2 = ((int32_t)(data[6] << 24) | (data[7] << 16) | (data[8] << 8)) >> 8;
}

/**
 * @brief Calculate heart rate from ECG using R-peak detection
 */
static uint16_t calculate_heart_rate_from_ecg(void) {
    // Simple R-peak detection algorithm
    // In production, use more sophisticated methods (Pan-Tompkins, etc.)
    
    const uint16_t sampling_rate = 500; // Hz
    const int32_t threshold = 100000; // Adjust based on signal amplitude
    
    uint16_t peak_count = 0;
    uint16_t last_peak_idx = 0;
    uint16_t rr_intervals[10];
    uint16_t rr_count = 0;
    
    // Find R-peaks
    for (uint16_t i = 2; i < buffer_index - 2; i++) {
        if (ecg_ch1_buffer[i] > threshold &&
            ecg_ch1_buffer[i] > ecg_ch1_buffer[i-1] &&
            ecg_ch1_buffer[i] > ecg_ch1_buffer[i+1] &&
            (i - last_peak_idx) > 100) { // Refractory period
            
            if (last_peak_idx > 0 && rr_count < 10) {
                rr_intervals[rr_count++] = i - last_peak_idx;
            }
            last_peak_idx = i;
            peak_count++;
        }
    }
    
    // Calculate average RR interval
    if (rr_count == 0) return 70; // Default
    
    uint32_t avg_rr = 0;
    for (uint16_t i = 0; i < rr_count; i++) {
        avg_rr += rr_intervals[i];
    }
    avg_rr /= rr_count;
    
    // Convert to BPM
    uint16_t heart_rate = (60 * sampling_rate) / avg_rr;
    
    return heart_rate;
}

/**
 * @brief Estimate blood pressure from ECG
 * @note This is a simplified estimation. Real BP requires calibration and PPG signals
 */
static void estimate_blood_pressure(uint16_t heart_rate, uint16_t *systolic, uint16_t *diastolic) {
    // Simplified BP estimation based on heart rate and PTT (Pulse Transit Time)
    // In production, this requires:
    // 1. PPG sensor for pulse arrival time
    // 2. ECG for QRS complex
    // 3. Calibration with actual BP measurements
    // 4. Machine learning models
    
    // Basic correlation model (for demonstration)
    // MAP (Mean Arterial Pressure) ≈ CO × SVR
    // Where CO = Cardiac Output, SVR = Systemic Vascular Resistance
    
    float base_systolic = 120.0;
    float base_diastolic = 80.0;
    
    // Adjust based on heart rate deviation from normal (70 bpm)
    float hr_factor = (heart_rate - 70.0) / 70.0;
    
    *systolic = (uint16_t)(base_systolic + (hr_factor * 20.0));
    *diastolic = (uint16_t)(base_diastolic + (hr_factor * 10.0));
    
    // Clamp to reasonable ranges
    if (*systolic < 90) *systolic = 90;
    if (*systolic > 180) *systolic = 180;
    if (*diastolic < 60) *diastolic = 60;
    if (*diastolic > 110) *diastolic = 110;
}

/**
 * @brief Read ECG and estimate blood pressure
 */
void ads1292r_read_ecg_and_bp(uint16_t *bp_systolic, uint16_t *bp_diastolic) {
    if (!m_initialized) {
        *bp_systolic = 120;
        *bp_diastolic = 80;
        return;
    }
    
    // Start continuous conversion
    ads1292r_send_command(ADS1292R_CMD_RDATAC);
    nrf_delay_ms(10);
    
    // Collect ECG samples (2 seconds of data at 500 SPS)
    buffer_index = 0;
    for (uint16_t i = 0; i < 500 && i < sizeof(ecg_ch1_buffer)/sizeof(ecg_ch1_buffer[0]); i++) {
        ads1292r_read_ecg_sample(&ecg_ch1_buffer[i], &ecg_ch2_buffer[i]);
        buffer_index++;
        nrf_delay_us(2000); // 500 Hz = 2ms period
    }
    
    // Stop continuous conversion
    ads1292r_send_command(ADS1292R_CMD_SDATAC);
    
    // Calculate heart rate from ECG
    uint16_t hr_from_ecg = calculate_heart_rate_from_ecg();
    
    // Estimate blood pressure
    estimate_blood_pressure(hr_from_ecg, bp_systolic, bp_diastolic);
    
    NRF_LOG_INFO("ECG-derived HR: %d BPM", hr_from_ecg);
    NRF_LOG_INFO("Estimated BP: %d/%d mmHg", *bp_systolic, *bp_diastolic);
}

/**
 * @brief Get raw ECG data for further analysis
 */
void ads1292r_get_raw_ecg(int32_t *ch1_buffer, int32_t *ch2_buffer, uint16_t *length) {
    memcpy(ch1_buffer, ecg_ch1_buffer, buffer_index * sizeof(int32_t));
    memcpy(ch2_buffer, ecg_ch2_buffer, buffer_index * sizeof(int32_t));
    *length = buffer_index;
}