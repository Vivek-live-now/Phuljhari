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
#include "controller_config.h"
#include "controller_imu_math.h"
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
 * Acceptance Test 8: Strict GPIO 2 Exclusivity (Requirements 1-7)
 * ------------------------------------------------------------------------- */
static bool test_acceptance_gpio2_strictly_button_controlled() {
    ReceiverStateMachine receiver;
    receiver.init(1000);

    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    uint8_t resp[PHULJHARI_MAX_FRAME_SIZE];
    size_t resp_len = 0;
    bool has_resp = false;

    // Req 1: GPIO 2 remains OFF at startup and during idle
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 1: GPIO 2 is OFF at startup");
    receiver.tick(2000);
    receiver.tick(3000);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 1: GPIO 2 remains OFF during idle");

    // Req 2: PING/PONG, status requests, and effect commands never change GPIO 2
    PingMsg ping = { 1000 };
    serialize_ping(1, &ping, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 3100, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 2: PING does not affect GPIO 2");

    serialize_status_req(2, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 3200, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 2: STATUS_REQ does not affect GPIO 2");

    EffectCmd cmd = { EFFECT_ID_PATAKHA, 2000, 70, 0 };
    serialize_effect_cmd(3, &cmd, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 3300, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(receiver.is_output_active(), "WS2812B is active");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 2: EFFECT_CMD does not affect GPIO 2");

    // Req 3: A valid Button A press turns GPIO 2 ON
    ButtonEventMsg btn = { 0, 0, 0 };
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 10, &btn, wire, sizeof(wire), &wire_len);
    StatusCode st = receiver.process_incoming_packet(wire, wire_len, 4000, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_OK, "Valid BUTTON_DOWN processed");
    ACC_ASSERT(receiver.is_button_indicator_active(), "Req 3: Valid Button A press turns GPIO 2 ON");

    // Req 4: Valid hold-sync maintains the active hold
    serialize_button_event(MSG_TYPE_BUTTON_SYNC, 11, &btn, wire, sizeof(wire), &wire_len);
    st = receiver.process_incoming_packet(wire, wire_len, 4180, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_OK, "Valid BUTTON_SYNC processed");
    ACC_ASSERT(receiver.is_button_indicator_active(), "Req 4: Valid hold-sync maintains active hold");

    // Req 5: Button release turns GPIO 2 OFF immediately
    serialize_button_event(MSG_TYPE_BUTTON_UP, 12, &btn, wire, sizeof(wire), &wire_len);
    st = receiver.process_incoming_packet(wire, wire_len, 4300, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_OK, "Valid BUTTON_UP processed");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 5: Button release turns GPIO 2 OFF immediately");

    // Req 6: Missing hold-sync triggers failsafe timeout
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 20, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 5000, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(receiver.is_button_indicator_active(), "Hold started at 5000ms");
    receiver.tick(5599);
    ACC_ASSERT(receiver.is_button_indicator_active(), "Still active before 600ms timeout");
    receiver.tick(5601);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 6: Missing hold-sync triggers 600ms failsafe timeout");

    // Req 7: Invalid, duplicate, stale, and out-of-order packets cannot activate output or extend timeout
    // Stale sync while inactive -> rejected
    serialize_button_event(MSG_TYPE_BUTTON_SYNC, 25, &btn, wire, sizeof(wire), &wire_len);
    st = receiver.process_incoming_packet(wire, wire_len, 6000, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_ERR_DUPLICATE, "Stale sync rejected");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 7: Stale sync cannot activate GPIO 2");

    // Out-of-order BUTTON_DOWN (seq 15 < last seq 25)
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 15, &btn, wire, sizeof(wire), &wire_len);
    st = receiver.process_incoming_packet(wire, wire_len, 6100, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_ERR_DUPLICATE, "Out-of-order down rejected");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 7: Out-of-order down cannot activate GPIO 2");

    // Start valid hold session seq 30 at 7000ms
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 30, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 7000, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(receiver.is_button_indicator_active(), "Hold active seq 30");

    // Duplicate seq 30 must not extend timeout
    serialize_button_event(MSG_TYPE_BUTTON_SYNC, 30, &btn, wire, sizeof(wire), &wire_len);
    st = receiver.process_incoming_packet(wire, wire_len, 7300, resp, sizeof(resp), &resp_len, &has_resp);
    ACC_ASSERT(st == STATUS_ERR_DUPLICATE, "Duplicate seq rejected");

    // If duplicate had extended timeout, it would expire at 7300 + 600 = 7900.
    // Because it did NOT extend timeout, it must expire at 7000 + 600 = 7600!
    receiver.tick(7601);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 7: Duplicate sync did NOT extend timeout");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 9: IMU Subtle Visual Tilt & Cracker Isolation (Reqs 8-12)
 * ------------------------------------------------------------------------- */
static bool test_acceptance_imu_tilt_subtle_visual_only() {
    ControllerStateMachine controller;
    controller.init(1000);

    // Initial state: Phuljhari selected
    ACC_ASSERT(controller.get_selected_character() == EFFECT_ID_PHULJHARI, "Initial cracker Phuljhari");

    // Req 8: IMU tilt changes visual rotation without changing selected cracker
    float tilt = 0.0f;
    // Apply moderate tilt G = 0.40 G
    for (int i = 0; i < 20; ++i) {
        tilt = ImuTiltMath::update_smooth_tilt(0.40f, tilt);
    }
    float rad = ImuTiltMath::tilt_to_rotation_rad(tilt);
    controller.set_sim_tilt(tilt, rad);

    ACC_ASSERT(controller.get_sim_tilt_x() > 0.3f, "Visual tilt updated");
    ACC_ASSERT(controller.get_sim_tilt_y() > 0.1f && controller.get_sim_tilt_y() <= PHULJHARI_IMU_MAX_ROTATION_RAD,
               "Rotation angle clamped");
    ACC_ASSERT(controller.get_selected_character() == EFFECT_ID_PHULJHARI,
               "Req 8: Tilting does NOT change selected cracker!");

    // Req 9: Returning to neutral tilt smoothly restores neutral orientation
    for (int i = 0; i < 35; ++i) {
        tilt = ImuTiltMath::update_smooth_tilt(0.0f, tilt); // Device returned to level
    }
    rad = ImuTiltMath::tilt_to_rotation_rad(tilt);
    controller.set_sim_tilt(tilt, rad);

    ACC_ASSERT(controller.get_sim_tilt_x() == 0.0f, "Req 9: Neutral tilt restored to 0.0f");
    ACC_ASSERT(controller.get_sim_tilt_y() == 0.0f, "Req 9: Neutral rotation angle restored to 0.0 rad");

    // Req 10: Flicking or tilting does not select another cracker or transmit an effect command
    controller.trigger_sim_virtual_burst(); // Flick simulator telemetry
    controller.set_sim_tilt(0.9f, 0.32f);   // Extreme tilt reading
    ACC_ASSERT(controller.get_selected_character() == EFFECT_ID_PHULJHARI,
               "Req 10: Flicking or tilting does NOT change selected cracker!");
    ACC_ASSERT(!controller.has_pending_action(),
               "Req 10: Flicking or tilting does NOT transmit an effect command!");
    ACC_ASSERT(controller.get_total_triggers() == 0,
               "Req 10: Triggers count remains 0");

    // Req 11: Button-based selection still works independently (8 characters in Phuljhari 2.0)
    ACC_ASSERT(controller.get_selected_character_id() == CHAR_PHULJHARI, "Starts at Phuljhari");
    controller.cycle_character_forward();
    ACC_ASSERT(controller.get_selected_character_id() == CHAR_ANAR, "Req 11: Button cycles to Anar");
    controller.cycle_character_forward();
    ACC_ASSERT(controller.get_selected_character_id() == CHAR_SKY_ROCKET, "Req 11: Button cycles to Sky Rocket");
    // Cycle through remaining 6 characters to return to Phuljhari
    for (int i = 0; i < 6; ++i) {
        controller.cycle_character_forward();
    }
    ACC_ASSERT(controller.get_selected_character_id() == CHAR_PHULJHARI, "Req 11: Button cycles back to Phuljhari");
    ACC_ASSERT(controller.get_selected_character() == EFFECT_ID_PHULJHARI, "Maps to EFFECT_ID_PHULJHARI");

    // Req 12: WS2812B effects and GPIO 2 remain independent
    ReceiverStateMachine receiver;
    receiver.init(1000);
    // WS2812B effect running
    EffectCmd cmd = { EFFECT_ID_PHULJHARI, 4000, 80, 0 };
    EffectAck ack;
    receiver.handle_effect_cmd(100, cmd, 1000, &ack);
    ACC_ASSERT(receiver.is_output_active(), "WS2812B effect is running");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 12: WS2812B running does NOT activate GPIO 2");

    // Button down arrives during effect
    ButtonEventMsg btn = { 0, 0, 0 };
    uint8_t wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    serialize_button_event(MSG_TYPE_BUTTON_DOWN, 101, &btn, wire, sizeof(wire), &wire_len);
    uint8_t dummy[PHULJHARI_MAX_FRAME_SIZE];
    size_t dlen = 0;
    bool dresp = false;
    receiver.process_incoming_packet(wire, wire_len, 1200, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(receiver.is_output_active(), "WS2812B effect still running");
    ACC_ASSERT(receiver.is_button_indicator_active(), "Req 12: Button hold activates GPIO 2 without affecting strip");

    // Effect finishes (advance to 5100ms)
    receiver.tick(5100);
    ACC_ASSERT(!receiver.is_output_active(), "WS2812B finished");
    // Button release arrives
    serialize_button_event(MSG_TYPE_BUTTON_UP, 102, &btn, wire, sizeof(wire), &wire_len);
    receiver.process_incoming_packet(wire, wire_len, 5200, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "Req 12: GPIO 2 turns OFF on release");

    return true;
}

/* -------------------------------------------------------------------------
 * Acceptance Test 10: Controller Reboot & Immediate Button Response
 * Verifies that when M5Stick reboots (resetting sequence number and uptime),
 * the receiver immediately accepts button presses and effect commands without
 * requiring the receiver to be rebooted.
 * ------------------------------------------------------------------------- */
static bool test_controller_reboot_and_button_response() {
    ControllerStateMachine controller;
    ReceiverStateMachine receiver;
    controller.init(1000);
    receiver.init(1000);

    // 1. Establish initial communication and run a few transactions
    // Initial boot ping at t=1000
    controller.send_initial_ping(1000);
    ACC_ASSERT(controller.has_pending_action(), "Initial ping queued");
    OutgoingAction ping1 = controller.pop_pending_action();

    uint8_t pong_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t pong_len = 0;
    bool has_pong = false;
    receiver.process_incoming_packet(ping1.buffer, ping1.length, 1000, pong_buf, sizeof(pong_buf), &pong_len, &has_pong);
    ACC_ASSERT(has_pong, "Receiver ponged");
    controller.process_incoming_packet(pong_buf, pong_len, 1010);

    // Press and hold button at seq 2, 3
    controller.on_button_a_down(2000);
    OutgoingAction btn_down = controller.pop_pending_action();
    uint8_t dummy[PHULJHARI_MAX_FRAME_SIZE];
    size_t dlen = 0;
    bool dresp = false;
    receiver.process_incoming_packet(btn_down.buffer, btn_down.length, 2000, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(receiver.is_button_indicator_active(), "GPIO 2 active before reboot");

    // Release button
    controller.on_button_a_up(2300);
    OutgoingAction btn_up = controller.pop_pending_action();
    receiver.process_incoming_packet(btn_up.buffer, btn_up.length, 2300, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(!receiver.is_button_indicator_active(), "GPIO 2 inactive after release");

    // Send pings to advance controller sequence number past 20
    for (int i = 0; i < 20; ++i) {
        controller.tick(3000 + i * 2000);
        if (controller.has_pending_action()) {
            OutgoingAction p = controller.pop_pending_action();
            receiver.process_incoming_packet(p.buffer, p.length, 3000 + i * 2000, dummy, sizeof(dummy), &dlen, &dresp);
        }
    }

    // 2. SIMULATE M5STICK REBOOT: controller restarts, seq resets to 1, uptime resets to 100ms
    controller.init(100);

    // Initial boot ping from rebooted controller (announcing new session and uptime rollback)
    controller.send_initial_ping(100);
    ACC_ASSERT(controller.has_pending_action(), "Reboot ping queued");
    OutgoingAction reb_ping = controller.pop_pending_action();
    receiver.process_incoming_packet(reb_ping.buffer, reb_ping.length, 50000, pong_buf, sizeof(pong_buf), &pong_len, &has_pong);
    ACC_ASSERT(has_pong, "Receiver accepted reboot ping");

    // 3. User presses Button A on rebooted M5Stick (sequence number is now low: seq 2)
    controller.on_button_a_down(50100);
    ACC_ASSERT(controller.has_pending_action(), "Button down action queued");
    OutgoingAction reb_btn_down = controller.pop_pending_action();
    StatusCode st = receiver.process_incoming_packet(reb_btn_down.buffer, reb_btn_down.length, 50100, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(st == STATUS_OK, "Receiver MUST accept button down from rebooted controller");
    ACC_ASSERT(receiver.is_button_indicator_active(), "GPIO 2 MUST turn ON after controller reboot without rebooting receiver!");

    // Button release
    controller.on_button_a_up(50400);
    ACC_ASSERT(controller.has_pending_action(), "Button up action queued");
    OutgoingAction reb_btn_up = controller.pop_pending_action();
    st = receiver.process_incoming_packet(reb_btn_up.buffer, reb_btn_up.length, 50400, dummy, sizeof(dummy), &dlen, &dresp);
    ACC_ASSERT(st == STATUS_OK, "Receiver MUST accept button up from rebooted controller");
    ACC_ASSERT(!receiver.is_button_indicator_active(), "GPIO 2 MUST turn OFF on release");

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
    RUN_ACC_TEST(test_acceptance_gpio2_strictly_button_controlled);
    RUN_ACC_TEST(test_acceptance_imu_tilt_subtle_visual_only);
    RUN_ACC_TEST(test_controller_reboot_and_button_response);

    std::cout << "====================================================\n";
    std::cout << "Acceptance Summary: " << g_acc_tests_passed << "/" << g_acc_tests_run << " passed";
    if (g_acc_tests_failed > 0) {
        std::cout << " (" << g_acc_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_acc_tests_failed == 0) ? 0 : 1;
}
