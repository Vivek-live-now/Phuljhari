/**
 * @file controller_radio.cpp
 * @brief Implementation of ESP-NOW radio driver for M5StickS3 Controller ✨
 */

#include "controller_radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include <cstring>

namespace phuljhari {
namespace controller {

static ControllerRadio* s_ctrl_radio = nullptr;

#if defined(ESP_ARDUINO_VERSION_VAL) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static void ctrl_espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (s_ctrl_radio != nullptr && info != nullptr && data != nullptr) {
        s_ctrl_radio->enqueue_rx_packet(info->src_addr, data, (size_t)len);
    }
}

static void ctrl_espnow_send_cb(const wifi_tx_info_t * /*info*/, esp_now_send_status_t status) {
    if (s_ctrl_radio != nullptr) {
        s_ctrl_radio->on_data_sent(nullptr, (uint8_t)status);
    }
}
#else
static void ctrl_espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
    if (s_ctrl_radio != nullptr && mac != nullptr && data != nullptr) {
        s_ctrl_radio->enqueue_rx_packet(mac, data, (size_t)len);
    }
}

static void ctrl_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (s_ctrl_radio != nullptr) {
        s_ctrl_radio->on_data_sent(mac_addr, (uint8_t)status);
    }
}
#endif

ControllerRadio::ControllerRadio()
    : rx_head_(0),
      rx_tail_(0),
      enabled_(true) {
    s_ctrl_radio = this;
    std::memset(broadcast_mac_, 0xFF, 6);
    std::memset(rx_queue_, 0, sizeof(rx_queue_));
}

bool ControllerRadio::begin() {
    Serial.println("[RADIO] Initializing M5StickS3 ESP-NOW Controller...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // ESP-NOW requires continuous RX listening without AP beacons
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Limit Wi-Fi TX power to +11 dBm (~13 mW) to prevent battery voltage sag
    // and reduce peak transmission current by ~50%
    WiFi.setTxPower(WIFI_POWER_11dBm);

    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) {
        esp_wifi_set_channel(PHULJHARI_RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("[RADIO] ERROR: Controller ESP-NOW Init Failed!");
        return false;
    }

    esp_now_register_send_cb(ctrl_espnow_send_cb);
    esp_now_register_recv_cb(ctrl_espnow_recv_cb);

    // Register Broadcast Peer
    esp_now_peer_info_t peerInfo;
    std::memset(&peerInfo, 0, sizeof(peerInfo));
    std::memcpy(peerInfo.peer_addr, broadcast_mac_, 6);
    peerInfo.channel = PHULJHARI_RADIO_CHANNEL;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(broadcast_mac_)) {
        esp_now_add_peer(&peerInfo);
    }

    enabled_ = true;
    Serial.println("[RADIO] Controller ESP-NOW ready (Modem-Sleep + 11dBm TX).");
    return true;
}

void ControllerRadio::set_wireless_enabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (!enabled_) {
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        Serial.println("[RADIO] Wi-Fi powered OFF for low-power operation.");
    } else {
        begin();
        Serial.println("[RADIO] Wi-Fi restored.");
    }
}

void ControllerRadio::enqueue_rx_packet(const uint8_t* sender_mac, const uint8_t* data, size_t len) {
    if (sender_mac == nullptr || data == nullptr || len == 0 || len > PHULJHARI_MAX_FRAME_SIZE) {
        return;
    }

    uint8_t next_head = (rx_head_ + 1) % RX_QUEUE_CAP;
    if (next_head == rx_tail_) {
        return; // Drop if queue full
    }

    ControllerRxItem& item = rx_queue_[rx_head_];
    std::memcpy(item.sender_mac, sender_mac, 6);
    std::memcpy(item.data, data, len);
    item.len = (uint8_t)len;

    rx_head_ = next_head;
}

void ControllerRadio::on_data_sent(const uint8_t* /*mac_addr*/, uint8_t /*status*/) {
    // Non-blocking MAC send status callback
}

bool ControllerRadio::send_broadcast(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return false;
    esp_err_t result = esp_now_send(broadcast_mac_, data, len);
    return (result == ESP_OK);
}

void ControllerRadio::update(ControllerStateMachine& csm, uint32_t current_time_ms) {
    // 1. Dispatch pending outgoing actions from state machine
    if (csm.has_pending_action()) {
        OutgoingAction act = csm.pop_pending_action();
        send_broadcast(act.buffer, act.length);
    }

    // 2. Drain incoming received packets from ring buffer
    while (rx_tail_ != rx_head_) {
        ControllerRxItem item = rx_queue_[rx_tail_];
        rx_tail_ = (rx_tail_ + 1) % RX_QUEUE_CAP;

        csm.process_incoming_packet(item.data, item.len, current_time_ms);
    }
}

} // namespace controller
} // namespace phuljhari
