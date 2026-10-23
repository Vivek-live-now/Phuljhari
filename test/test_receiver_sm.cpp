/**
 * @file test_receiver_sm.cpp
 * @brief Host-native unit test suite for Receiver State Machine
 */

#include <iostream>
#include <cassert>
#include <cstring>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "receiver_state_machine.h"

using namespace phuljhari;
using namespace phuljhari::protocol;
using namespace phuljhari::receiver;

static int g_sm_tests_run = 0;
static int g_sm_tests_passed = 0;
static int g_sm_tests_failed = 0;

#define SM_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        return false; \
    } \
} while(0)

#define RUN_SM_TEST(fn) do { \
    g_sm_tests_run++; \
    std::cout << "[RUN] " << #fn << "...\n"; \
    if (fn()) { \
        g_sm_tests_passed++; \
        std::cout << "  [PASS] " << #fn << "\n"; \
    } else { \
        g_sm_tests_failed++; \
        std::cout << "  [FAILED] " << #fn << "\n"; \
    } \
} while(0)

/* -------------------------------------------------------------------------
 * Test 1: Initial State Safety Defaults
 * ------------------------------------------------------------------------- */
static bool test_sm_init_safety() {
    ReceiverStateMachine sm;
    sm.init(1000);

    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "Initial state must be SAFE_IDLE");
    SM_ASSERT(sm.get_active_effect() == EFFECT_ID_IDLE, "Active effect must be IDLE");
    SM_ASSERT(sm.is_output_active() == false, "Output must be inactive at startup");
    SM_ASSERT(sm.get_active_intensity() == 0, "Intensity must be 0");
    SM_ASSERT(sm.get_total_effects() == 0, "Total effects executed must start at 0");
    SM_ASSERT(sm.get_remaining_ms(1000) == 0, "Remaining ms should be 0");
    return true;
}

/* -------------------------------------------------------------------------
 * Test 2: Valid Effect Command & Execution Transitions
 * ------------------------------------------------------------------------- */
static bool test_sm_valid_command_and_lifecycle() {
    ReceiverStateMachine sm;
    sm.init(1000);

    EffectCmd cmd = { EFFECT_ID_PHULJHARI, 4000, 80, 0 };
    EffectAck ack;
    StatusCode st = sm.handle_effect_cmd(10, cmd, 1000, &ack);

    SM_ASSERT(st == STATUS_OK, "handle_effect_cmd should succeed");
    SM_ASSERT(ack.ack_seq == 10, "ACK seq match");
    SM_ASSERT(ack.status_code == STATUS_OK, "ACK status code match");
    SM_ASSERT(ack.receiver_state == RECV_STATE_EFFECT_ACTIVE, "ACK state match");
    SM_ASSERT(ack.remaining_ms == 4000, "ACK duration match");

    SM_ASSERT(sm.get_state() == RECV_STATE_EFFECT_ACTIVE, "SM should be in EFFECT_ACTIVE");
    SM_ASSERT(sm.get_active_effect() == EFFECT_ID_PHULJHARI, "Active effect should be PHULJHARI");
    SM_ASSERT(sm.is_output_active() == true, "Output must be active");
    SM_ASSERT(sm.get_total_effects() == 1, "Total effects executed should be 1");

    // Advance 2000 ms (midway through effect)
    sm.tick(3000);
    SM_ASSERT(sm.get_state() == RECV_STATE_EFFECT_ACTIVE, "Still active midway");
    SM_ASSERT(sm.is_output_active() == true, "Output still active");
    SM_ASSERT(sm.get_remaining_ms(3000) == 2000, "2000ms remaining");

    // Advance to 5000 ms (4000 ms duration finished)
    sm.tick(5000);
    SM_ASSERT(sm.get_state() == RECV_STATE_COOLDOWN, "Transitioned to COOLDOWN");
    SM_ASSERT(sm.is_output_active() == false, "Output must be inactive during cooldown");

    // Advance past cooldown (5000 + 300 ms = 5300 ms)
    sm.tick(5300);
    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "Returned to SAFE_IDLE after cooldown");
    SM_ASSERT(sm.get_active_effect() == EFFECT_ID_IDLE, "Effect reset to IDLE");
    SM_ASSERT(sm.is_output_active() == false, "Output remains inactive in idle");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 3: Idempotent Retransmission Handling
 * ------------------------------------------------------------------------- */
static bool test_sm_idempotent_duplicate_handling() {
    ReceiverStateMachine sm;
    sm.init(1000);

    EffectCmd cmd = { EFFECT_ID_ANAR, 5000, 75, 0 };
    EffectAck ack1;
    StatusCode st = sm.handle_effect_cmd(42, cmd, 1000, &ack1);
    SM_ASSERT(st == STATUS_OK, "First command accepted");
    SM_ASSERT(sm.get_total_effects() == 1, "Executed count is 1");

    // Time advances by 1500 ms; controller retransmits exact same seq=42
    EffectAck ack2;
    st = sm.handle_effect_cmd(42, cmd, 2500, &ack2);
    SM_ASSERT(st == STATUS_OK, "Duplicate command handled idempotently");
    SM_ASSERT(ack2.ack_seq == 42, "ACK seq match");
    SM_ASSERT(ack2.status_code == STATUS_OK, "ACK status OK");
    SM_ASSERT(ack2.receiver_state == RECV_STATE_EFFECT_ACTIVE, "State active");
    SM_ASSERT(ack2.remaining_ms == 3500, "Remaining time updated (5000 - 1500 = 3500)");
    SM_ASSERT(sm.get_total_effects() == 1, "Total effects executed must NOT increment on duplicate");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 4: Safety Boundary Rejections
 * ------------------------------------------------------------------------- */
static bool test_sm_safety_rejections() {
    ReceiverStateMachine sm;
    sm.init(1000);

    // 1. Invalid Effect ID
    EffectCmd bad_id = { 0x55, 5000, 80, 0 };
    EffectAck ack;
    StatusCode st = sm.handle_effect_cmd(1, bad_id, 1000, &ack);
    SM_ASSERT(st == STATUS_ERR_INVALID_EFFECT, "Should reject invalid effect ID");
    SM_ASSERT(ack.status_code == STATUS_ERR_INVALID_EFFECT, "ACK reports invalid effect");
    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "State remains SAFE_IDLE");
    SM_ASSERT(sm.is_output_active() == false, "Outputs remain inactive");

    // 2. Duration exceeding maximum safety limit (>15s)
    EffectCmd excessive_duration = { EFFECT_ID_PHULJHARI, 20000, 80, 0 };
    st = sm.handle_effect_cmd(2, excessive_duration, 1000, &ack);
    SM_ASSERT(st == STATUS_ERR_SAFETY_VIOLATION, "Should reject >15s duration");
    SM_ASSERT(ack.status_code == STATUS_ERR_SAFETY_VIOLATION, "ACK reports safety violation");
    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "State remains SAFE_IDLE");

    // 3. Duration below minimum limit (<100ms)
    EffectCmd short_duration = { EFFECT_ID_PHULJHARI, 50, 80, 0 };
    st = sm.handle_effect_cmd(3, short_duration, 1000, &ack);
    SM_ASSERT(st == STATUS_ERR_SAFETY_VIOLATION, "Should reject <100ms duration");

    // 4. Intensity 0%
    EffectCmd zero_intensity = { EFFECT_ID_PHULJHARI, 3000, 0, 0 };
    st = sm.handle_effect_cmd(4, zero_intensity, 1000, &ack);
    SM_ASSERT(st == STATUS_ERR_SAFETY_VIOLATION, "Should reject 0% intensity");

    // 5. Intensity >100%
    EffectCmd over_intensity = { EFFECT_ID_PHULJHARI, 3000, 105, 0 };
    st = sm.handle_effect_cmd(5, over_intensity, 1000, &ack);
    SM_ASSERT(st == STATUS_ERR_SAFETY_VIOLATION, "Should reject >100% intensity");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 5: STOP Command Halts Effect Immediately
 * ------------------------------------------------------------------------- */
static bool test_sm_stop_command() {
    ReceiverStateMachine sm;
    sm.init(1000);

    EffectCmd cmd = { EFFECT_ID_PATAKHA, 10000, 90, 0 };
    EffectAck ack;
    sm.handle_effect_cmd(100, cmd, 1000, &ack);
    SM_ASSERT(sm.get_state() == RECV_STATE_EFFECT_ACTIVE, "Active");
    SM_ASSERT(sm.is_output_active() == true, "Output active");

    // Send STOP command at t = 2500ms
    EffectStopMsg stop_msg = { 0 };
    StatusCode st = sm.handle_effect_stop(101, stop_msg, 2500);
    SM_ASSERT(st == STATUS_OK, "Stop handled successfully");
    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "Returned immediately to SAFE_IDLE");
    SM_ASSERT(sm.get_active_effect() == EFFECT_ID_IDLE, "Effect reset to IDLE");
    SM_ASSERT(sm.is_output_active() == false, "Output immediately inactive");
    SM_ASSERT(sm.get_remaining_ms(2500) == 0, "0ms remaining");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 6: Watchdog Failsafe Auto-Cutoff
 * ------------------------------------------------------------------------- */
static bool test_sm_watchdog_cutoff() {
    ReceiverStateMachine sm;
    sm.init(1000);

    EffectCmd cmd = { EFFECT_ID_PHULJHARI, 12000, 80, 0 };
    EffectAck ack;
    sm.handle_effect_cmd(1, cmd, 1000, &ack);
    SM_ASSERT(sm.is_output_active() == true, "Output active");

    // Advance time past watchdog timeout (1000 + 20000 ms)
    sm.tick(1000 + PHULJHARI_SAFETY_WATCHDOG_TIMEOUT_MS + 1);
    SM_ASSERT(sm.get_state() == RECV_STATE_SAFE_IDLE, "Watchdog must force state to SAFE_IDLE");
    SM_ASSERT(sm.is_output_active() == false, "Output must be forced inactive by watchdog");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 7: Full Wire Packet Ingestion & Response Generation
 * ------------------------------------------------------------------------- */
static bool test_sm_wire_packet_processing() {
    ReceiverStateMachine sm;
    sm.init(1000);

    // 1. Serialize a real EFFECT_CMD frame
    EffectCmd cmd = { EFFECT_ID_PATAKHA, 3000, 85, 0 };
    uint8_t wire_cmd[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_cmd_len = 0;
    StatusCode st = serialize_effect_cmd(555, &cmd, wire_cmd, sizeof(wire_cmd), &wire_cmd_len);
    SM_ASSERT(st == STATUS_OK, "serialize_effect_cmd failed");

    // 2. Feed raw wire bytes into process_incoming_packet
    uint8_t resp_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t resp_len = 0;
    bool has_resp = false;
    st = sm.process_incoming_packet(wire_cmd, wire_cmd_len, 1000, resp_buf, sizeof(resp_buf), &resp_len, &has_resp);

    SM_ASSERT(st == STATUS_OK, "Packet processing succeeded");
    SM_ASSERT(has_resp == true, "Response generated");
    SM_ASSERT(resp_len > 0, "Response length > 0");

    // 3. Deserialize generated response and verify it is a valid EFFECT_ACK
    RawFrame resp_frame;
    st = deserialize_frame(resp_buf, resp_len, &resp_frame);
    SM_ASSERT(st == STATUS_OK, "Response frame deserialization succeeded");
    SM_ASSERT(resp_frame.msg_type == MSG_TYPE_EFFECT_ACK, "Response is MSG_TYPE_EFFECT_ACK");
    SM_ASSERT(resp_frame.seq == 555, "Response seq matches request seq");

    EffectAck unpacked_ack;
    st = deserialize_effect_ack(&resp_frame, &unpacked_ack);
    SM_ASSERT(st == STATUS_OK, "ACK unpack succeeded");
    SM_ASSERT(unpacked_ack.ack_seq == 555, "ACK seq match");
    SM_ASSERT(unpacked_ack.status_code == STATUS_OK, "ACK status match");
    SM_ASSERT(unpacked_ack.receiver_state == RECV_STATE_EFFECT_ACTIVE, "State active");

    return true;
}

/* -------------------------------------------------------------------------
 * Main SM Test Runner
 * ------------------------------------------------------------------------- */
int main() {
    std::cout << "====================================================\n";
    std::cout << "Project Phuljhari ✨ Receiver State Machine Tests\n";
    std::cout << "====================================================\n";

    RUN_SM_TEST(test_sm_init_safety);
    RUN_SM_TEST(test_sm_valid_command_and_lifecycle);
    RUN_SM_TEST(test_sm_idempotent_duplicate_handling);
    RUN_SM_TEST(test_sm_safety_rejections);
    RUN_SM_TEST(test_sm_stop_command);
    RUN_SM_TEST(test_sm_watchdog_cutoff);
    RUN_SM_TEST(test_sm_wire_packet_processing);

    std::cout << "====================================================\n";
    std::cout << "Receiver SM Summary: " << g_sm_tests_passed << "/" << g_sm_tests_run << " passed";
    if (g_sm_tests_failed > 0) {
        std::cout << " (" << g_sm_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_sm_tests_failed == 0) ? 0 : 1;
}
