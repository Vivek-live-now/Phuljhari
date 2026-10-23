/**
 * @file receiver_radio.cpp
 * @brief Implementation of ESP-NOW radio manager for Project Phuljhari Receiver ✨
 */

#include "receiver_radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include <cstring>

namespace phuljhari {
namespace receiver {

static ReceiverRadio* s_radio_instance = nullptr;

/* -------------------------------------------------------------------------
 * Low-level ESP-NOW Callbacks (ISR / Wi-Fi task context)
 * ------------------------------------------------------------------------- */

#if defined(ESP_ARDUINO_VERSION_VAL) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (s_radio_instance != nullptr && info != nullptr && data != nullptr) {
        s_radio_instance->enqueue_rx_packet(info->src_addr, data, (size_t)len);
    }
}
#else
static void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
    if (s_radio_instance != nullptr && mac != nullptr && data != nullptr) {
        s_radio_instance->enqueue_rx_packet(mac, data, (size_t)len);
    }
}
#endif

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (s_radio_instance != nullptr) {
        s_radio_instance->on_data_sent(mac_addr, (uint8_t)status);
    }
}

/* -------------------------------------------------------------------------
 * ReceiverRadio Class Implementation
 * ------------------------------------------------------------------------- */

ReceiverRadio::ReceiverRadio(ReceiverStateMachine& sm)
    : sm_(sm),
      rx_head_(0),
      rx_tail_(0),
      packets_received_(0),
      packets_sent_(0),
      queue_overflows_(0),
      last_tx_status_(protocol::TRANSPORT_TX_IDLE) {
    s_radio_instance = this;
    std::memset(rx_queue_, 0, sizeof(rx_queue_));
}

bool ReceiverRadio::begin() {
    Serial.println("[RADIO] Initializing ESP-NOW Receiver...");

    // 1. Initialize Wi-Fi in Station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // 2. Lock Wi-Fi Channel
    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) {
        esp_wifi_set_channel(PHULJHARI_CONFIG_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
    } else {
        Serial.printf("[RADIO] Failed to set Wi-Fi channel: %d\n", err);
    }

    uint8_t primary_chan = 0;
    wifi_second_chan_t second_chan = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary_chan, &second_chan);
    Serial.printf("[RADIO] Operating on Wi-Fi Channel %d\n", primary_chan);

    // 3. Print MAC Address
    String mac = WiFi.macAddress();
    Serial.printf("[RADIO] Receiver MAC Address: %s\n", mac.c_str());

    // 4. Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[RADIO] ERROR: ESP-NOW Init Failed!");
        return false;
    }

    // 5. Register non-blocking callbacks
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);

    Serial.println("[RADIO] ESP-NOW Receiver ready.");
    return true;
}

void ReceiverRadio::enqueue_rx_packet(const uint8_t* sender_mac, const uint8_t* data, size_t len) {
    if (sender_mac == nullptr || data == nullptr || len == 0 || len > PHULJHARI_MAX_FRAME_SIZE) {
        return;
    }

    uint8_t next_head = (rx_head_ + 1) % PHULJHARI_CONFIG_RX_QUEUE_SIZE;
    if (next_head == rx_tail_) {
        // Queue full: drop packet to protect ISR latency and increment overflow counter
        queue_overflows_++;
        return;
    }

    RxQueueItem& item = rx_queue_[rx_head_];
    std::memcpy(item.sender_mac, sender_mac, 6);
    std::memcpy(item.data, data, len);
    item.len = (uint8_t)len;

    rx_head_ = next_head;
    packets_received_++;
}

void ReceiverRadio::on_data_sent(const uint8_t* /*mac_addr*/, uint8_t status) {
    last_tx_status_ = (status == ESP_NOW_SEND_SUCCESS)
                          ? protocol::TRANSPORT_TX_SUCCESS
                          : protocol::TRANSPORT_TX_FAIL;
}

bool ReceiverRadio::ensure_peer_registered(const uint8_t* peer_mac) {
    if (peer_mac == nullptr) return false;

    if (esp_now_is_peer_exist(peer_mac)) {
        return true;
    }

    esp_now_peer_info_t peerInfo;
    std::memset(&peerInfo, 0, sizeof(peerInfo));
    std::memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = PHULJHARI_CONFIG_WIFI_CHANNEL;
    peerInfo.encrypt = false;

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result == ESP_OK) {
        Serial.printf("[RADIO] Registered new peer: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      peer_mac[0], peer_mac[1], peer_mac[2],
                      peer_mac[3], peer_mac[4], peer_mac[5]);
        return true;
    } else {
        Serial.printf("[RADIO] Failed to register peer: %d\n", result);
        return false;
    }
}

bool ReceiverRadio::send_to_peer(const uint8_t* peer_mac, const uint8_t* data, size_t len) {
    if (peer_mac == nullptr || data == nullptr || len == 0) return false;

    if (!ensure_peer_registered(peer_mac)) {
        return false;
    }

    esp_err_t result = esp_now_send(peer_mac, data, len);
    if (result == ESP_OK) {
        packets_sent_++;
        return true;
    } else {
        Serial.printf("[RADIO] esp_now_send failed: %d\n", result);
        return false;
    }
}

void ReceiverRadio::update(uint32_t current_time_ms) {
    // Drain queued packets from the ISR ring buffer
    while (rx_tail_ != rx_head_) {
        RxQueueItem item = rx_queue_[rx_tail_];
        rx_tail_ = (rx_tail_ + 1) % PHULJHARI_CONFIG_RX_QUEUE_SIZE;

        uint8_t resp_buf[PHULJHARI_MAX_FRAME_SIZE];
        size_t resp_len = 0;
        bool has_resp = false;

        protocol::StatusCode st = sm_.process_incoming_packet(
            item.data,
            item.len,
            current_time_ms,
            resp_buf,
            sizeof(resp_buf),
            &resp_len,
            &has_resp
        );

        if (has_resp && resp_len > 0) {
            send_to_peer(item.sender_mac, resp_buf, resp_len);
        }

        if (st != protocol::STATUS_OK && st != protocol::STATUS_ERR_DUPLICATE) {
            Serial.printf("[RADIO] Frame validation error code: 0x%02X\n", st);
        }
    }
}

} // namespace receiver
} // namespace phuljhari
