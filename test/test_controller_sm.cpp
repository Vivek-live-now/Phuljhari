/**
 * @file test_controller_sm.cpp
 * @brief Host-native unit test suite for M5StickS3 Controller State Machine
 */

#include <iostream>
#include <cassert>
#include <cstring>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "controller_state_machine.h"

using namespace phuljhari;
using namespace phuljhari::protocol;
using namespace phuljhari::controller;

static int g_ctrl_tests_run = 0;
static int g_ctrl_tests_passed = 0;
static int g_ctrl_tests_failed = 0;

#define CTRL_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        return false; \
    } \
} while(0)

#define RUN_CTRL_TEST(fn) do { \
    g_ctrl_tests_run++; \
    std::cout << "[RUN] " << #fn << "...\n"; \
    if (fn()) { \
        g_ctrl_tests_passed++; \
        std::cout << "  [PASS] " << #fn << "\n"; \
    } else { \
        g_ctrl_tests_failed++; \
        std::cout << "  [FAILED] " << #fn << "\n"; \
    } \
} while(0)

/* -------------------------------------------------------------------------
 * Test 1: Initial State & Character Cycling
 * ------------------------------------------------------------------------- */
static bool test_ctrl_init_and_cycling() {
    ControllerStateMachine csm;
    csm.init(1000);

    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_PHULJHARI, "Default character should be Phuljhari");
    CTRL_ASSERT(csm.get_total_triggers() == 0, "Initial triggers should be 0");
    CTRL_ASSERT(!csm.is_button_a_held(), "Button A not held");
    CTRL_ASSERT(!csm.is_command_pending(), "No command pending");
    CTRL_ASSERT(!csm.is_ack_toast_active(1000), "Toast not active");
    CTRL_ASSERT(csm.get_link_health(1000) == LINK_OFFLINE, "Initial link state OFFLINE");

    // Forward cycling: Phuljhari -> Anar -> Patakha -> Phuljhari
    csm.cycle_character_forward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_ANAR, "Cycled forward to Anar");
    csm.cycle_character_forward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_PATAKHA, "Cycled forward to Patakha");
    csm.cycle_character_forward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_PHULJHARI, "Cycled back to Phuljhari");

    // Backward cycling: Phuljhari -> Patakha -> Anar -> Phuljhari
    csm.cycle_character_backward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_PATAKHA, "Cycled backward to Patakha");
    csm.cycle_character_backward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_ANAR, "Cycled backward to Anar");
    csm.cycle_character_backward();
    CTRL_ASSERT(csm.get_selected_character() == EFFECT_ID_PHULJHARI, "Cycled backward to Phuljhari");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 2: Live Button Synchronization (DOWN, SYNC, UP)
 * ------------------------------------------------------------------------- */
static bool test_ctrl_live_button_sync() {
    ControllerStateMachine csm;
    csm.init(1000);

    // 1. Button down at 1000ms
    csm.on_button_a_down(1000);
    CTRL_ASSERT(csm.is_button_a_held(), "Button A is held");
    CTRL_ASSERT(csm.has_pending_action(), "Action queued for button down");

    OutgoingAction act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_BTN_DOWN, "Action type is ACTION_SEND_BTN_DOWN");

    RawFrame frame;
    StatusCode st = deserialize_frame(act.buffer, act.length, &frame);
    CTRL_ASSERT(st == STATUS_OK, "Valid button down frame");
    CTRL_ASSERT(frame.msg_type == MSG_TYPE_BUTTON_DOWN, "Msg type is MSG_TYPE_BUTTON_DOWN");

    // 2. Button held: advance time by 180ms tick -> should generate BUTTON_SYNC
    csm.tick(1185);
    CTRL_ASSERT(csm.has_pending_action(), "Periodic sync action queued");
    act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_BTN_SYNC, "Action type is ACTION_SEND_BTN_SYNC");

    st = deserialize_frame(act.buffer, act.length, &frame);
    CTRL_ASSERT(st == STATUS_OK, "Valid sync frame");
    CTRL_ASSERT(frame.msg_type == MSG_TYPE_BUTTON_SYNC, "Msg type is MSG_TYPE_BUTTON_SYNC");

    // 3. Button up at 1350ms
    csm.on_button_a_up(1350);
    CTRL_ASSERT(!csm.is_button_a_held(), "Button A released");
    CTRL_ASSERT(csm.has_pending_action(), "Action queued on release");

    // First popped action is BUTTON_UP (or trigger command)
    act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_CMD, "Trigger effect command action queued on release");
    CTRL_ASSERT(csm.is_command_pending(), "Command is pending ACK");
    CTRL_ASSERT(csm.get_total_triggers() == 1, "Total triggers incremented to 1");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 3: Bounded Retries on Unacknowledged Commands
 * ------------------------------------------------------------------------- */
static bool test_ctrl_bounded_retries() {
    ControllerStateMachine csm;
    csm.init(1000);
    csm.set_selected_character(EFFECT_ID_ANAR);

    csm.on_button_a_down(1000);
    csm.pop_pending_action(); // pop btn down
    csm.on_button_a_up(1050);
    csm.pop_pending_action(); // pop initial cmd

    CTRL_ASSERT(csm.is_command_pending(), "Command is pending");
    CTRL_ASSERT(csm.get_pending_retries_left() == PHULJHARI_MAX_TX_RETRIES, "3 retries left initially");

    // Retry 1 after 200ms
    csm.tick(1251);
    CTRL_ASSERT(csm.has_pending_action(), "Retry 1 action queued");
    OutgoingAction act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_CMD, "Retry 1 is ACTION_SEND_CMD");
    CTRL_ASSERT(csm.get_pending_retries_left() == 2, "2 retries left");

    // Retry 2 after another 200ms
    csm.tick(1452);
    CTRL_ASSERT(csm.has_pending_action(), "Retry 2 action queued");
    csm.pop_pending_action();
    CTRL_ASSERT(csm.get_pending_retries_left() == 1, "1 retry left");

    // Retry 3 after another 200ms
    csm.tick(1653);
    CTRL_ASSERT(csm.has_pending_action(), "Retry 3 action queued");
    csm.pop_pending_action();
    CTRL_ASSERT(csm.get_pending_retries_left() == 0, "0 retries left");

    // After retry 3 interval elapses without ACK, command gives up cleanly
    csm.tick(1854);
    CTRL_ASSERT(!csm.is_command_pending(), "Command dropped cleanly after retries exhausted");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 4: Application-Level ACK Matching & Confirmation Toast
 * ------------------------------------------------------------------------- */
static bool test_ctrl_application_ack_matching() {
    ControllerStateMachine csm;
    csm.init(1000);
    csm.set_selected_character(EFFECT_ID_PATAKHA);

    csm.on_button_a_down(1000);
    csm.pop_pending_action();
    csm.on_button_a_up(1020);
    OutgoingAction cmd_act = csm.pop_pending_action();

    CTRL_ASSERT(csm.is_command_pending(), "Command is pending");
    uint16_t sent_seq = cmd_act.seq;

    // Simulate incoming ACK with a non-matching sequence number (e.g. 999)
    EffectAck non_matching_ack = { 999, STATUS_OK, RECV_STATE_EFFECT_ACTIVE, 4800 };
    uint8_t wire_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;
    serialize_effect_ack(999, &non_matching_ack, wire_buf, sizeof(wire_buf), &wire_len);

    csm.process_incoming_packet(wire_buf, wire_len, 1050);
    CTRL_ASSERT(csm.is_command_pending(), "Non-matching ACK must NOT clear pending command");
    CTRL_ASSERT(!csm.is_ack_toast_active(1050), "Toast must not activate on non-matching ACK");

    // Simulate incoming ACK matching the sent sequence number
    EffectAck matching_ack = { sent_seq, STATUS_OK, RECV_STATE_EFFECT_ACTIVE, 4800 };
    serialize_effect_ack(sent_seq, &matching_ack, wire_buf, sizeof(wire_buf), &wire_len);

    csm.process_incoming_packet(wire_buf, wire_len, 1060);
    CTRL_ASSERT(!csm.is_command_pending(), "Matching ACK clears pending command");
    CTRL_ASSERT(csm.is_ack_toast_active(1060), "Celebratory ACK toast activates");
    CTRL_ASSERT(csm.is_ack_toast_active(1060 + 1000), "Toast remains active at 1000ms");

    // Advance past toast duration (1060 + 1600ms = 2660ms)
    csm.tick(2661);
    CTRL_ASSERT(!csm.is_ack_toast_active(2661), "Toast expires after duration");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 5: Virtual Simulator Decoupling
 * ------------------------------------------------------------------------- */
static bool test_ctrl_simulator_decoupling() {
    ControllerStateMachine csm;
    csm.init(1000);

    // Adjusting virtual simulator controls (tilt, virtual burst)
    csm.set_sim_tilt(0.42f, -0.31f);
    CTRL_ASSERT(csm.get_sim_tilt_x() == 0.42f, "Tilt X matches");
    CTRL_ASSERT(csm.get_sim_tilt_y() == -0.31f, "Tilt Y matches");

    csm.trigger_sim_virtual_burst();
    CTRL_ASSERT(csm.consume_sim_virtual_burst(), "Virtual burst consumed");
    CTRL_ASSERT(!csm.consume_sim_virtual_burst(), "Virtual burst is one-shot");

    // Virtual simulator actions must NOT queue any wireless radio packets!
    CTRL_ASSERT(!csm.has_pending_action(), "Simulator adjustments do NOT queue radio actions");
    CTRL_ASSERT(csm.get_total_triggers() == 0, "Triggers remain 0");

    return true;
}

/* -------------------------------------------------------------------------
 * Main Controller SM Test Runner
 * ------------------------------------------------------------------------- */
int main() {
    std::cout << "====================================================\n";
    std::cout << "Project Phuljhari ✨ Controller State Machine Tests\n";
    std::cout << "====================================================\n";

    RUN_CTRL_TEST(test_ctrl_init_and_cycling);
    RUN_CTRL_TEST(test_ctrl_live_button_sync);
    RUN_CTRL_TEST(test_ctrl_bounded_retries);
    RUN_CTRL_TEST(test_ctrl_application_ack_matching);
    RUN_CTRL_TEST(test_ctrl_simulator_decoupling);

    std::cout << "====================================================\n";
    std::cout << "Controller SM Summary: " << g_ctrl_tests_passed << "/" << g_ctrl_tests_run << " passed";
    if (g_ctrl_tests_failed > 0) {
        std::cout << " (" << g_ctrl_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_ctrl_tests_failed == 0) ? 0 : 1;
}
