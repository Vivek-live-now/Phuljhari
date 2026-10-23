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
    float tilt_x,
    float tilt_y,
    bool trigger_virtual_burst,
    uint32_t current_time_ms
) {
    (void)tilt_y;
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
            render_phuljhari(canvas, cx, cy, tilt_x, current_time_ms);
            break;
        case protocol::EFFECT_ID_ANAR:
            render_anar(canvas, cx, cy, tilt_x, current_time_ms);
            break;
        case protocol::EFFECT_ID_PATAKHA:
            render_patakha(canvas, cx, cy, tilt_x, current_time_ms);
            break;
        default:
            render_phuljhari(canvas, cx, cy, tilt_x, current_time_ms);
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
void AnimCharacterEngine::render_phuljhari(M5Canvas& canvas, int cx, int cy, float tilt_x, uint32_t current_time_ms) {
    // Gentle floating breathing sway
    int sway_y = (int)(sin(current_time_ms * 0.005f) * 3.5f);
    int char_y = cy + sway_y;
    int char_x = cx + (int)(tilt_x * 12.0f);

    // 1. Sparkler Wand handle
    canvas.drawLine(char_x, char_y + 12, char_x, char_y + 36, COL_DARK_GRAY);
    canvas.drawLine(char_x + 1, char_y + 12, char_x + 1, char_y + 36, COL_AMBER);

    // 2. Cute smiling sparkler head
    canvas.fillCircle(char_x, char_y, 14, COL_GOLD);
    canvas.fillCircle(char_x, char_y, 12, COL_FACE_SKIN);

    // Cute anime eyes (blink every 3 seconds)
    bool blink = ((current_time_ms / 3000) % 2 == 0) && ((current_time_ms % 3000) < 150);
    if (blink) {
        canvas.drawLine(char_x - 6, char_y - 2, char_x - 2, char_y - 2, TFT_BLACK);
        canvas.drawLine(char_x + 2, char_y - 2, char_x + 6, char_y - 2, TFT_BLACK);
    } else {
        canvas.fillCircle(char_x - 4, char_y - 2, 2, TFT_BLACK);
        canvas.fillCircle(char_x + 4, char_y - 2, 2, TFT_BLACK);
        // Eye reflections
        canvas.drawPixel(char_x - 5, char_y - 3, COL_PURE_WHITE);
        canvas.drawPixel(char_x + 3, char_y - 3, COL_PURE_WHITE);
    }

    // Rosy blushing cheeks
    canvas.fillCircle(char_x - 7, char_y + 3, 2, COL_CHEEK_PINK);
    canvas.fillCircle(char_x + 7, char_y + 3, 2, COL_CHEEK_PINK);

    // Sweet smile
    canvas.drawPixel(char_x - 2, char_y + 4, COL_DEEP_RED);
    canvas.drawPixel(char_x - 1, char_y + 5, COL_DEEP_RED);
    canvas.drawPixel(char_x,     char_y + 5, COL_DEEP_RED);
    canvas.drawPixel(char_x + 1, char_y + 5, COL_DEEP_RED);
    canvas.drawPixel(char_x + 2, char_y + 4, COL_DEEP_RED);

    // Glowing sparkler wand tip
    int tip_y = char_y - 18;
    canvas.fillCircle(char_x, tip_y, 4, COL_WARM_WHITE);
    canvas.fillCircle(char_x, tip_y, 2, COL_PURE_WHITE);

    // Spawn floating golden sparkle particles
    if (rand() % 10 < 4) {
        for (size_t i = 0; i < MAX_SPARKLES; ++i) {
            if (!sparkles_[i].active) {
                sparkles_[i].active = true;
                sparkles_[i].x = (float)char_x;
                sparkles_[i].y = (float)tip_y;
                float angle = ((float)(rand() % 360)) * (3.14159f / 180.0f);
                float speed = ((float)(rand() % 150)) / 100.0f + 0.4f;
                sparkles_[i].vx = cos(angle) * speed + tilt_x * 0.8f;
                sparkles_[i].vy = sin(angle) * speed - 0.5f;
                sparkles_[i].color = (rand() % 2 == 0) ? COL_GOLD : COL_WARM_WHITE;
                sparkles_[i].life = (uint8_t)(rand() % 16 + 10);
                break;
            }
        }
    }

    // Update and draw sparkles
    for (size_t i = 0; i < MAX_SPARKLES; ++i) {
        if (sparkles_[i].active) {
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
void AnimCharacterEngine::render_anar(M5Canvas& canvas, int cx, int cy, float tilt_x, uint32_t current_time_ms) {
    int char_y = cy + 10;
    int char_x = cx + (int)(tilt_x * 8.0f);

    // Conical decorative pot body
    canvas.fillTriangle(char_x - 18, char_y + 24, char_x + 18, char_y + 24, char_x, char_y - 8, COL_DEEP_RED);
    canvas.fillRect(char_x - 14, char_y + 20, 28, 4, COL_GOLD);
    canvas.fillRect(char_x - 10, char_y + 12, 20, 3, COL_AMBER);

    // Cute eyes on the pot
    canvas.fillCircle(char_x - 5, char_y + 6, 2, TFT_BLACK);
    canvas.fillCircle(char_x + 5, char_y + 6, 2, TFT_BLACK);
    canvas.drawPixel(char_x - 6, char_y + 5, COL_PURE_WHITE);
    canvas.drawPixel(char_x + 4, char_y + 5, COL_PURE_WHITE);

    // Cute smile
    canvas.drawPixel(char_x - 1, char_y + 11, COL_WARM_WHITE);
    canvas.drawPixel(char_x,     char_y + 12, COL_WARM_WHITE);
    canvas.drawPixel(char_x + 1, char_y + 11, COL_WARM_WHITE);

    // Nozzle opening
    int nozzle_y = char_y - 8;
    canvas.fillCircle(char_x, nozzle_y, 4, COL_AMBER);

    // Eject colorful fountain particles from top nozzle
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        if (!fountain_[i].active && (rand() % 10 < 5)) {
            fountain_[i].active = true;
            fountain_[i].x = (float)char_x;
            fountain_[i].y = (float)nozzle_y;
            fountain_[i].vx = ((float)(rand() % 160) - 80.0f) / 60.0f + tilt_x * 1.5f;
            fountain_[i].vy = -((float)(rand() % 180 + 120)) / 60.0f;
            static const uint16_t FOUNTAIN_COLS[] = { COL_GOLD, COL_NEON_GREEN, COL_BRIGHT_RED, COL_MAGENTA, COL_CYAN };
            fountain_[i].color = FOUNTAIN_COLS[rand() % 5];
            break;
        }
    }

    // Update and draw fountain particles with gravity
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        if (fountain_[i].active) {
            fountain_[i].x += fountain_[i].vx;
            fountain_[i].y += fountain_[i].vy;
            fountain_[i].vy += 0.08f; // Gravity
            canvas.drawPixel((int)fountain_[i].x, (int)fountain_[i].y, fountain_[i].color);
            if (fountain_[i].y > (float)(char_y + 24)) {
                fountain_[i].active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 3. Character: Patakha 💥 (Pixel-Burst Cartoon Character)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_patakha(M5Canvas& canvas, int cx, int cy, float tilt_x, uint32_t current_time_ms) {
    // Energetic bouncy squash-and-stretch hopping
    float hop = fabs(sin(current_time_ms * 0.008f));
    int hop_offset = (int)(hop * 7.0f);
    int char_y = cy + 12 - hop_offset;
    int char_x = cx + (int)(tilt_x * 10.0f);

    int radius_x = 16 + (hop_offset > 0 ? -1 : 2);
    int radius_y = 16 + (hop_offset > 0 ? 2 : -2);

    // Cute round festive mascot body
    canvas.fillEllipse(char_x, char_y, radius_x, radius_y, COL_BRIGHT_RED);
    // Festive golden sash / headband
    canvas.fillRect(char_x - 12, char_y - 8, 24, 4, COL_GOLD);

    // Playful winking face
    // Left eye: open and sparkling
    canvas.fillCircle(char_x - 5, char_y - 1, 3, TFT_BLACK);
    canvas.drawPixel(char_x - 6, char_y - 2, COL_PURE_WHITE);
    // Right eye: playful happy wink '>'
    canvas.drawLine(char_x + 3, char_y - 3, char_x + 7, char_y - 1, TFT_BLACK);
    canvas.drawLine(char_x + 3, char_y + 1, char_x + 7, char_y - 1, TFT_BLACK);

    // Rosy blushing cheeks
    canvas.fillCircle(char_x - 9, char_y + 4, 2, COL_CHEEK_PINK);
    canvas.fillCircle(char_x + 9, char_y + 4, 2, COL_CHEEK_PINK);

    // Cheerful open smile
    canvas.fillCircle(char_x, char_y + 4, 3, COL_DEEP_RED);
    canvas.drawPixel(char_x, char_y + 5, COL_WARM_WHITE);

    // Little cartoon wick on top with popping star
    canvas.drawLine(char_x, char_y - radius_y, char_x + 2, char_y - radius_y - 6, COL_DARK_GRAY);
    canvas.drawPixel(char_x + 2, char_y - radius_y - 7, COL_WARM_WHITE);

    // Celebratory popping sparkles around him
    if (rand() % 10 < 3) {
        int px = char_x + (rand() % 40) - 20;
        int py = char_y + (rand() % 40) - 20;
        canvas.drawPixel(px, py, (rand() % 2 == 0) ? COL_GOLD : COL_MAGENTA);
    }
}

} // namespace controller
} // namespace phuljhari
