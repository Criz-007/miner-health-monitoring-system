/**
 * @file communication.c
 * @brief Communication module for LoRa/BLE gateway transmission
 */

#include "communication.h"
#include "nrf_log.h"
#include "nrf_delay.h"

static bool m_comm_initialized = false;

/**
 * @brief Initialize communication module
 */
void communication_init(void) {
    // Initialize BLE for local communication
    // Initialize LoRa interface for underground-to-surface
    
    NRF_LOG_INFO("Communication module initialized");
    m_comm_initialized = true;
}

/**
 * @brief Send data packet
 * @param data Pointer to data buffer
 * @param length Data length
 * @param is_emergency Emergency flag for priority transmission
 */
void communication_send_data(uint8_t *data, uint16_t length, bool is_emergency) {
    if (!m_comm_initialized) {
        NRF_LOG_ERROR("Communication not initialized");
        return;
    }
    
    if (is_emergency) {
        NRF_LOG_ERROR("!!! EMERGENCY TRANSMISSION !!!");
        // Use both BLE (nearby) and LoRa (surface) with high priority
    } else {
        NRF_LOG_INFO("Standard data transmission");
    }
    
    // Simulate transmission
    NRF_LOG_INFO("Transmitting %d bytes...", length);
    nrf_delay_ms(100); // Simulate transmission time
    
    NRF_LOG_HEXDUMP_INFO(data, length);
    NRF_LOG_INFO("Transmission successful");
}