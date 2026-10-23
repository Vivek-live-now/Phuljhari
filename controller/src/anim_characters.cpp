/**
 * @file anim_characters.cpp
 * @brief Implementation of animated characters and particle engines for M5StickS3 Controller ✨
 */

#include "anim_characters.h"
#include <cmath>
#include <cstring>

namespace phuljhari {
namespace controller {

// RGB565 Color Definitions
#define COL_GOLD        0xFEA0
#define COL_AMBER       0xFD20
#define COL_WARM_WHITE  0xFFDF
#define COL_PURE_WHITE  0xFFFF
#define COL_FACE_SKIN   0xFF75
#define COL_CHEEK_PINK  0xFAEA
#define COL_DEEP_RED    0xD800
#define COL_BRIGHT_RED  0xF800
#define COL_NEON_GREEN  0x07E0
#define COL_MAGENTA     0xF81F
#define COL_CYAN        0x07FF
#define COL_SKY_BLUE    0x54BF
#define COL_DARK_GRAY   0x2104

AnimCharacterEngine::AnimCharacterEngine() : frame_count_(0) {
    for (size_t i = 0; i < MAX_SPARKLES; ++i) {
        sparkles_[i].x = 0;
        sparkles_[i].y = 0;
        sparkles_[i].vx = 0;
        sparkles_[i].vy = 0;
        sparkles_[i].color = COL_GOLD;
        sparkles_[i].life = 0;
        sparkles_[i].active = false;
    }
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        fountain_[i].x = 0;
        fountain_[i].y = 0;
        fountain_[i].vx = 0;
        fountain_[i].vy = 0;
        fountain_[i].color = COL_GOLD;
        fountain_[i].active = false;
    }
    for (size_t i = 0; i < MAX_BURSTS; ++i) {
        bursts_[i].cx = 0;
        bursts_[i].cy = 0;
        bursts_[i].radius = 0;
        bursts_[i].color = COL_GOLD;
        bursts_[i].active = false;
    }
}

void AnimCharacterEngine::init() {
    frame_count_ = 0;
}

void AnimCharacterEngine::render_startup_screen(M5Canvas& canvas, uint32_t step) {
    canvas.fillScreen(TFT_BLACK);

    // Decorative sparkling stars in background
    for (int i = 0; i < 12; ++i) {
        int sx = (i * 37 + (int)step * 2) % PHULJHARI_DISP_WIDTH;
        int sy = (i * 29 + (int)(sin((step + i) * 0.1f) * 12.0f) + 40) % PHULJHARI_DISP_HEIGHT;
        canvas.drawPixel(sx, sy, (i % 2 == 0) ? COL_GOLD : COL_WARM_WHITE);
        if (i % 3 == 0) {
            canvas.drawPixel(sx + 1, sy, COL_AMBER);
            canvas.drawPixel(sx, sy + 1, COL_AMBER);
        }
    }

    // Glowing Golden Sparkler Wand Icon
    int cx = PHULJHARI_DISP_WIDTH / 2;
    int cy = 42;
    canvas.fillCircle(cx, cy, 14, COL_GOLD);
    canvas.fillCircle(cx, cy, 9, COL_WARM_WHITE);
    canvas.drawLine(cx, cy + 14, cx, cy + 32, COL_AMBER);
    canvas.drawLine(cx - 1, cy + 14, cx - 1, cy + 32, COL_GOLD);

    // Project Logo & Title
    canvas.setTextDatum(top_center);
    canvas.setTextColor(COL_GOLD);
    canvas.setTextSize(2);
    canvas.drawString("PHULJHARI *", cx, cy + 36);

    // Tagline
    canvas.setTextColor(COL_WARM_WHITE);
    canvas.setTextSize(1);
    canvas.drawString("Tiny sparks. Wireless smiles.", cx, cy + 58);

    // Loading / Sparkle Progress Bar
    int bar_w = 120;
    int bar_h = 4;
    int bar_x = (PHULJHARI_DISP_WIDTH - bar_w) / 2;
    int bar_y = cy + 74;
    canvas.drawRect(bar_x, bar_y, bar_w, bar_h, COL_DARK_GRAY);
    int progress = (int)((step * 4) % (bar_w - 2));
    canvas.fillRect(bar_x + 1, bar_y + 1, progress, bar_h - 2, COL_GOLD);
}

void AnimCharacterEngine::render_character(
    M5Canvas& canvas,
    protocol::EffectId effect,
    float tilt_norm,
    float tilt_angle_rad,
    bool trigger_virtual_burst,
    uint32_t current_time_ms
) {
    frame_count_++;
    int cx = PHULJHARI_DISP_WIDTH / 2;
    int cy = 70;

    if (trigger_virtual_burst) {
        // Trigger celebratory virtual burst on screen
        for (size_t i = 0; i < MAX_BURSTS; ++i) {
            if (!bursts_[i].active) {
                bursts_[i].active = true;
                bursts_[i].cx = (float)cx;
                bursts_[i].cy = (float)cy;
                bursts_[i].radius = 2.0f;
                bursts_[i].color = (effect == protocol::EFFECT_ID_PHULJHARI) ? COL_GOLD :
                                   (effect == protocol::EFFECT_ID_ANAR) ? COL_NEON_GREEN : COL_MAGENTA;
                break;
            }
        }
    }

    switch (effect) {
        case protocol::EFFECT_ID_PHULJHARI:
            render_phuljhari(canvas, cx, cy, tilt_norm, tilt_angle_rad, current_time_ms);
            break;
        case protocol::EFFECT_ID_ANAR:
            render_anar(canvas, cx, cy, tilt_norm, tilt_angle_rad, current_time_ms);
            break;
        case protocol::EFFECT_ID_PATAKHA:
            render_patakha(canvas, cx, cy, tilt_norm, tilt_angle_rad, current_time_ms);
            break;
        default:
            render_phuljhari(canvas, cx, cy, tilt_norm, tilt_angle_rad, current_time_ms);
            break;
    }

    // Render active shockwave bursts
    for (size_t i = 0; i < MAX_BURSTS; ++i) {
        if (bursts_[i].active) {
            bursts_[i].radius += 2.2f;
            canvas.drawCircle((int)bursts_[i].cx, (int)bursts_[i].cy, (int)bursts_[i].radius, bursts_[i].color);
            if (bursts_[i].radius > 45.0f) {
                bursts_[i].active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 1. Character: Phuljhari ✨ (Golden Sparkler Mascot)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_phuljhari(
    M5Canvas& canvas,
    int cx,
    int cy,
    float tilt_norm,
    float tilt_angle_rad,
    uint32_t current_time_ms
) {
    (void)tilt_norm;
    // Gentle floating breathing sway
    int sway_y = (int)(sin(current_time_ms * 0.005f) * 3.5f);
    int pivot_y = cy + 10 + sway_y;
    int pivot_x = cx;

    float theta = tilt_angle_rad;
    float sin_t = sin(theta);
    float cos_t = cos(theta);

    // Wand directional unit vector: points UPWARD along the rotated wand
    float ux = -sin_t;
    float uy = -cos_t;
    // Across directional unit vector: points RIGHTWARD across the wand
    float vx = cos_t;
    float vy = -sin_t;

    // 1. Sparkler Wand handle extending downwards from pivot
    int h_start_x = pivot_x - (int)(ux * 4.0f);
    int h_start_y = pivot_y - (int)(uy * 4.0f);
    int h_end_x   = pivot_x - (int)(ux * 26.0f);
    int h_end_y   = pivot_y - (int)(uy * 26.0f);

    canvas.drawLine(h_start_x, h_start_y, h_end_x, h_end_y, COL_DARK_GRAY);
    canvas.drawLine(h_start_x + (int)vx, h_start_y + (int)vy, h_end_x + (int)vx, h_end_y + (int)vy, COL_AMBER);

    // 2. Cute smiling sparkler head along rotated axis
    int char_x = pivot_x + (int)(ux * 10.0f);
    int char_y = pivot_y + (int)(uy * 10.0f);

    canvas.fillCircle(char_x, char_y, 14, COL_GOLD);
    canvas.fillCircle(char_x, char_y, 12, COL_FACE_SKIN);

    // Cute anime eyes (blink every 3 seconds), rotated with face
    bool blink = ((current_time_ms / 3000) % 2 == 0) && ((current_time_ms % 3000) < 150);
    int eye_l_x = char_x - (int)(vx * 4.0f) + (int)(ux * 2.0f);
    int eye_l_y = char_y - (int)(vy * 4.0f) + (int)(uy * 2.0f);
    int eye_r_x = char_x + (int)(vx * 4.0f) + (int)(ux * 2.0f);
    int eye_r_y = char_y + (int)(vy * 4.0f) + (int)(uy * 2.0f);

    if (blink) {
        canvas.drawLine(eye_l_x - (int)(vx * 2.0f), eye_l_y - (int)(vy * 2.0f),
                        eye_l_x + (int)(vx * 2.0f), eye_l_y + (int)(vy * 2.0f), TFT_BLACK);
        canvas.drawLine(eye_r_x - (int)(vx * 2.0f), eye_r_y - (int)(vy * 2.0f),
                        eye_r_x + (int)(vx * 2.0f), eye_r_y + (int)(vy * 2.0f), TFT_BLACK);
    } else {
        canvas.fillCircle(eye_l_x, eye_l_y, 2, TFT_BLACK);
        canvas.fillCircle(eye_r_x, eye_r_y, 2, TFT_BLACK);
        // Eye reflections
        canvas.drawPixel(eye_l_x - (int)vx, eye_l_y - (int)vy, COL_PURE_WHITE);
        canvas.drawPixel(eye_r_x - (int)vx, eye_r_y - (int)vy, COL_PURE_WHITE);
    }

    // Rosy blushing cheeks
    int cheek_l_x = char_x - (int)(vx * 7.0f) - (int)(ux * 3.0f);
    int cheek_l_y = char_y - (int)(vy * 7.0f) - (int)(uy * 3.0f);
    int cheek_r_x = char_x + (int)(vx * 7.0f) - (int)(ux * 3.0f);
    int cheek_r_y = char_y + (int)(vy * 7.0f) - (int)(uy * 3.0f);
    canvas.fillCircle(cheek_l_x, cheek_l_y, 2, COL_CHEEK_PINK);
    canvas.fillCircle(cheek_r_x, cheek_r_y, 2, COL_CHEEK_PINK);

    // Sweet smile
    int smile_x = char_x - (int)(ux * 4.0f);
    int smile_y = char_y - (int)(uy * 4.0f);
    canvas.drawPixel(smile_x - (int)(vx * 2.0f), smile_y - (int)(vy * 2.0f), COL_DEEP_RED);
    canvas.drawPixel(smile_x, smile_y, COL_DEEP_RED);
    canvas.drawPixel(smile_x + (int)(vx * 2.0f), smile_y + (int)(vy * 2.0f), COL_DEEP_RED);

    // Glowing sparkler wand tip at top of rotated wand
    int tip_x = char_x + (int)(ux * 14.0f);
    int tip_y = char_y + (int)(uy * 14.0f);
    canvas.fillCircle(tip_x, tip_y, 4, COL_WARM_WHITE);
    canvas.fillCircle(tip_x, tip_y, 2, COL_PURE_WHITE);

    // Spawn floating golden sparkle particles oriented along wand and tilt direction
    if (rand() % 10 < 4) {
        for (size_t i = 0; i < MAX_SPARKLES; ++i) {
            if (!sparkles_[i].active) {
                sparkles_[i].active = true;
                sparkles_[i].x = (float)tip_x;
                sparkles_[i].y = (float)tip_y;
                float angle = ((float)(rand() % 360)) * (3.14159f / 180.0f);
                float speed = ((float)(rand() % 140)) / 100.0f + 0.4f;
                // Eject velocity along wand orientation plus radial spread and tilt drift
                sparkles_[i].vx = cos(angle) * speed + (ux * 1.0f) + (sin_t * 0.8f);
                sparkles_[i].vy = sin(angle) * speed + (uy * 1.0f);
                sparkles_[i].color = (rand() % 2 == 0) ? COL_GOLD : COL_WARM_WHITE;
                sparkles_[i].life = (uint8_t)(rand() % 16 + 10);
                break;
            }
        }
    }

    // Update and draw sparkles: subtle tilt acceleration curves particle paths
    for (size_t i = 0; i < MAX_SPARKLES; ++i) {
        if (sparkles_[i].active) {
            sparkles_[i].vx += sin_t * 0.05f; // Sparkles drift naturally with tilt
            sparkles_[i].x += sparkles_[i].vx;
            sparkles_[i].y += sparkles_[i].vy;
            sparkles_[i].life--;
            canvas.drawPixel((int)sparkles_[i].x, (int)sparkles_[i].y, sparkles_[i].color);
            if (sparkles_[i].life == 0) {
                sparkles_[i].active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 2. Character: Anar 🎆 (Fountain Particle Mascot)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_anar(
    M5Canvas& canvas,
    int cx,
    int cy,
    float tilt_norm,
    float tilt_angle_rad,
    uint32_t current_time_ms
) {
    (void)tilt_norm;
    (void)current_time_ms;
    int base_x = cx;
    int base_y = cy + 24;

    float theta = tilt_angle_rad;
    float sin_t = sin(theta);
    float cos_t = cos(theta);

    // Upward axis of fountain cone
    float ux = -sin_t;
    float uy = -cos_t;
    // Across axis of fountain base
    float vx = cos_t;
    float vy = -sin_t;

    // Conical decorative pot corners
    int p_left_x  = base_x - (int)(vx * 18.0f);
    int p_left_y  = base_y - (int)(vy * 18.0f);
    int p_right_x = base_x + (int)(vx * 18.0f);
    int p_right_y = base_y + (int)(vy * 18.0f);
    int p_top_x   = base_x + (int)(ux * 32.0f);
    int p_top_y   = base_y + (int)(uy * 32.0f);

    canvas.fillTriangle(p_left_x, p_left_y, p_right_x, p_right_y, p_top_x, p_top_y, COL_DEEP_RED);

    // Decorative bands
    int band1_x0 = base_x - (int)(vx * 14.0f) + (int)(ux * 4.0f);
    int band1_y0 = base_y - (int)(vy * 14.0f) + (int)(uy * 4.0f);
    int band1_x1 = base_x + (int)(vx * 14.0f) + (int)(ux * 4.0f);
    int band1_y1 = base_y + (int)(vy * 14.0f) + (int)(uy * 4.0f);
    canvas.drawLine(band1_x0, band1_y0, band1_x1, band1_y1, COL_GOLD);

    int band2_x0 = base_x - (int)(vx * 10.0f) + (int)(ux * 12.0f);
    int band2_y0 = base_y - (int)(vy * 10.0f) + (int)(uy * 12.0f);
    int band2_x1 = base_x + (int)(vx * 10.0f) + (int)(ux * 12.0f);
    int band2_y1 = base_y + (int)(vy * 10.0f) + (int)(uy * 12.0f);
    canvas.drawLine(band2_x0, band2_y0, band2_x1, band2_y1, COL_AMBER);

    // Cute eyes on the pot
    int eye_c_x = base_x + (int)(ux * 18.0f);
    int eye_c_y = base_y + (int)(uy * 18.0f);
    int eye_l_x = eye_c_x - (int)(vx * 5.0f);
    int eye_l_y = eye_c_y - (int)(vy * 5.0f);
    int eye_r_x = eye_c_x + (int)(vx * 5.0f);
    int eye_r_y = eye_c_y + (int)(vy * 5.0f);

    canvas.fillCircle(eye_l_x, eye_l_y, 2, TFT_BLACK);
    canvas.fillCircle(eye_r_x, eye_r_y, 2, TFT_BLACK);
    canvas.drawPixel(eye_l_x - (int)vx, eye_l_y - (int)vy, COL_PURE_WHITE);
    canvas.drawPixel(eye_r_x - (int)vx, eye_r_y - (int)vy, COL_PURE_WHITE);

    // Smile
    int smile_x = base_x + (int)(ux * 12.0f);
    int smile_y = base_y + (int)(uy * 12.0f);
    canvas.drawPixel(smile_x, smile_y, COL_WARM_WHITE);

    // Nozzle opening at top
    int nozzle_x = p_top_x;
    int nozzle_y = p_top_y;
    canvas.fillCircle(nozzle_x, nozzle_y, 4, COL_AMBER);

    // Eject colorful fountain particles directed along nozzle axis
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        if (!fountain_[i].active && (rand() % 10 < 5)) {
            fountain_[i].active = true;
            fountain_[i].x = (float)nozzle_x;
            fountain_[i].y = (float)nozzle_y;
            float speed = ((float)(rand() % 180 + 120)) / 60.0f;
            float spread = ((float)(rand() % 120) - 60.0f) / 60.0f;
            // Ejection along rotated nozzle direction plus spread
            fountain_[i].vx = (ux * speed) + (vx * spread);
            fountain_[i].vy = (uy * speed) + (vy * spread);
            static const uint16_t FOUNTAIN_COLS[] = { COL_GOLD, COL_NEON_GREEN, COL_BRIGHT_RED, COL_MAGENTA, COL_CYAN };
            fountain_[i].color = FOUNTAIN_COLS[rand() % 5];
            break;
        }
    }

    // Update and draw fountain particles with gravity & tilt curvature
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        if (fountain_[i].active) {
            fountain_[i].vx += sin_t * 0.06f; // Tilt curves fountain stream
            fountain_[i].vy += 0.08f;        // Simulated gravity
            fountain_[i].x += fountain_[i].vx;
            fountain_[i].y += fountain_[i].vy;
            canvas.drawPixel((int)fountain_[i].x, (int)fountain_[i].y, fountain_[i].color);
            if (fountain_[i].y > (float)(base_y + 4)) {
                fountain_[i].active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 3. Character: Patakha 💥 (Pixel-Burst Cartoon Character)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_patakha(
    M5Canvas& canvas,
    int cx,
    int cy,
    float tilt_norm,
    float tilt_angle_rad,
    uint32_t current_time_ms
) {
    (void)tilt_norm;
    float theta = tilt_angle_rad;
    float sin_t = sin(theta);
    float cos_t = cos(theta);

    // Energetic bouncy squash-and-stretch hopping
    float hop = fabs(sin(current_time_ms * 0.008f));
    int hop_offset = (int)(hop * 7.0f);
    int char_y = cy + 12 - hop_offset;
    int char_x = cx + (int)(sin_t * 12.0f);

    int radius_x = 16 + (hop_offset > 0 ? -1 : 2);
    int radius_y = 16 + (hop_offset > 0 ? 2 : -2);

    float ux = -sin_t;
    float uy = -cos_t;
    float vx = cos_t;
    float vy = -sin_t;

    // Cute round festive mascot body
    canvas.fillEllipse(char_x, char_y, radius_x, radius_y, COL_BRIGHT_RED);
    // Festive golden sash / headband
    canvas.drawLine(char_x - (int)(vx * 12.0f) + (int)(ux * 8.0f),
                    char_y - (int)(vy * 12.0f) + (int)(uy * 8.0f),
                    char_x + (int)(vx * 12.0f) + (int)(ux * 8.0f),
                    char_y + (int)(vy * 12.0f) + (int)(uy * 8.0f), COL_GOLD);
    canvas.drawLine(char_x - (int)(vx * 12.0f) + (int)(ux * 7.0f),
                    char_y - (int)(vy * 12.0f) + (int)(uy * 7.0f),
                    char_x + (int)(vx * 12.0f) + (int)(ux * 7.0f),
                    char_y + (int)(vy * 12.0f) + (int)(uy * 7.0f), COL_GOLD);

    // Playful winking face
    // Left eye: open and sparkling
    int eye_l_x = char_x - (int)(vx * 5.0f) + (int)(ux * 1.0f);
    int eye_l_y = char_y - (int)(vy * 5.0f) + (int)(uy * 1.0f);
    canvas.fillCircle(eye_l_x, eye_l_y, 3, TFT_BLACK);
    canvas.drawPixel(eye_l_x - (int)vx, eye_l_y - (int)vy, COL_PURE_WHITE);

    // Right eye: playful happy wink '>'
    int eye_r_x = char_x + (int)(vx * 5.0f) + (int)(ux * 1.0f);
    int eye_r_y = char_y + (int)(vy * 5.0f) + (int)(uy * 1.0f);
    canvas.drawLine(eye_r_x - (int)(vx * 2.0f) + (int)(ux * 2.0f),
                    eye_r_y - (int)(vy * 2.0f) + (int)(uy * 2.0f),
                    eye_r_x + (int)(vx * 2.0f),
                    eye_r_y + (int)(vy * 2.0f), TFT_BLACK);
    canvas.drawLine(eye_r_x - (int)(vx * 2.0f) - (int)(ux * 2.0f),
                    eye_r_y - (int)(vy * 2.0f) - (int)(uy * 2.0f),
                    eye_r_x + (int)(vx * 2.0f),
                    eye_r_y + (int)(vy * 2.0f), TFT_BLACK);

    // Rosy blushing cheeks
    int cheek_l_x = char_x - (int)(vx * 9.0f) - (int)(ux * 4.0f);
    int cheek_l_y = char_y - (int)(vy * 9.0f) - (int)(uy * 4.0f);
    int cheek_r_x = char_x + (int)(vx * 9.0f) - (int)(ux * 4.0f);
    int cheek_r_y = char_y + (int)(vy * 9.0f) - (int)(uy * 4.0f);
    canvas.fillCircle(cheek_l_x, cheek_l_y, 2, COL_CHEEK_PINK);
    canvas.fillCircle(cheek_r_x, cheek_r_y, 2, COL_CHEEK_PINK);

    // Cheerful open smile
    int smile_x = char_x - (int)(ux * 4.0f);
    int smile_y = char_y - (int)(uy * 4.0f);
    canvas.fillCircle(smile_x, smile_y, 3, COL_DEEP_RED);
    canvas.drawPixel(smile_x, smile_y + 1, COL_WARM_WHITE);

    // Cartoon wick on top tilted with body
    int wick_base_x = char_x + (int)(ux * (radius_y));
    int wick_base_y = char_y + (int)(uy * (radius_y));
    int wick_tip_x  = char_x + (int)(ux * (radius_y + 7.0f));
    int wick_tip_y  = char_y + (int)(uy * (radius_y + 7.0f));
    canvas.drawLine(wick_base_x, wick_base_y, wick_tip_x, wick_tip_y, COL_DARK_GRAY);
    canvas.drawPixel(wick_tip_x, wick_tip_y, COL_WARM_WHITE);

    // Celebratory popping sparkles drifting with tilt
    if (rand() % 10 < 3) {
        int px = char_x + (rand() % 40) - 20 + (int)(sin_t * 6.0f);
        int py = char_y + (rand() % 40) - 20;
        canvas.drawPixel(px, py, (rand() % 2 == 0) ? COL_GOLD : COL_MAGENTA);
    }
}

} // namespace controller
} // namespace phuljhari
