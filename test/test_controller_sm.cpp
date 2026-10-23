/**
 * @file test_controller_sm.cpp
 * @brief Host-native unit test suite for Phuljhari 2.0 M5StickS3 Controller
 */

#include <iostream>
#include <cassert>
#include <cstring>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "controller_config.h"
#include "character_types.h"
#include "controller_settings.h"
#include "controller_sound.h"
#include "controller_flick_detector.h"
#include "controller_imu_math.h"
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
 * Test 1: 8 Selectable Festive Characters & Independent Cycling
 * ------------------------------------------------------------------------- */
static bool test_ctrl_8_characters_and_cycling() {
    ControllerStateMachine csm;
    csm.init(1000);

    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_PHULJHARI, "Default character should be Phuljhari (0)");
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_IDLE, "Character must start in IDLE");
    CTRL_ASSERT(csm.get_total_triggers() == 0, "Initial triggers should be 0");
    CTRL_ASSERT(!csm.is_button_a_held(), "Button A not held");
    CTRL_ASSERT(csm.get_screen_mode() == SCREEN_FESTIVAL, "Starts in festival screen mode");

    // Forward cycling through all 8 characters
    CharacterId expected_order[] = {
        CHAR_ANAR, CHAR_SKY_ROCKET, CHAR_GARLAND, CHAR_BOMB,
        CHAR_CHAKRI, CHAR_CANDLE, CHAR_DIYA, CHAR_PHULJHARI
    };

    for (size_t i = 0; i < 8; ++i) {
        csm.cycle_character_forward();
        CTRL_ASSERT(csm.get_selected_character_id() == expected_order[i], "Correct forward character cycle");
        CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_IDLE, "Cycled character must begin in IDLE");
    }

    // Backward cycling
    csm.cycle_character_backward();
    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_DIYA, "Cycled backward to Diya");
    csm.cycle_character_backward();
    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_CANDLE, "Cycled backward to Candle");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 2: Animation Lifecycle (IDLE -> PLAYING -> COOLDOWN -> IDLE)
 * ------------------------------------------------------------------------- */
static bool test_ctrl_animation_lifecycle() {
    ControllerStateMachine csm;
    csm.init(1000);

    // 1. Initially in IDLE
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_IDLE, "Starts in IDLE");
    CTRL_ASSERT(csm.get_anim_elapsed_ms(1000) == 0, "Elapsed ms is 0 in IDLE");

    // 2. Trigger animation at t = 1000ms
    bool started = csm.trigger_animation(1000);
    CTRL_ASSERT(started, "trigger_animation must succeed from IDLE");
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_PLAYING, "State transitions to PLAYING");
    CTRL_ASSERT(csm.get_anim_elapsed_ms(1500) == 500, "Elapsed time is 500ms");

    // 3. Repeated trigger attempts while PLAYING must be rejected
    bool retrigger = csm.trigger_animation(1500);
    CTRL_ASSERT(!retrigger, "Cannot retrigger while already PLAYING");

    // Phuljhari duration is 3500ms -> expires at 1000 + 3500 = 4500ms
    csm.tick(4499);
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_PLAYING, "Still PLAYING at 4499ms");

    csm.tick(4501);
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_COOLDOWN, "Transitions to COOLDOWN at 4501ms");

    // 4. Repeated trigger attempts while in COOLDOWN must be rejected
    retrigger = csm.trigger_animation(4600);
    CTRL_ASSERT(!retrigger, "Cannot trigger while in COOLDOWN");

    // Cooldown is 500ms -> expires at 4501 + 500 = 5001ms
    csm.tick(5002);
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_IDLE, "Returns to IDLE after cooldown");

    // 5. Switching characters during celebration safely resets to IDLE
    csm.trigger_animation(6000);
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_PLAYING, "Playing again");
    csm.cycle_character_forward();
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_IDLE, "Switching character resets immediately to IDLE");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 3: Dedicated Flick Detector
 * ------------------------------------------------------------------------- */
static bool test_ctrl_flick_detector() {
    FlickDetector detector;
    detector.init();
    detector.set_threshold(PHULJHARI_FLICK_SENS_MED_G); // 1.35G
    detector.set_cooldown(PHULJHARI_FLICK_COOLDOWN_MS);  // 700ms

    // 1. Static tilt / normal gravity (1.0G, no jerk) -> must NOT trigger
    bool flick = detector.process_sample(0.5f, 0.866f, 0.0f, 1000);
    CTRL_ASSERT(!flick, "Static tilt (1.0G) must NOT trigger flick");

    // 2. Slow wrist movement below threshold (1.1G) -> must NOT trigger
    flick = detector.process_sample(0.7f, 0.8f, 0.3f, 1050);
    CTRL_ASSERT(!flick, "Slow movement below threshold must NOT trigger");

    // 3. Deliberate sudden flick exceeding threshold (1.6G with rapid onset) -> triggers!
    flick = detector.process_sample(1.2f, 0.9f, 0.6f, 1100);
    CTRL_ASSERT(flick, "Deliberate flick (1.6G) must trigger flick detection");

    // 4. Follow-up shake during 700ms cooldown window -> must NOT trigger
    flick = detector.process_sample(1.4f, 1.0f, 0.5f, 1300); // 200ms after last flick
    CTRL_ASSERT(!flick, "Flick within cooldown window must be rejected");

    // 5. Subsequent flick after cooldown (1100 + 750 = 1850ms) -> triggers!
    flick = detector.process_sample(1.3f, 0.9f, 0.5f, 1850);
    CTRL_ASSERT(flick, "Flick after cooldown window must trigger");

    // 6. Disabled flick detector must never trigger
    detector.set_enabled(false);
    flick = detector.process_sample(2.0f, 2.0f, 2.0f, 2700);
    CTRL_ASSERT(!flick, "Disabled flick detector must NEVER trigger");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 4: Procedural Sound Manager & Volume Controls
 * ------------------------------------------------------------------------- */
static bool test_ctrl_sound_manager() {
    ControllerSoundManager sound;
    sound.begin();

    CTRL_ASSERT(sound.is_enabled(), "Sound enabled by default");
    CTRL_ASSERT(sound.get_volume() == 75, "Default volume 75%");
    CTRL_ASSERT(!sound.is_playing(), "No sound playing initially");

    // 1. All 8 Characters mapped to distinct valid sound IDs
    for (size_t i = 0; i < CHAR_COUNT; ++i) {
        CharacterId cid = (CharacterId)i;
        const CharacterDescriptor& desc = get_character_descriptor(cid);
        CTRL_ASSERT(desc.sound_id != SFX_NONE, "Character has designated sound ID");
    }
    CTRL_ASSERT(get_character_descriptor(CHAR_PHULJHARI).sound_id == SFX_PHULJHARI, "Phuljhari sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_ANAR).sound_id == SFX_ANAR, "Anar sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_SKY_ROCKET).sound_id == SFX_SKY_ROCKET, "Sky Rocket sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_GARLAND).sound_id == SFX_GARLAND, "Garland sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_BOMB).sound_id == SFX_BOMB, "Patakha Bomb sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_CHAKRI).sound_id == SFX_CHAKRI, "Chakri sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_CANDLE).sound_id == SFX_CANDLE, "Candle sound mapping");
    CTRL_ASSERT(get_character_descriptor(CHAR_DIYA).sound_id == SFX_DIYA, "Diya sound mapping");

    // 2. Playing procedural sound and verifying continuous generation
    sound.play(SFX_PHULJHARI, 1000);
    CTRL_ASSERT(sound.is_playing(), "Sound is playing");
    CTRL_ASSERT(sound.get_current_sound() == SFX_PHULJHARI, "Sound effect is Phuljhari");
    CTRL_ASSERT(sound.get_total_events() > 0, "First event generated immediately");

    // Advance through procedural updates
    uint32_t first_events = sound.get_total_events();
    sound.update(1080);
    sound.update(1200);
    CTRL_ASSERT(sound.is_playing(), "Phuljhari sizzle continues playing");
    CTRL_ASSERT(sound.get_total_events() > first_events, "New procedural crackles generated");

    // Full duration completion -> return to idle
    sound.update(5000); // 3500ms duration passed
    CTRL_ASSERT(!sound.is_playing(), "Sound stopped after sequence completed");

    // 3. Sound OFF silences audio immediately
    sound.play(SFX_ANAR, 6000);
    CTRL_ASSERT(sound.is_playing(), "Anar sound playing");
    sound.set_enabled(false);
    CTRL_ASSERT(!sound.is_playing(), "Sound OFF immediately silences output");

    // Playing while disabled must do nothing
    sound.play(SFX_BOMB, 6100);
    CTRL_ASSERT(!sound.is_playing(), "Play when disabled does nothing");

    // 4. Volume 0 is silent
    sound.set_enabled(true);
    sound.set_volume(0);
    sound.play(SFX_SKY_ROCKET, 6200);
    CTRL_ASSERT(!sound.is_playing(), "Volume 0 is strictly silent");

    // Restoring volume allows playback
    sound.set_volume(50);
    sound.play(SFX_SKY_ROCKET, 6300);
    CTRL_ASSERT(sound.is_playing(), "Volume 50% plays audio");

    // 5. Sound cancellation on active sound
    sound.stop();
    CTRL_ASSERT(!sound.is_playing(), "stop() cancels sound immediately");

    // 6. Test all 8 sound profiles can be played and progress
    SoundEffectId all_sfx[] = {
        SFX_PHULJHARI, SFX_ANAR, SFX_SKY_ROCKET, SFX_GARLAND,
        SFX_BOMB, SFX_CHAKRI, SFX_CANDLE, SFX_DIYA
    };
    for (size_t i = 0; i < 8; ++i) {
        sound.play(all_sfx[i], 7000 + i * 100);
        CTRL_ASSERT(sound.is_playing(), "Profile started playing");
        CTRL_ASSERT(sound.get_current_sound() == all_sfx[i], "Correct sound profile active");
        sound.update(7050 + i * 100);
        sound.stop();
        CTRL_ASSERT(!sound.is_playing(), "Sound cleanly stopped");
    }

    // 7. Rapid repeated triggers without queue overflow or clipping
    for (int i = 0; i < 50; ++i) {
        sound.play(SFX_GARLAND, 8000 + i * 10);
        CTRL_ASSERT(sound.is_playing(), "Rapid trigger accepted without crash or overflow");
    }
    sound.stop();

    // 8. Switching character cancels active sound in ControllerStateMachine
    ControllerStateMachine csm;
    csm.init(10000);
    csm.trigger_animation(10000);
    CTRL_ASSERT(csm.get_sound_manager().is_playing(), "Sound playing during celebration");
    csm.cycle_character_forward();
    CTRL_ASSERT(!csm.get_sound_manager().is_playing(), "Character change cancelled playing sound");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 5: Settings Page Navigation & Non-Resetting Selection
 * ------------------------------------------------------------------------- */
static bool test_ctrl_settings_navigation() {
    ControllerStateMachine csm;
    csm.init(1000);

    csm.set_selected_character_id(CHAR_CHAKRI);
    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_CHAKRI, "Selected Chakri");

    // Enter settings
    csm.enter_settings();
    CTRL_ASSERT(csm.get_screen_mode() == SCREEN_SETTINGS, "Screen mode is SCREEN_SETTINGS");
    CTRL_ASSERT(csm.get_settings_cursor() == 0, "Cursor at row 0 (Wireless)");

    // Cursor navigation
    csm.settings_next_item(); // 1 (Sound)
    csm.settings_next_item(); // 2 (Volume)
    CTRL_ASSERT(csm.get_settings_cursor() == 2, "Cursor at row 2");

    // Toggle volume: 75% -> 100%
    csm.settings_toggle_current();
    CTRL_ASSERT(csm.get_settings().volume_percent == 100, "Volume toggled to 100%");

    // Exit settings
    csm.exit_settings();
    CTRL_ASSERT(csm.get_screen_mode() == SCREEN_FESTIVAL, "Returned to SCREEN_FESTIVAL");

    // CRITICAL: Character selection must NOT have been reset!
    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_CHAKRI,
                "Character selection MUST be preserved when exiting settings!");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 6: Wireless Enable / Disable Behavior
 * ------------------------------------------------------------------------- */
static bool test_ctrl_wireless_toggle() {
    ControllerStateMachine csm;
    csm.init(1000);

    // Initial state: Wireless ON
    CTRL_ASSERT(csm.get_settings().wireless_enabled, "Wireless enabled by default");

    // Trigger celebration with wireless ON -> dispatches ACTION_SEND_CMD
    csm.trigger_animation(1000);
    CTRL_ASSERT(csm.has_pending_action(), "Action queued when wireless ON");
    OutgoingAction act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_CMD, "Queued ACTION_SEND_CMD");

    // Enter settings and toggle Wireless OFF
    csm.enter_settings();
    csm.settings_toggle_current(); // Cursor 0 toggles wireless
    CTRL_ASSERT(!csm.get_settings().wireless_enabled, "Wireless is now OFF");
    csm.exit_settings();

    // Advance to idle
    csm.tick(5000);

    // Trigger celebration with wireless OFF:
    // Local animation triggers, but NO radio actions must be queued!
    csm.trigger_animation(6000);
    CTRL_ASSERT(csm.get_animation_state() == CHAR_STATE_PLAYING, "Local animation triggers offline");
    CTRL_ASSERT(!csm.has_pending_action(), "No radio action queued when wireless OFF");
    CTRL_ASSERT(csm.get_link_health(6000) == LINK_OFFLINE, "Link reports OFFLINE when wireless disabled");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 7: Button A Hold & Strict GPIO 2 Synchronization
 * ------------------------------------------------------------------------- */
static bool test_ctrl_live_button_sync() {
    ControllerStateMachine csm;
    csm.init(1000);

    // Button down
    csm.on_button_a_down(1000);
    CTRL_ASSERT(csm.is_button_a_held(), "Button A held");
    CTRL_ASSERT(csm.has_pending_action(), "ACTION_SEND_BTN_DOWN queued");
    OutgoingAction act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_BTN_DOWN, "Action type is ACTION_SEND_BTN_DOWN");

    // Advance 185ms while held -> periodic BUTTON_SYNC queued
    csm.tick(1185);
    CTRL_ASSERT(csm.has_pending_action(), "Periodic sync queued");
    act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_BTN_SYNC, "Action type is ACTION_SEND_BTN_SYNC");

    // Button up after hold (> 300ms)
    csm.on_button_a_up(1400);
    CTRL_ASSERT(!csm.is_button_a_held(), "Button released");
    CTRL_ASSERT(csm.has_pending_action(), "ACTION_SEND_BTN_UP queued");
    act = csm.pop_pending_action();
    CTRL_ASSERT(act.type == ACTION_SEND_BTN_UP, "Action type is ACTION_SEND_BTN_UP");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 8: IMU Tilt & Cracker Selection Decoupling
 * ------------------------------------------------------------------------- */
static bool test_ctrl_imu_tilt_visual_rotation_only() {
    ControllerStateMachine csm;
    csm.init(1000);

    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_PHULJHARI, "Start with Phuljhari");

    // Apply tilt
    float tilt = 0.0f;
    for (int i = 0; i < 20; ++i) {
        tilt = ImuTiltMath::update_smooth_tilt(0.40f, tilt);
    }
    float rad = ImuTiltMath::tilt_to_rotation_rad(tilt);
    csm.set_sim_tilt(tilt, rad);

    CTRL_ASSERT(csm.get_sim_tilt_x() > 0.3f, "Visual tilt updated");
    CTRL_ASSERT(csm.get_selected_character_id() == CHAR_PHULJHARI, "Tilting does NOT change character selection!");
    CTRL_ASSERT(!csm.has_pending_action(), "Tilting does NOT send any radio command!");

    // Return to level
    for (int i = 0; i < 30; ++i) {
        tilt = ImuTiltMath::update_smooth_tilt(0.0f, tilt);
    }
    rad = ImuTiltMath::tilt_to_rotation_rad(tilt);
    csm.set_sim_tilt(tilt, rad);
    CTRL_ASSERT(csm.get_sim_tilt_x() == 0.0f, "Neutral orientation restored");

    return true;
}

/* -------------------------------------------------------------------------
 * Main Controller SM Test Runner
 * ------------------------------------------------------------------------- */
int main() {
    std::cout << "====================================================\n";
    std::cout << "Project Phuljhari 2.0 ✨ Controller State Machine Tests\n";
    std::cout << "====================================================\n";

    RUN_CTRL_TEST(test_ctrl_8_characters_and_cycling);
    RUN_CTRL_TEST(test_ctrl_animation_lifecycle);
    RUN_CTRL_TEST(test_ctrl_flick_detector);
    RUN_CTRL_TEST(test_ctrl_sound_manager);
    RUN_CTRL_TEST(test_ctrl_settings_navigation);
    RUN_CTRL_TEST(test_ctrl_wireless_toggle);
    RUN_CTRL_TEST(test_ctrl_live_button_sync);
    RUN_CTRL_TEST(test_ctrl_imu_tilt_visual_rotation_only);

    std::cout << "====================================================\n";
    std::cout << "Controller SM Summary: " << g_ctrl_tests_passed << "/" << g_ctrl_tests_run << " passed";
    if (g_ctrl_tests_failed > 0) {
        std::cout << " (" << g_ctrl_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_ctrl_tests_failed == 0) ? 0 : 1;
}
