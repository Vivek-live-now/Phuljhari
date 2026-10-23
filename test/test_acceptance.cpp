/**
 * @file test_acceptance.cpp
 * @brief Comprehensive End-to-End Acceptance Tests for Project Phuljhari ✨
 * 
 * Verifies:
 * 1. Protocol mismatch and malformed frame rejection
 * 2. Receiver reboot and clean link recovery
 * 3. Communication loss and 20s failsafe watchdog auto-cutoff
 * 4. Stale and out-of-order button hold sync protection (non-reactivation)
 * 5. Rapid button pressing cycles and debounce safety
 * 6. Lost release packet recovery (600ms hold timeout failsafe)
 * 7. Independent control paths for WS2812B strip and separate status LED
 */

#include <iostream>
#include <cassert>
#include <cstring>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "receiver_state_machine.h"
#include "controller_state_machine.h"

using namespace phuljhari;
using namespace phuljhari::protocol;
using namespace phuljhari::receiver;
using namespace phuljhari::controller;

static int g_acc_tests_run = 0;
static int g_acc_tests_passed = 0;
static int g_acc_tests_failed = 0;

#define ACC_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        return false; \
    } \
} while(0)

#define RUN_ACC_TEST(fn) do { \
    g_acc_tests_run++; \
    std::cout << "[RUN] " << #fn << "...\n"; \
    if (fn()) { \
        g_acc_tests_passed++; \
        std::cout << "  [PASS] " << #fn << "\n"; \
    } else { \
        g_acc_tests_failed++; \
        std::cout << "  [FAILED] " << #fn << "\n"; \
    } \
} while(0)

/* -------------------------------------------------------------------------
 * Acceptance Test 1: Protocol Mismatch & Corrupted Wire Frame Rejection
 * ------------------------------------------------------------------------- */
static bool test_protocol_mismatch_and_rejection() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    // 1. Unsupported protocol version frame (version = 0x02)
    uint8_t bad_version_frame[] = {
        PHULJHARI_MAGIC_0, PHULJHARI_MAGIC_1,
        0x02, // Unsupported Version!
        MSG_TYPE_PING,
        0x00, 0x01,
        0x04,
        0x00, 0x00, 0x03, 0xE8,
        0x00 // CRC placeholder
    };
    bad_version_frame[sizeof(bad_version_frame) - 1] = crc8_calculate(bad_version_frame, sizeof(bad_version_frame) - 1);

    uint8_t resp_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t resp_len = 0;
    bool has_resp = false;

    StatusCode st = receiver.process_incoming_packet(
        bad_version_frame, sizeof(bad_version_frame), 1000,
        resp_buf, sizeof(resp_buf), &resp_len, &has_resp
    );
    ACC_ASSERT(st == STATUS_ERR_VERSION, "Must reject unsupported protocol version");
    ACC_ASSERT(!has_resp, "Must not generate response for version mismatch");

    // 2. Corrupted CRC
    bad_version_frame[2] = PHULJHARI_PROTO_VERSION; // Restore version
    bad_version_frame[sizeof(bad_version_frame) - 1] ^= 0xAA; // Corrupt CRC
    st = receiver.process_incoming_packet(
        bad_version_frame, sizeof(bad_version_frame), 1000,
        resp_buf, sizeof(resp_buf), &resp_len, &has_resp
    );
    ACC_ASSERT(st == STATUS_ERR_CRC, "Must reject corrupted CRC");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 2: Receiver Reboot & Clean Link Recovery
 * ------------------------------------------------------------------------- */
static bool test_receiver_reboot_and_recovery() {
    ControllerStateMachine controller;
    ReceiverStateMachine receiver;
    controller.init(1000);
    receiver.init(1000);

    // Initial ping from controller to establish link
    controller.tick(3001);
    ACC_ASSERT(controller.has_pending_action(), "Ping action queued");
    OutgoingAction ping_act = controller.pop_pending_action();

    uint8_t pong_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t pong_len = 0;
    bool has_pong = false;
    receiver.process_incoming_packet(ping_act.buffer, ping_act.length, 3001, pong_buf, sizeof(pong_buf), &pong_len, &has_pong);
    ACC_ASSERT(has_pong, "Receiver generated pong");

    controller.process_incoming_packet(pong_buf, pong_len, 3010);
    ACC_ASSERT(controller.get_link_health(3010) == LINK_CONNECTED, "Link established: CONNECTED");

    // SIMULATE RECEIVER REBOOT at t = 3500ms
    receiver.init(3500);
    ACC_ASSERT(receiver.get_state() == RECV_STATE_SAFE_IDLE, "Reboot state is SAFE_IDLE");
    ACC_ASSERT(!receiver.is_output_active(), "Outputs inactive on reboot");

    // Advance time: controller experiences timeout due to missed heartbeats
    ACC_ASSERT(controller.get_link_health(3500 + 4000) == LINK_RECONNECTING, "Transitions to RECONNECTING");
    ACC_ASSERT(controller.get_link_health(3500 + 8000) == LINK_OFFLINE, "Transitions to OFFLINE");

    // Controller sends next ping at t = 12000ms after reboot
    controller.tick(12000);
    OutgoingAction recov_ping = controller.pop_pending_action();
    receiver.process_incoming_packet(recov_ping.buffer, recov_ping.length, 12000, pong_buf, sizeof(pong_buf), &pong_len, &has_pong);
    ACC_ASSERT(has_pong, "Rebooted receiver replies with pong");

    controller.process_incoming_packet(pong_buf, pong_len, 12005);
    ACC_ASSERT(controller.get_link_health(12005) == LINK_CONNECTED, "Link recovered to CONNECTED after reboot");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 3: Communication Loss & 20s Watchdog Cutoff
 * ------------------------------------------------------------------------- */
static bool test_communication_loss_and_watchdog() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    // Trigger effect: Phuljhari for 12 seconds
    EffectCmd cmd = { EFFECT_ID_PHULJHARI, 12000, 80, 0 };
    EffectAck ack;
    receiver.handle_effect_cmd(50, cmd, 1000, &ack);
    ACC_ASSERT(receiver.is_output_active(), "Effect output active");

    // Advance 6 seconds: effect is active midway
    receiver.tick(7000);
    ACC_ASSERT(receiver.is_output_active(), "Still active at 6s");

    // Total communication loss occurs: no pings or commands for >20 seconds
    // Advance past watchdog timeout: 1000 + 20000ms + 1ms = 21001ms
    receiver.tick(21001);
    ACC_ASSERT(receiver.get_state() == RECV_STATE_SAFE_IDLE, "Watchdog forced SAFE_IDLE");
    ACC_ASSERT(!receiver.is_output_active(), "Outputs forced inactive by watchdog failsafe");
    ACC_ASSERT(receiver.get_active_effect() == EFFECT_ID_IDLE, "Effect reset to IDLE");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 4: Stale & Out-of-Order Button Hold Protection
 * ------------------------------------------------------------------------- */
static bool test_stale_and_out_of_order_button_sync() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    // 1. Controller sends BUTTON_DOWN (seq 10)
    ButtonEventMsg btn = { 0, 0, 0 };
    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 10, &btn, wire, sizeof(wire), &wire_len);

    uint8_t dummy_resp[PHULJHARI_MAX_FRAME_SIZE];
    size_t dummy_len = 0;
    bool has_resp = false;
    receiver.process_incoming_packet(wire, wire_len, 1000, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
    ACC_ASSERT(receiver.is_button_indicator_active(), "Hold indicator active after BUTTON_DOWN");

    // 2. Controller sends BUTTON_UP (seq 12) at t = 1300ms
    btn.hold_duration_ms = 300;
    serialize_button_event(MSG_TYPE_BUTTON_UP, 12, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 1300, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Hold indicator OFF after BUTTON_UP");

    // 3. A delayed, stale BUTTON_SYNC (seq 11) arrives LATE at t = 1350ms!
    btn.hold_duration_ms = 180;
    serialize_button_event(MSG_TYPE_BUTTON_SYNC, 11, &btn, wire, sizeof(wire), &wire_len);
    StatusCode st = receiver.process_incoming_packet(wire, wire_len, 1350, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
    ACC_ASSERT(st == STATUS_ERR_DUPLICATE, "Stale sync must be rejected");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Delayed sync MUST NOT reactivate released indicator");

    // 4. Stale duplicate BUTTON_DOWN (seq 9) arrives late
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 9, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 1400, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Stale BUTTON_DOWN must not reactivate indicator");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 5: Rapid Button Presses (Debounce & Sequence Safety)
 * ------------------------------------------------------------------------- */
static bool test_rapid_button_press_cycles() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    uint8_t dummy_resp[PHULJHARI_MAX_FRAME_SIZE];
    size_t dummy_len = 0;
    bool has_resp = false;

    // Simulate 5 rapid button press & release cycles (e.g. 40ms each)
    uint16_t seq = 100;
    for (int i = 0; i < 5; ++i) {
        uint32_t press_time = 1000 + i * 80;
        uint32_t release_time = press_time + 40;

        ButtonEventMsg btn = { 0, 0, 0 };
        serialize_button_event(MSG_TYPE_BUTTON_DOWN, seq++, &btn, wire, sizeof(wire), &wire_len);
        receiver.process_incoming_packet(wire, wire_len, press_time, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
        ACC_ASSERT(receiver.is_button_indicator_active(), "Indicator active on rapid down");

        btn.hold_duration_ms = 40;
        serialize_button_event(MSG_TYPE_BUTTON_UP, seq++, &btn, wire, sizeof(wire), &wire_len);
        receiver.process_incoming_packet(wire, wire_len, release_time, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
        ACC_ASSERT(!receiver.is_button_indicator_active(), "Indicator OFF on rapid up");
    }

    ACC_ASSERT(!receiver.is_button_indicator_active(), "Final state is cleanly inactive");
    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 6: Lost Release Packet Recovery (600ms Failsafe)
 * ------------------------------------------------------------------------- */
static bool test_button_release_recovery_failsafe() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    ButtonEventMsg btn = { 0, 0, 0 };
    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 300, &btn, wire, sizeof(wire), &wire_len);

    uint8_t dummy_resp[PHULJHARI_MAX_FRAME_SIZE];
    size_t dummy_len = 0;
    bool has_resp = false;
    receiver.process_incoming_packet(wire, wire_len, 1000, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);
    ACC_ASSERT(receiver.is_button_indicator_active(), "Indicator active");

    // BUTTON_UP packet is completely dropped over the air and never arrives!
    // Time advances past 600ms: 1000 + 600ms = 1600ms
    receiver.tick(1601);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Failsafe auto-cutoff deactivated indicator after 600ms");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 7: Independent Control Paths for Status LED & WS2812B
 * ------------------------------------------------------------------------- */
static bool test_independent_led_control_paths() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    // 1. Initial State: both inactive
    ACC_ASSERT(!receiver.is_output_active(), "WS2812B strip initially inactive");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Status LED initially inactive");

    // 2. Button held down while idle: Status LED ON, Strip remains INACTIVE
    ButtonEventMsg btn = { 0, 0, 0 };
    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 401, &btn, wire, sizeof(wire), &wire_len);
    uint8_t dummy_resp[PHULJHARI_MAX_FRAME_SIZE];
    size_t dummy_len = 0;
    bool has_resp = false;
    receiver.process_incoming_packet(wire, wire_len, 1000, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);

    ACC_ASSERT(receiver.is_button_indicator_active(), "Status LED is ON");
    ACC_ASSERT(!receiver.is_output_active(), "WS2812B strip MUST remain inactive while holding in idle");

    // 3. Trigger decorative effect (Anar, 5000ms): Strip turns ACTIVE
    EffectCmd cmd = { EFFECT_ID_ANAR, 5000, 80, 0 };
    EffectAck ack;
    receiver.handle_effect_cmd(402, cmd, 1100, &ack);

    ACC_ASSERT(receiver.is_output_active(), "WS2812B strip is ACTIVE");
    ACC_ASSERT(receiver.is_button_indicator_active(), "Status LED is still ON");

    // 4. Release button while effect is running: Status LED turns OFF, Strip remains ACTIVE
    serialize_button_event(MSG_TYPE_BUTTON_UP, 403, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 1200, dummy_resp, sizeof(dummy_resp), &dummy_len, &has_resp);

    ACC_ASSERT(!receiver.is_button_indicator_active(), "Status LED turns OFF on release");
    ACC_ASSERT(receiver.is_output_active(), "WS2812B strip MUST continue running its decorative effect");
    ACC_ASSERT(receiver.get_remaining_ms(1200) > 0, "Effect has remaining runtime");

    // 5. STOP command issued: Strip immediately cuts off, Status LED unaffected
    EffectStopMsg stop = { 0 };
    receiver.handle_effect_stop(404, stop, 1500);
    ACC_ASSERT(!receiver.is_output_active(), "WS2812B strip immediately stopped");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Status LED remains unaffected");

    return true;
}

/* -------------------------------------------------------------------------
 * Main Acceptance Test Runner
 * ------------------------------------------------------------------------- */
int main() {
    std::cout << "====================================================\n";
    std::cout << "Project Phuljhari ✨ Acceptance & Integration Tests\n";
    std::cout << "====================================================\n";

    RUN_ACC_TEST(test_protocol_mismatch_and_rejection);
    RUN_ACC_TEST(test_receiver_reboot_and_recovery);
    RUN_ACC_TEST(test_communication_loss_and_watchdog);
    RUN_ACC_TEST(test_stale_and_out_of_order_button_sync);
    RUN_ACC_TEST(test_rapid_button_press_cycles);
    RUN_ACC_TEST(test_button_release_recovery_failsafe);
    RUN_ACC_TEST(test_independent_led_control_paths);

    std::cout << "====================================================\n";
    std::cout << "Acceptance Summary: " << g_acc_tests_passed << "/" << g_acc_tests_run << " passed";
    if (g_acc_tests_failed > 0) {
        std::cout << " (" << g_acc_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_acc_tests_failed == 0) ? 0 : 1;
}
