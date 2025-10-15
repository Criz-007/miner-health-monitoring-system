/**
 * @file main.c
 * @brief Miner Health Monitoring System - Real-time Vital Signs Monitor
 * @author Criz-007
 * @date 2025-10-15
 * 
 * System Overview:
 * - Monitors SpO2, Heart Rate, ECG, Blood Pressure, Temperature, Fall Detection
 * - Wakes every 35 seconds to check vitals
 * - Triggers emergency alerts on anomaly detection
 * - Power-optimized with deep sleep between measurements
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "nrf_delay.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_rtc.h"
#include "nrf_drv_clock.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// Include sensor drivers
#include "max30102_driver.h"
#include "ads1292r_driver.h"
#include "tmp117_driver.h"
#include "icm42688_driver.h"
#include "communication.h"

// Configuration Constants
#define NORMAL_MONITORING_INTERVAL_MS    35000  // 35 seconds
#define EXTENDED_MONITORING_INTERVAL_MS  10000  // 10 seconds for anomalies
#define EMERGENCY_MONITORING_INTERVAL_MS 5000   // 5 seconds for critical
#define SENSOR_WARMUP_TIME_MS            2000   // Sensor stabilization time

// Health Thresholds
#define SPO2_MIN_NORMAL          92      // Below 92% is concerning
#define SPO2_MIN_CRITICAL        85      // Below 85% is critical
#define HEART_RATE_MIN           45      // BPM
#define HEART_RATE_MAX           120     // BPM (for underground work)
#define HEART_RATE_CRITICAL_MIN  40
#define HEART_RATE_CRITICAL_MAX  150
#define TEMP_MIN_NORMAL          35.5    // Celsius
#define TEMP_MAX_NORMAL          38.5    // Celsius
#define TEMP_CRITICAL_MIN        35.0
#define TEMP_CRITICAL_MAX        40.0
#define BP_SYSTOLIC_MAX          160     // mmHg
#define BP_SYSTOLIC_MIN          90
#define BP_DIASTOLIC_MAX         100
#define BP_DIASTOLIC_MIN         60

// Fall Detection Thresholds
#define FALL_ACCEL_THRESHOLD     2.5     // g-force
#define FALL_IMPACT_THRESHOLD    3.5     // g-force
#define NO_MOVEMENT_TIME_MS      30000   // 30 seconds no movement after fall

// System States
typedef enum {
    STATE_SLEEP,
    STATE_WAKING,
    STATE_MONITORING,
    STATE_EXTENDED_MONITORING,
    STATE_EMERGENCY,
    STATE_TRANSMITTING
} system_state_t;

// Health Status
typedef enum {
    HEALTH_NORMAL,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_EMERGENCY
} health_status_t;

// Vital Signs Structure
typedef struct {
    uint8_t  spo2;              // Blood oxygen saturation (%)
    uint16_t heart_rate;        // Beats per minute
    uint16_t bp_systolic;       // Blood pressure systolic (mmHg)
    uint16_t bp_diastolic;      // Blood pressure diastolic (mmHg)
    float    temperature;       // Body temperature (Celsius)
    float    accel_x;           // Accelerometer X-axis (g)
    float    accel_y;           // Accelerometer Y-axis (g)
    float    accel_z;           // Accelerometer Z-axis (g)
    bool     fall_detected;     // Fall detection flag
    bool     no_movement;       // No movement detected flag
    uint32_t timestamp;         // Measurement timestamp
} vital_signs_t;

// System Context
typedef struct {
    system_state_t   current_state;
    health_status_t  health_status;
    vital_signs_t    vitals;
    uint32_t         monitoring_interval;
    uint8_t          anomaly_count;
    bool             emergency_sent;
    uint32_t         last_measurement_time;
} system_context_t;

// Global system context
static system_context_t g_system_ctx = {
    .current_state = STATE_SLEEP,
    .health_status = HEALTH_NORMAL,
    .monitoring_interval = NORMAL_MONITORING_INTERVAL_MS,
    .anomaly_count = 0,
    .emergency_sent = false,
    .last_measurement_time = 0
};

// Timer instance
APP_TIMER_DEF(m_monitoring_timer);

// Function Prototypes
static void system_init(void);
static void sensors_init(void);
static void sensors_power_on(void);
static void sensors_power_off(void);
static void measure_vitals(vital_signs_t *vitals);
static health_status_t analyze_health(vital_signs_t *vitals);
static void handle_health_status(health_status_t status);
static void enter_sleep_mode(uint32_t duration_ms);
static void monitoring_timer_handler(void *p_context);
static void transmit_data(vital_signs_t *vitals, health_status_t status);
static void log_vitals(vital_signs_t *vitals);

/**
 * @brief Main application entry point
 */
int main(void) {
    // Initialize system
    system_init();
    
    NRF_LOG_INFO("===========================================");
    NRF_LOG_INFO("Miner Health Monitoring System Started");
    NRF_LOG_INFO("Monitoring Interval: %d seconds", NORMAL_MONITORING_INTERVAL_MS / 1000);
    NRF_LOG_INFO("===========================================");
    
    // Start monitoring timer
    ret_code_t err_code = app_timer_start(m_monitoring_timer, 
                                          APP_TIMER_TICKS(g_system_ctx.monitoring_interval), 
                                          NULL);
    APP_ERROR_CHECK(err_code);
    
    // Main loop
    while (true) {
        switch (g_system_ctx.current_state) {
            case STATE_SLEEP:
                // Enter low-power sleep mode
                NRF_LOG_INFO("Entering sleep mode...");
                NRF_LOG_FLUSH();
                enter_sleep_mode(g_system_ctx.monitoring_interval);
                break;
                
            case STATE_WAKING:
                // Wake up sensors
                NRF_LOG_INFO("Waking up sensors...");
                sensors_power_on();
                nrf_delay_ms(SENSOR_WARMUP_TIME_MS);
                g_system_ctx.current_state = STATE_MONITORING;
                break;
                
            case STATE_MONITORING:
            case STATE_EXTENDED_MONITORING:
                // Measure vitals
                NRF_LOG_INFO("Measuring vitals...");
                measure_vitals(&g_system_ctx.vitals);
                log_vitals(&g_system_ctx.vitals);
                
                // Analyze health
                g_system_ctx.health_status = analyze_health(&g_system_ctx.vitals);
                handle_health_status(g_system_ctx.health_status);
                
                // Power off sensors
                sensors_power_off();
                
                // Return to sleep if not emergency
                if (g_system_ctx.current_state != STATE_EMERGENCY) {
                    g_system_ctx.current_state = STATE_SLEEP;
                }
                break;
                
            case STATE_EMERGENCY:
                // Handle emergency
                NRF_LOG_WARNING("EMERGENCY STATE - Critical health issue detected!");
                transmit_data(&g_system_ctx.vitals, g_system_ctx.health_status);
                
                // Continue monitoring at high frequency
                nrf_delay_ms(EMERGENCY_MONITORING_INTERVAL_MS);
                g_system_ctx.current_state = STATE_MONITORING;
                break;
                
            case STATE_TRANSMITTING:
                // Data transmission handled separately
                g_system_ctx.current_state = STATE_SLEEP;
                break;
        }
        
        // Process logs
        NRF_LOG_FLUSH();
        
        // Power management
        nrf_pwr_mgmt_run();
    }
}

/**
 * @brief Initialize system components
 */
static void system_init(void) {
    ret_code_t err_code;
    
    // Initialize logging
    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    
    // Initialize clock
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
    
    // Initialize app timer
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
    
    // Create monitoring timer
    err_code = app_timer_create(&m_monitoring_timer, 
                                APP_TIMER_MODE_REPEATED, 
                                monitoring_timer_handler);
    APP_ERROR_CHECK(err_code);
    
    // Initialize power management
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
    
    // Initialize sensors
    sensors_init();
    
    // Initialize communication
    communication_init();
}

/**
 * @brief Initialize all sensors
 */
static void sensors_init(void) {
    NRF_LOG_INFO("Initializing sensors...");
    
    // Initialize MAX30102 (SpO2 & Heart Rate)
    if (max30102_init() == 0) {
        NRF_LOG_INFO("MAX30102 initialized successfully");
    } else {
        NRF_LOG_ERROR("MAX30102 initialization failed");
    }
    
    // Initialize ADS1292R (ECG & BP)
    if (ads1292r_init() == 0) {
        NRF_LOG_INFO("ADS1292R initialized successfully");
    } else {
        NRF_LOG_ERROR("ADS1292R initialization failed");
    }
    
    // Initialize TMP117 (Temperature)
    if (tmp117_init() == 0) {
        NRF_LOG_INFO("TMP117 initialized successfully");
    } else {
        NRF_LOG_ERROR("TMP117 initialization failed");
    }
    
    // Initialize ICM-42688 (Fall Detection)
    if (icm42688_init() == 0) {
        NRF_LOG_INFO("ICM-42688 initialized successfully");
    } else {
        NRF_LOG_ERROR("ICM-42688 initialization failed");
    }
    
    // Put sensors in low-power mode initially
    sensors_power_off();
}

/**
 * @brief Power on all sensors
 */
static void sensors_power_on(void) {
    max30102_power_on();
    ads1292r_power_on();
    tmp117_wakeup();
    icm42688_wakeup();
}

/**
 * @brief Power off all sensors to save energy
 */
static void sensors_power_off(void) {
    max30102_power_off();
    ads1292r_power_off();
    tmp117_sleep();
    icm42688_sleep();
}

/**
 * @brief Measure all vital signs
 * @param vitals Pointer to vital signs structure
 */
static void measure_vitals(vital_signs_t *vitals) {
    // Clear previous data
    memset(vitals, 0, sizeof(vital_signs_t));
    
    // Timestamp
    vitals->timestamp = app_timer_cnt_get();
    
    // Measure SpO2 and Heart Rate (MAX30102)
    max30102_read_data(&vitals->spo2, &vitals->heart_rate);
    
    // Measure ECG and estimate Blood Pressure (ADS1292R)
    ads1292r_read_ecg_and_bp(&vitals->bp_systolic, &vitals->bp_diastolic);
    
    // Measure Temperature (TMP117)
    vitals->temperature = tmp117_read_temperature();
    
    // Measure Acceleration and detect falls (ICM-42688)
    icm42688_read_accel(&vitals->accel_x, &vitals->accel_y, &vitals->accel_z);
    
    // Fall detection logic
    float accel_magnitude = sqrt(vitals->accel_x * vitals->accel_x + 
                                  vitals->accel_y * vitals->accel_y + 
                                  vitals->accel_z * vitals->accel_z);
    
    if (accel_magnitude > FALL_IMPACT_THRESHOLD) {
        vitals->fall_detected = true;
        NRF_LOG_WARNING("FALL DETECTED! Magnitude: %d.%02d g", 
                        (int)accel_magnitude, 
                        (int)(accel_magnitude * 100) % 100);
    }
    
    // Check for no movement after fall
    if (vitals->fall_detected && accel_magnitude < 0.5) {
        vitals->no_movement = true;
    }
}

/**
 * @brief Analyze health status based on vital signs
 * @param vitals Pointer to vital signs structure
 * @return Health status
 */
static health_status_t analyze_health(vital_signs_t *vitals) {
    health_status_t status = HEALTH_NORMAL;
    uint8_t warning_flags = 0;
    uint8_t critical_flags = 0;
    
    // Check SpO2
    if (vitals->spo2 < SPO2_MIN_CRITICAL) {
        critical_flags++;
        NRF_LOG_ERROR("CRITICAL: SpO2 too low: %d%%", vitals->spo2);
    } else if (vitals->spo2 < SPO2_MIN_NORMAL) {
        warning_flags++;
        NRF_LOG_WARNING("WARNING: SpO2 below normal: %d%%", vitals->spo2);
    }
    
    // Check Heart Rate
    if (vitals->heart_rate < HEART_RATE_CRITICAL_MIN || 
        vitals->heart_rate > HEART_RATE_CRITICAL_MAX) {
        critical_flags++;
        NRF_LOG_ERROR("CRITICAL: Heart rate abnormal: %d BPM", vitals->heart_rate);
    } else if (vitals->heart_rate < HEART_RATE_MIN || 
               vitals->heart_rate > HEART_RATE_MAX) {
        warning_flags++;
        NRF_LOG_WARNING("WARNING: Heart rate outside normal range: %d BPM", vitals->heart_rate);
    }
    
    // Check Temperature
    if (vitals->temperature < TEMP_CRITICAL_MIN || 
        vitals->temperature > TEMP_CRITICAL_MAX) {
        critical_flags++;
        NRF_LOG_ERROR("CRITICAL: Temperature abnormal: %d.%02d°C", 
                     (int)vitals->temperature, 
                     (int)(vitals->temperature * 100) % 100);
    } else if (vitals->temperature < TEMP_MIN_NORMAL || 
               vitals->temperature > TEMP_MAX_NORMAL) {
        warning_flags++;
        NRF_LOG_WARNING("WARNING: Temperature outside normal range");
    }
    
    // Check Blood Pressure
    if (vitals->bp_systolic > BP_SYSTOLIC_MAX || 
        vitals->bp_systolic < BP_SYSTOLIC_MIN) {
        warning_flags++;
        NRF_LOG_WARNING("WARNING: Systolic BP abnormal: %d mmHg", vitals->bp_systolic);
    }
    
    // Check for fall
    if (vitals->fall_detected) {
        if (vitals->no_movement) {
            critical_flags++;
            NRF_LOG_ERROR("EMERGENCY: Fall detected with no movement!");
        } else {
            warning_flags++;
            NRF_LOG_WARNING("WARNING: Fall detected!");
        }
    }
    
    // Determine overall status
    if (critical_flags > 0 || vitals->fall_detected) {
        status = HEALTH_EMERGENCY;
    } else if (warning_flags >= 2) {
        status = HEALTH_CRITICAL;
    } else if (warning_flags > 0) {
        status = HEALTH_WARNING;
    }
    
    return status;
}

/**
 * @brief Handle different health status scenarios
 * @param status Current health status
 */
static void handle_health_status(health_status_t status) {
    switch (status) {
        case HEALTH_NORMAL:
            NRF_LOG_INFO("Health Status: NORMAL");
            g_system_ctx.monitoring_interval = NORMAL_MONITORING_INTERVAL_MS;
            g_system_ctx.anomaly_count = 0;
            g_system_ctx.emergency_sent = false;
            break;
            
        case HEALTH_WARNING:
            NRF_LOG_WARNING("Health Status: WARNING");
            g_system_ctx.anomaly_count++;
            
            if (g_system_ctx.anomaly_count >= 2) {
                // Switch to extended monitoring
                g_system_ctx.monitoring_interval = EXTENDED_MONITORING_INTERVAL_MS;
                g_system_ctx.current_state = STATE_EXTENDED_MONITORING;
                NRF_LOG_INFO("Switching to extended monitoring mode");
            }
            break;
            
        case HEALTH_CRITICAL:
            NRF_LOG_ERROR("Health Status: CRITICAL");
            g_system_ctx.monitoring_interval = EXTENDED_MONITORING_INTERVAL_MS;
            g_system_ctx.current_state = STATE_EXTENDED_MONITORING;
            
            // Send alert but not emergency yet
            transmit_data(&g_system_ctx.vitals, status);
            break;
            
        case HEALTH_EMERGENCY:
            NRF_LOG_ERROR("Health Status: EMERGENCY");
            g_system_ctx.monitoring_interval = EMERGENCY_MONITORING_INTERVAL_MS;
            g_system_ctx.current_state = STATE_EMERGENCY;
            
            // Send emergency alert
            if (!g_system_ctx.emergency_sent) {
                transmit_data(&g_system_ctx.vitals, status);
                g_system_ctx.emergency_sent = true;
            }
            break;
    }
}

/**
 * @brief Enter low-power sleep mode
 * @param duration_ms Sleep duration in milliseconds
 */
static void enter_sleep_mode(uint32_t duration_ms) {
    // Enter System ON idle mode
    __WFE();
    __SEV();
    __WFE();
}

/**
 * @brief Timer handler for monitoring intervals
 * @param p_context Timer context (unused)
 */
static void monitoring_timer_handler(void *p_context) {
    // Wake up system for measurement
    g_system_ctx.current_state = STATE_WAKING;
}

/**
 * @brief Transmit vital data to gateway
 * @param vitals Pointer to vital signs
 * @param status Health status
 */
static void transmit_data(vital_signs_t *vitals, health_status_t status) {
    NRF_LOG_INFO("Transmitting data to gateway...");
    g_system_ctx.current_state = STATE_TRANSMITTING;
    
    // Prepare data packet
    uint8_t data_packet[64];
    uint16_t packet_len = 0;
    
    // Pack data (simplified protocol)
    data_packet[packet_len++] = 0xAA; // Start marker
    data_packet[packet_len++] = status; // Health status
    data_packet[packet_len++] = vitals->spo2;
    data_packet[packet_len++] = (vitals->heart_rate >> 8) & 0xFF;
    data_packet[packet_len++] = vitals->heart_rate & 0xFF;
    data_packet[packet_len++] = (vitals->bp_systolic >> 8) & 0xFF;
    data_packet[packet_len++] = vitals->bp_systolic & 0xFF;
    data_packet[packet_len++] = (vitals->bp_diastolic >> 8) & 0xFF;
    data_packet[packet_len++] = vitals->bp_diastolic & 0xFF;
    
    // Temperature (scaled by 100)
    uint16_t temp_scaled = (uint16_t)(vitals->temperature * 100);
    data_packet[packet_len++] = (temp_scaled >> 8) & 0xFF;
    data_packet[packet_len++] = temp_scaled & 0xFF;
    
    // Fall detection flags
    data_packet[packet_len++] = (vitals->fall_detected << 1) | vitals->no_movement;
    data_packet[packet_len++] = 0x55; // End marker
    
    // Transmit via LoRa/BLE
    communication_send_data(data_packet, packet_len, status == HEALTH_EMERGENCY);
    
    NRF_LOG_INFO("Data transmission complete");
}

/**
 * @brief Log vital signs to console
 * @param vitals Pointer to vital signs
 */
static void log_vitals(vital_signs_t *vitals) {
    NRF_LOG_INFO("========== VITAL SIGNS ==========");
    NRF_LOG_INFO("SpO2: %d%%", vitals->spo2);
    NRF_LOG_INFO("Heart Rate: %d BPM", vitals->heart_rate);
    NRF_LOG_INFO("BP: %d/%d mmHg", vitals->bp_systolic, vitals->bp_diastolic);
    NRF_LOG_INFO("Temperature: %d.%02d°C", 
                 (int)vitals->temperature, 
                 (int)(vitals->temperature * 100) % 100);
    NRF_LOG_INFO("Accel: X=%d.%02d Y=%d.%02d Z=%d.%02d g",
                 (int)vitals->accel_x, (int)(fabs(vitals->accel_x) * 100) % 100,
                 (int)vitals->accel_y, (int)(fabs(vitals->accel_y) * 100) % 100,
                 (int)vitals->accel_z, (int)(fabs(vitals->accel_z) * 100) % 100);
    NRF_LOG_INFO("Fall Detected: %s", vitals->fall_detected ? "YES" : "NO");
    NRF_LOG_INFO("================================");
}