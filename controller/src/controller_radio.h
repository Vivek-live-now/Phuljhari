/**
 * @file controller_radio.h
 * @brief ESP-NOW radio driver and non-blocking packet dispatcher for M5StickS3 Controller ✨
 */

#ifndef PHULJHARI_CONTROLLER_RADIO_H
#define PHULJHARI_CONTROLLER_RADIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "phuljhari_protocol.h"
#include "controller_config.h"
#include "controller_state_machine.h"

namespace phuljhari {
namespace controller {

struct ControllerRxItem {
    uint8_t sender_mac[6];
    uint8_t data[PHULJHARI_MAX_FRAME_SIZE];
    uint8_t len;
};

class ControllerRadio {
public:
    ControllerRadio();

    bool begin();
    void set_wireless_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Processes pending outgoing actions from state machine and drains incoming packets.
     */
    void update(ControllerStateMachine& csm, uint32_t current_time_ms);

    void enqueue_rx_packet(const uint8_t* sender_mac, const uint8_t* data, size_t len);
    void on_data_sent(const uint8_t* mac_addr, uint8_t status);

    bool send_broadcast(const uint8_t* data, size_t len);

private:
    static const size_t RX_QUEUE_CAP = 8;
    ControllerRxItem rx_queue_[RX_QUEUE_CAP];
    volatile uint8_t rx_head_;
    volatile uint8_t rx_tail_;

    uint8_t broadcast_mac_[6];
    bool enabled_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_RADIO_H */
