/**
 * @file receiver_radio.h
 * @brief ESP-NOW radio manager and non-blocking packet queue for Project Phuljhari Receiver ✨
 */

#ifndef PHULJHARI_RECEIVER_RADIO_H
#define PHULJHARI_RECEIVER_RADIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "phuljhari_protocol.h"
#include "receiver_config.h"
#include "receiver_state_machine.h"

namespace phuljhari {
namespace receiver {

/**
 * @brief Represents an incoming raw packet copied in the radio callback.
 */
struct RxQueueItem {
    uint8_t sender_mac[6];
    uint8_t data[PHULJHARI_MAX_FRAME_SIZE];
    uint8_t len;
};

class ReceiverRadio {
public:
    ReceiverRadio(ReceiverStateMachine& sm);

    /**
     * @brief Initializes Wi-Fi in Station mode and configures ESP-NOW on the fixed channel.
     * @return true if initialization succeeded.
     */
    bool begin();

    /**
     * @brief Non-blocking dispatch called from Arduino main loop.
     * Drains the incoming packet queue and transmits generated responses.
     */
    void update(uint32_t current_time_ms);

    /**
     * @brief Checks if a specific peer MAC is registered, and registers it if not.
     */
    bool ensure_peer_registered(const uint8_t* peer_mac);

    /**
     * @brief Transmits a raw buffer to a specific peer MAC via ESP-NOW.
     */
    bool send_to_peer(const uint8_t* peer_mac, const uint8_t* data, size_t len);

    /**
     * @brief Enqueues a packet from the radio receive callback into the ring buffer.
     * Guaranteed non-blocking and safe for execution in the Wi-Fi task/ISR.
     */
    void enqueue_rx_packet(const uint8_t* sender_mac, const uint8_t* data, size_t len);

    /**
     * @brief Callback invoked when an ESP-NOW packet transmission completes.
     */
    void on_data_sent(const uint8_t* mac_addr, uint8_t status);

    /* Telemetry */
    uint32_t get_packets_received() const { return packets_received_; }
    uint32_t get_packets_sent() const { return packets_sent_; }
    uint32_t get_queue_overflows() const { return queue_overflows_; }

private:
    ReceiverStateMachine& sm_;

    /* Ring buffer for ISR-to-loop packet decoupling */
    RxQueueItem rx_queue_[PHULJHARI_CONFIG_RX_QUEUE_SIZE];
    volatile uint8_t rx_head_;
    volatile uint8_t rx_tail_;

    uint32_t packets_received_;
    uint32_t packets_sent_;
    uint32_t queue_overflows_;
    protocol::TransportStatus last_tx_status_;
};

} // namespace receiver
} // namespace phuljhari

#endif /* PHULJHARI_RECEIVER_RADIO_H */
