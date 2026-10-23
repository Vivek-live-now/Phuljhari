/**
 * @file anim_characters.cpp
 * @brief Implementation of Phuljhari 2.0 festive characters, backgrounds, and particles ✨
 */

#include "anim_characters.h"
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace phuljhari {
namespace controller {

// RGB565 Palette
#define COL_BLACK       0x0000
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
#define COL_MIDNIGHT    0x0845
#define COL_INDIGO      0x18C8
#define COL_TERRACOTTA  0x8A03
#define COL_MAROON      0x5802
#define COL_BRASS       0xCE60

AnimCharacterEngine::AnimCharacterEngine()
    : rangoli_next_idx_(0),
      frame_count_(0) {
    for (size_t i = 0; i < MAX_SPARKLES; ++i) {
        sparkles_[i] = { 0, 0, 0, 0, COL_GOLD, 0, false };
    }
    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        fountain_[i] = { 0, 0, 0, 0, COL_GOLD, false };
    }
    for (size_t i = 0; i < MAX_RANGOLI; ++i) {
        rangoli_[i] = { 0, 0, COL_GOLD, 0, false };
    }
}

void AnimCharacterEngine::init() {
    frame_count_ = 0;
    rangoli_next_idx_ = 0;
}

#if defined(ARDUINO)

void AnimCharacterEngine::render_startup_screen(M5Canvas& canvas, uint32_t step) {
    canvas.fillScreen(COL_MIDNIGHT);

    // Decorative sparkling stars in background
    for (int i = 0; i < 16; ++i) {
        int sx = (i * 29 + (int)step * 2) % PHULJHARI_DISP_WIDTH;
        int sy = (i * 37 + (int)(sin((step + i) * 0.1f) * 14.0f) + 30) % PHULJHARI_DISP_HEIGHT;
        canvas.drawPixel(sx, sy, (i % 2 == 0) ? COL_GOLD : COL_WARM_WHITE);
    }

    int cx = PHULJHARI_DISP_WIDTH / 2;
    int cy = 75;

    // Glowing Golden Sparkler Wand Icon (Portrait)
    canvas.fillCircle(cx, cy, 18, COL_GOLD);
    canvas.fillCircle(cx, cy, 12, COL_WARM_WHITE);
    canvas.drawLine(cx, cy + 18, cx, cy + 42, COL_AMBER);
    canvas.drawLine(cx - 1, cy + 18, cx - 1, cy + 42, COL_GOLD);

    // Logo & Tagline
    canvas.setTextDatum(top_center);
    canvas.setTextColor(COL_GOLD);
    canvas.setTextSize(2);
    canvas.drawString("PHULJHARI", cx, cy + 48);

    canvas.setTextColor(COL_WARM_WHITE);
    canvas.setTextSize(1);
    canvas.drawString("2.0 POCKET DIWALI", cx, cy + 68);
    canvas.setTextColor(COL_AMBER);
    canvas.drawString("Tiny sparks. Wireless smiles.", cx, cy + 82);

    // Loading Progress Bar
    int bar_w = 90;
    int bar_h = 4;
    int bar_x = (PHULJHARI_DISP_WIDTH - bar_w) / 2;
    int bar_y = cy + 102;
    canvas.drawRect(bar_x, bar_y, bar_w, bar_h, COL_DARK_GRAY);
    int progress = (int)((step * 3) % (bar_w - 2));
    canvas.fillRect(bar_x + 1, bar_y + 1, progress, bar_h - 2, COL_GOLD);
}

void AnimCharacterEngine::render_background(M5Canvas& canvas, BackgroundTheme theme, bool power_save, uint32_t current_time_ms) {
    switch (theme) {
        case BG_DIYA_COURTYARD:
            render_bg_courtyard(canvas, power_save, current_time_ms);
            break;
        case BG_FESTIVE_BAZAAR:
            render_bg_bazaar(canvas, power_save, current_time_ms);
            break;
        case BG_MOONLIT_ROOFTOP:
        default:
            render_bg_rooftop(canvas, power_save, current_time_ms);
            break;
    }
}

void AnimCharacterEngine::render_bg_rooftop(M5Canvas& canvas, bool power_save, uint32_t current_time_ms) {
    canvas.fillScreen(COL_MIDNIGHT);

    // Crescent Moon in top-right
    canvas.fillCircle(112, 38, 12, COL_WARM_WHITE);
    canvas.fillCircle(107, 34, 11, COL_MIDNIGHT);

    // Night stars (subtle twinkle)
    if (!power_save) {
        for (int i = 0; i < 8; ++i) {
            int sx = (i * 23 + 17) % (PHULJHARI_DISP_WIDTH - 10);
            int sy = (i * 19 + 25) % 110;
            if (((current_time_ms / 600 + i) % 3) != 0) {
                canvas.drawPixel(sx, sy, COL_GOLD);
            }
        }
    }

    // Distant rooftop skyline silhouette
    canvas.fillRect(0, 185, 35, 55, COL_DARK_GRAY);
    canvas.fillRect(35, 175, 45, 65, 0x18C3);
    canvas.fillRect(80, 182, 55, 58, COL_DARK_GRAY);

    // Decorative balcony railing
    canvas.drawLine(0, 188, PHULJHARI_DISP_WIDTH, 188, COL_INDIGO);
    for (int x = 6; x < PHULJHARI_DISP_WIDTH; x += 14) {
        canvas.drawLine(x, 188, x, 202, COL_INDIGO);
    }
}

void AnimCharacterEngine::render_bg_courtyard(M5Canvas& canvas, bool power_save, uint32_t current_time_ms) {
    canvas.fillScreen(COL_TERRACOTTA);

    // Decorative courtyard arch
    canvas.drawFastHLine(10, 32, PHULJHARI_DISP_WIDTH - 20, COL_AMBER);
    canvas.drawFastHLine(14, 34, PHULJHARI_DISP_WIDTH - 28, COL_GOLD);

    // Two small ambient boundary lamps (diyas) on left and right ledges
    int flame_flicker = (current_time_ms / 120) % 2;
    // Left diya
    canvas.fillEllipse(18, 192, 9, 4, COL_BRASS);
    canvas.fillCircle(18, 187 - flame_flicker, 2, COL_AMBER);
    canvas.drawPixel(18, 184 - flame_flicker, COL_PURE_WHITE);

    // Right diya
    canvas.fillEllipse(117, 192, 9, 4, COL_BRASS);
    canvas.fillCircle(117, 187 - (1 - flame_flicker), 2, COL_AMBER);
    canvas.drawPixel(117, 184 - (1 - flame_flicker), COL_PURE_WHITE);

    // Courtyard stone floor line
    canvas.drawLine(0, 196, PHULJHARI_DISP_WIDTH, 196, COL_AMBER);
    (void)power_save;
}

void AnimCharacterEngine::render_bg_bazaar(M5Canvas& canvas, bool power_save, uint32_t current_time_ms) {
    canvas.fillScreen(COL_MAROON);

    // Celebratory fairy light string along the top
    for (int x = 10; x < PHULJHARI_DISP_WIDTH - 10; x += 18) {
        uint16_t light_col = ((x / 18) % 3 == 0) ? COL_GOLD :
                             ((x / 18) % 3 == 1) ? COL_NEON_GREEN : COL_CYAN;
        canvas.fillCircle(x, 28, 2, light_col);
    }

    // Traditional hanging paper lanterns (kandils) in top corners
    // Left kandil
    canvas.fillRect(16, 32, 10, 10, COL_GOLD);
    canvas.drawLine(18, 42, 18, 52, COL_AMBER);
    canvas.drawLine(24, 42, 24, 52, COL_AMBER);

    // Right kandil
    canvas.fillRect(108, 32, 10, 10, COL_MAGENTA);
    canvas.drawLine(110, 42, 110, 52, COL_GOLD);
    canvas.drawLine(116, 42, 116, 52, COL_GOLD);

    // Floor horizon
    canvas.drawLine(0, 195, PHULJHARI_DISP_WIDTH, 195, COL_GOLD);
    (void)power_save;
    (void)current_time_ms;
}

void AnimCharacterEngine::render_rangoli(M5Canvas& canvas, bool active, uint32_t current_time_ms) {
    if (!active) return;

    int rx = PHULJHARI_DISP_WIDTH / 2;
    int ry = 190;
    uint16_t col = COL_GOLD;

    // Glowing symmetrical rangoli mandala on floor
    float pulse = 0.5f + 0.5f * sin(current_time_ms * 0.004f);
    int rad = 14 + (int)(pulse * 3.0f);

    canvas.drawCircle(rx, ry, rad, col);
    canvas.drawCircle(rx, ry, rad - 5, COL_AMBER);
    // Petal ticks
    for (int a = 0; a < 8; ++a) {
        float ang = ((float)a * 45.0f) * (3.14159f / 180.0f);
        int px = rx + (int)(cos(ang) * rad);
        int py = ry + (int)(sin(ang) * (rad * 0.5f));
        canvas.drawPixel(px, py, COL_WARM_WHITE);
    }
}

void AnimCharacterEngine::deposit_rangoli_petal(uint16_t color) {
    rangoli_[rangoli_next_idx_].angle = (float)(rand() % 360) * (3.14159f / 180.0f);
    rangoli_[rangoli_next_idx_].radius = (float)(rand() % 16 + 8);
    rangoli_[rangoli_next_idx_].color = color;
    rangoli_[rangoli_next_idx_].alpha = 255;
    rangoli_[rangoli_next_idx_].active = true;
    rangoli_next_idx_ = (rangoli_next_idx_ + 1) % MAX_RANGOLI;
}

void AnimCharacterEngine::render_character(
    M5Canvas& canvas,
    CharacterId character,
    AnimationState state,
    float tilt_norm,
    float tilt_angle_rad,
    uint32_t elapsed_ms,
    uint32_t current_time_ms
) {
    (void)tilt_norm;
    frame_count_++;
    int cx = PHULJHARI_DISP_WIDTH / 2; // 67
    int cy = 115;                       // Prominent center stage

    switch (character) {
        case CHAR_PHULJHARI:
            render_phuljhari(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_ANAR:
            render_anar(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_SKY_ROCKET:
            render_sky_rocket(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_GARLAND:
            render_garland(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_BOMB:
            render_bomb(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_CHAKRI:
            render_chakri(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_CANDLE:
            render_candle(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        case CHAR_DIYA:
            render_diya(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
        default:
            render_phuljhari(canvas, cx, cy, state, tilt_angle_rad, elapsed_ms, current_time_ms);
            break;
    }
}

/* -------------------------------------------------------------------------
 * 1. Character: Phuljhari ✨ (Golden Wand)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_phuljhari(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)elapsed;
    float sin_t = sin(theta);
    float cos_t = cos(theta);
    float ux = -sin_t, uy = -cos_t; // Upward along wand
    float vx = cos_t, vy = -sin_t;  // Across wand

    int sway = (state == CHAR_STATE_IDLE) ? (int)(sin(now * 0.005f) * 2.0f) : 0;
    int px = cx;
    int py = cy + 20 + sway;

    // Handle
    int h0x = px - (int)(ux * 6.0f), h0y = py - (int)(uy * 6.0f);
    int h1x = px - (int)(ux * 42.0f), h1y = py - (int)(uy * 42.0f);
    canvas.drawLine(h0x, h0y, h1x, h1y, COL_DARK_GRAY);
    canvas.drawLine(h0x + (int)vx, h0y + (int)vy, h1x + (int)vx, h1y + (int)vy, COL_AMBER);

    // Large Sparkler Head
    int head_x = px + (int)(ux * 14.0f);
    int head_y = py + (int)(uy * 14.0f);
    canvas.fillCircle(head_x, head_y, 18, COL_GOLD);
    canvas.fillCircle(head_x, head_y, 15, COL_FACE_SKIN);

    // Cute Anime Face (blink every 3s)
    bool blink = ((now / 3000) % 2 == 0) && ((now % 3000) < 140);
    int elx = head_x - (int)(vx * 5.0f) + (int)(ux * 2.0f);
    int ely = head_y - (int)(vy * 5.0f) + (int)(uy * 2.0f);
    int erx = head_x + (int)(vx * 5.0f) + (int)(ux * 2.0f);
    int ery = head_y + (int)(vy * 5.0f) + (int)(uy * 2.0f);
    if (blink) {
        canvas.drawLine(elx - 2, ely, elx + 2, ely, COL_BLACK);
        canvas.drawLine(erx - 2, ery, erx + 2, ery, COL_BLACK);
    } else {
        canvas.fillCircle(elx, ely, 2, COL_BLACK);
        canvas.fillCircle(erx, ery, 2, COL_BLACK);
        canvas.drawPixel(elx - 1, ely - 1, COL_PURE_WHITE);
        canvas.drawPixel(erx - 1, ery - 1, COL_PURE_WHITE);
    }
    // Cheeks & Smile
    canvas.fillCircle(head_x - (int)(vx * 9.0f), head_y - (int)(vy * 9.0f), 2, COL_CHEEK_PINK);
    canvas.fillCircle(head_x + (int)(vx * 9.0f), head_y + (int)(vy * 9.0f), 2, COL_CHEEK_PINK);
    canvas.drawPixel(head_x - (int)(ux * 5.0f), head_y - (int)(uy * 5.0f), COL_DEEP_RED);

    // Tip
    int tip_x = head_x + (int)(ux * 18.0f);
    int tip_y = head_y + (int)(uy * 18.0f);

    if (state == CHAR_STATE_IDLE) {
        // Soft glowing tip in idle
        int glow = (now / 150) % 2;
        canvas.fillCircle(tip_x, tip_y, 3 + glow, COL_AMBER);
        canvas.drawPixel(tip_x, tip_y, COL_PURE_WHITE);
    } else if (state == CHAR_STATE_PLAYING) {
        // Active radiant sparks shooting out
        canvas.fillCircle(tip_x, tip_y, 5, COL_WARM_WHITE);
        canvas.fillCircle(tip_x, tip_y, 3, COL_PURE_WHITE);

        for (size_t i = 0; i < MAX_SPARKLES; ++i) {
            if (!sparkles_[i].active && (rand() % 10 < 6)) {
                sparkles_[i].active = true;
                sparkles_[i].x = (float)tip_x;
                sparkles_[i].y = (float)tip_y;
                float ang = ((float)(rand() % 360)) * (3.14159f / 180.0f);
                float spd = ((float)(rand() % 160)) / 100.0f + 0.6f;
                sparkles_[i].vx = cos(ang) * spd + ux * 1.4f + sin_t * 0.9f;
                sparkles_[i].vy = sin(ang) * spd + uy * 1.4f;
                sparkles_[i].color = (rand() % 2 == 0) ? COL_GOLD : COL_WARM_WHITE;
                sparkles_[i].life = (uint8_t)(rand() % 18 + 12);
                break;
            }
        }
    }

    // Update particles (active in PLAYING and fading in COOLDOWN)
    for (size_t i = 0; i < MAX_SPARKLES; ++i) {
        if (sparkles_[i].active) {
            sparkles_[i].vx += sin_t * 0.05f;
            sparkles_[i].x += sparkles_[i].vx;
            sparkles_[i].y += sparkles_[i].vy;
            sparkles_[i].life--;
            canvas.drawPixel((int)sparkles_[i].x, (int)sparkles_[i].y, sparkles_[i].color);
            if (sparkles_[i].life == 0) sparkles_[i].active = false;
        }
    }
}

/* -------------------------------------------------------------------------
 * 2. Character: Anar 🎆 (Fountain Pot)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_anar(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)elapsed;
    float sin_t = sin(theta), cos_t = cos(theta);
    float ux = -sin_t, uy = -cos_t;
    float vx = cos_t, vy = -sin_t;

    int bx = cx, by = cy + 35;
    // Pot body
    int lx = bx - (int)(vx * 24.0f), ly = by - (int)(vy * 24.0f);
    int rx = bx + (int)(vx * 24.0f), ry = by + (int)(vy * 24.0f);
    int tx = bx + (int)(ux * 42.0f), ty = by + (int)(uy * 42.0f);
    canvas.fillTriangle(lx, ly, rx, ry, tx, ty, COL_DEEP_RED);

    // Decorative Bands
    canvas.drawLine(bx - (int)(vx * 18.0f) + (int)(ux * 8.0f), by - (int)(vy * 18.0f) + (int)(uy * 8.0f),
                    bx + (int)(vx * 18.0f) + (int)(ux * 8.0f), by + (int)(vy * 18.0f) + (int)(uy * 8.0f), COL_GOLD);
    canvas.drawLine(bx - (int)(vx * 12.0f) + (int)(ux * 18.0f), by - (int)(vy * 12.0f) + (int)(uy * 18.0f),
                    bx + (int)(vx * 12.0f) + (int)(ux * 18.0f), by + (int)(vy * 12.0f) + (int)(uy * 18.0f), COL_AMBER);

    // Cute pot face
    int fc_x = bx + (int)(ux * 24.0f), fc_y = by + (int)(uy * 24.0f);
    canvas.fillCircle(fc_x - (int)(vx * 6.0f), fc_y - (int)(vy * 6.0f), 2, COL_BLACK);
    canvas.fillCircle(fc_x + (int)(vx * 6.0f), fc_y + (int)(vy * 6.0f), 2, COL_BLACK);
    canvas.drawPixel(fc_x + (int)(ux * 2.0f), fc_y + (int)(uy * 2.0f), COL_WARM_WHITE);

    // Nozzle
    canvas.fillCircle(tx, ty, 5, COL_AMBER);

    if (state == CHAR_STATE_IDLE) {
        int glow = (now / 200) % 2;
        canvas.fillCircle(tx, ty, 3 + glow, COL_GOLD);
    } else if (state == CHAR_STATE_PLAYING) {
        // Eject tall colorful fountain stream
        for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
            if (!fountain_[i].active && (rand() % 10 < 7)) {
                fountain_[i].active = true;
                fountain_[i].x = (float)tx;
                fountain_[i].y = (float)ty;
                float spd = ((float)(rand() % 220 + 160)) / 60.0f;
                float spr = ((float)(rand() % 140) - 70.0f) / 60.0f;
                fountain_[i].vx = (ux * spd) + (vx * spr);
                fountain_[i].vy = (uy * spd) + (vy * spr);
                static const uint16_t FOUNT_COLS[] = { COL_GOLD, COL_NEON_GREEN, COL_BRIGHT_RED, COL_MAGENTA, COL_CYAN };
                fountain_[i].color = FOUNT_COLS[rand() % 5];
                break;
            }
        }
    }

    for (size_t i = 0; i < MAX_FOUNTAIN; ++i) {
        if (fountain_[i].active) {
            fountain_[i].vx += sin_t * 0.06f;
            fountain_[i].vy += 0.09f; // Gravity
            fountain_[i].x += fountain_[i].vx;
            fountain_[i].y += fountain_[i].vy;
            canvas.drawPixel((int)fountain_[i].x, (int)fountain_[i].y, fountain_[i].color);
            if (fountain_[i].y > (float)(by + 6)) fountain_[i].active = false;
        }
    }
}

/* -------------------------------------------------------------------------
 * 3. Character: Sky Rocket 🚀 (Star Rocket)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_sky_rocket(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)now;
    float sin_t = sin(theta), cos_t = cos(theta);
    float ux = -sin_t, uy = -cos_t;
    float vx = cos_t, vy = -sin_t;

    int rx = cx, ry = cy + 10;
    if (state == CHAR_STATE_PLAYING) {
        // Rocket lifts off upward across the portrait screen!
        int launch_y = (int)((elapsed * 120) / 1000);
        if (launch_y > 80) launch_y = 80;
        ry -= launch_y;
    }

    // Guide stick
    canvas.drawLine(rx, ry, rx - (int)(ux * 45.0f), ry - (int)(uy * 45.0f), COL_DARK_GRAY);

    // Rocket body
    int b0x = rx - (int)(vx * 6.0f), b0y = ry - (int)(vy * 6.0f);
    int b1x = rx + (int)(vx * 6.0f), b1y = ry + (int)(vy * 6.0f);
    int t0x = rx - (int)(vx * 6.0f) + (int)(ux * 30.0f), t0y = ry - (int)(vy * 6.0f) + (int)(uy * 30.0f);
    int t1x = rx + (int)(vx * 6.0f) + (int)(ux * 30.0f), t1y = ry + (int)(vy * 6.0f) + (int)(uy * 30.0f);
    canvas.fillTriangle(b0x, b0y, b1x, b1y, t0x, t0y, COL_BRIGHT_RED);
    canvas.fillTriangle(b1x, b1y, t0x, t0y, t1x, t1y, COL_BRIGHT_RED);

    // Conical Nose Cone
    int ncx = rx + (int)(ux * 44.0f), ncy = ry + (int)(uy * 44.0f);
    canvas.fillTriangle(t0x, t0y, t1x, t1y, ncx, ncy, COL_GOLD);

    // Cute Rocket Face
    int fc_x = rx + (int)(ux * 15.0f), fc_y = ry + (int)(uy * 15.0f);
    canvas.fillCircle(fc_x - 2, fc_y, 1, COL_PURE_WHITE);
    canvas.fillCircle(fc_x + 2, fc_y, 1, COL_PURE_WHITE);

    if (state == CHAR_STATE_IDLE) {
        // Fuse trailing from tail
        canvas.drawLine(rx, ry, rx + 4, ry + 8, COL_AMBER);
    } else if (state == CHAR_STATE_PLAYING) {
        // Tail exhaust trail
        canvas.fillCircle(rx, ry + 4, 3, COL_GOLD);
        canvas.fillCircle(rx, ry + 8, 2, COL_WARM_WHITE);

        // Apex burst after rocket ascends
        if (elapsed > 1200) {
            for (int i = 0; i < 8; ++i) {
                float a = (float)i * (3.14159f / 4.0f);
                int rad = (int)((elapsed - 1200) / 45);
                if (rad < 38) {
                    canvas.drawPixel(ncx + (int)(cos(a) * rad), ncy + (int)(sin(a) * rad), COL_GOLD);
                    canvas.drawPixel(ncx + (int)(cos(a) * (rad - 3)), ncy + (int)(sin(a) * (rad - 3)), COL_CYAN);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 4. Character: Garland Crackers 🧨 (Ladi Pops)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_garland(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)elapsed;
    (void)now;
    float sin_t = sin(theta);
    int top_x = cx + (int)(sin_t * 10.0f);
    int top_y = cy - 25;

    // Hanging garland string with paired red cylinders
    canvas.drawLine(top_x, top_y - 6, top_x, top_y + 60, COL_GOLD);

    for (int i = 0; i < 6; ++i) {
        int py = top_y + i * 10;
        int px = top_x + ((i % 2 == 0) ? -8 : 8);

        // Individual red firecracker tube
        canvas.fillRect(px - 4, py, 8, 7, COL_BRIGHT_RED);
        canvas.drawFastHLine(px - 4, py + 3, 8, COL_GOLD);

        if (state == CHAR_STATE_PLAYING) {
            // Sequential popping sparks
            if ((elapsed / 250) % 6 == (uint32_t)i) {
                canvas.fillCircle(px, py + 3, 4, COL_WARM_WHITE);
                canvas.drawCircle(px, py + 3, 7, COL_GOLD);
            }
        }
    }

    if (state == CHAR_STATE_IDLE) {
        // Golden hanging bottom tassel
        canvas.fillTriangle(top_x - 5, top_y + 62, top_x + 5, top_y + 62, top_x, top_y + 70, COL_GOLD);
    }
}

/* -------------------------------------------------------------------------
 * 5. Character: Patakha Bomb 💣 (Friendly Mascot)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_bomb(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    float sin_t = sin(theta);
    int hop = (state == CHAR_STATE_PLAYING && elapsed < 800) ? (int)(sin(elapsed * 0.01f) * 10.0f) : 0;
    int bx = cx + (int)(sin_t * 8.0f);
    int by = cy + 12 - hop;

    // Friendly round mascot body
    canvas.fillCircle(bx, by, 22, COL_BRIGHT_RED);
    // Green festive wrapping tape band
    canvas.fillRect(bx - 18, by - 4, 36, 8, COL_NEON_GREEN);

    // Cute winking anime face
    canvas.fillCircle(bx - 7, by - 1, 3, COL_BLACK);
    canvas.drawPixel(bx - 8, by - 2, COL_PURE_WHITE);
    // Winking right eye
    canvas.drawLine(bx + 4, by - 3, bx + 9, by, COL_BLACK);
    canvas.drawLine(bx + 4, by + 1, bx + 9, by - 2, COL_BLACK);
    // Cheeks & open smile
    canvas.fillCircle(bx - 12, by + 5, 2, COL_CHEEK_PINK);
    canvas.fillCircle(bx + 12, by + 5, 2, COL_CHEEK_PINK);
    canvas.fillCircle(bx, by + 6, 3, COL_DEEP_RED);

    // Wick on top
    int wx = bx + 2, wy = by - 22;
    canvas.drawLine(bx, by - 22, wx, wy - 8, COL_DARK_GRAY);

    if (state == CHAR_STATE_IDLE) {
        // Glowing star spark on fuse
        int pulse = (now / 150) % 2;
        canvas.drawPixel(wx, wy - 9 - pulse, COL_WARM_WHITE);
    } else if (state == CHAR_STATE_PLAYING) {
        // Harmless celebratory confetti starburst!
        if (elapsed > 600) {
            int rad = (int)((elapsed - 600) / 30);
            if (rad < 45) {
                for (int a = 0; a < 8; ++a) {
                    float ang = (float)a * (3.14159f / 4.0f);
                    int px = bx + (int)(cos(ang) * rad);
                    int py = by + (int)(sin(ang) * rad);
                    uint16_t c = (a % 2 == 0) ? COL_GOLD : COL_MAGENTA;
                    canvas.fillCircle(px, py, 2, c);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * 6. Character: Chakri 🌀 (Spinning Wheel)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_chakri(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)theta;
    int wx = cx, wy = cy + 20;

    // Ground shadow
    canvas.fillEllipse(wx, wy + 16, 26, 7, COL_DARK_GRAY);

    // Spinning angle
    float spin_angle = (state == CHAR_STATE_PLAYING) ? ((float)elapsed * 0.04f) : ((float)now * 0.002f);

    // Concentric spiral disc
    canvas.fillCircle(wx, wy, 20, COL_GOLD);
    canvas.fillCircle(wx, wy, 16, COL_DEEP_RED);
    canvas.fillCircle(wx, wy, 11, COL_NEON_GREEN);
    canvas.fillCircle(wx, wy, 6, COL_GOLD);

    // Radiant spokes spinning
    for (int i = 0; i < 4; ++i) {
        float a = spin_angle + (float)i * (3.14159f / 2.0f);
        int sx = wx + (int)(cos(a) * 18.0f);
        int sy = wy + (int)(sin(a) * 18.0f);
        canvas.drawLine(wx, wy, sx, sy, COL_PURE_WHITE);
    }

    if (state == CHAR_STATE_PLAYING) {
        // Circular spark rings flung outward!
        for (int i = 0; i < 6; ++i) {
            float a = spin_angle * 1.5f + (float)i * (3.14159f / 3.0f);
            int spk_x = wx + (int)(cos(a) * 26.0f);
            int spk_y = wy + (int)(sin(a) * 22.0f);
            canvas.drawPixel(spk_x, spk_y, COL_GOLD);
            canvas.drawPixel(spk_x + 1, spk_y, COL_WARM_WHITE);
        }
    }
}

/* -------------------------------------------------------------------------
 * 7. Character: Festival Candle 🕯️ (Wax Flame)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_candle(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)elapsed;
    float sin_t = sin(theta);
    int cx_pos = cx + (int)(sin_t * 6.0f);
    int cy_pos = cy + 22;

    // Brass base plate
    canvas.fillEllipse(cx_pos, cy_pos + 15, 20, 6, COL_BRASS);
    // Candle wax column
    canvas.fillRect(cx_pos - 8, cy_pos - 28, 16, 42, COL_WARM_WHITE);
    // Wax drips
    canvas.fillRect(cx_pos - 6, cy_pos - 18, 3, 10, COL_PURE_WHITE);

    // Wick
    canvas.drawLine(cx_pos, cy_pos - 28, cx_pos, cy_pos - 34, COL_DARK_GRAY);

    // Candle Flame with gentle flicker
    int flicker = (int)(sin(now * 0.02f) * 2.0f);
    int fx = cx_pos + flicker;
    int fy = cy_pos - 38;

    if (state == CHAR_STATE_IDLE) {
        canvas.fillCircle(fx, fy, 4, COL_AMBER);
        canvas.drawPixel(fx, fy - 2, COL_PURE_WHITE);
    } else if (state == CHAR_STATE_PLAYING) {
        // Radiant glowing golden halo
        canvas.drawCircle(fx, fy, 12, COL_AMBER);
        canvas.fillCircle(fx, fy, 7, COL_GOLD);
        canvas.fillCircle(fx, fy - 2, 4, COL_PURE_WHITE);
        // Floating peaceful motes
        for (int i = 0; i < 4; ++i) {
            int mx = cx_pos + (int)(sin(now * 0.003f + i) * 18.0f);
            int my = fy - 8 - ((now / 40 + i * 15) % 35);
            canvas.drawPixel(mx, my, COL_GOLD);
        }
    }
}

/* -------------------------------------------------------------------------
 * 8. Character: Diya 🪔 (Sacred Clay Lamp)
 * ------------------------------------------------------------------------- */
void AnimCharacterEngine::render_diya(
    M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now
) {
    (void)elapsed;
    float sin_t = sin(theta);
    int dx = cx + (int)(sin_t * 6.0f);
    int dy = cy + 25;

    // Traditional earthenware lamp body (mitti ka diya)
    canvas.fillEllipse(dx, dy, 26, 12, COL_TERRACOTTA);
    // Inner oil reservoir
    canvas.fillEllipse(dx, dy - 2, 20, 7, COL_AMBER);
    // Golden carved rim
    canvas.drawEllipse(dx, dy - 2, 22, 9, COL_GOLD);

    // Lip & cotton wick on left
    int wx = dx - 16, wy = dy - 6;
    canvas.drawLine(wx + 4, wy + 2, wx, wy - 3, COL_DARK_GRAY);

    // Sacred Flame
    int flicker = (int)(sin(now * 0.025f) * 2.0f);
    int fx = wx + flicker, fy = wy - 8;

    if (state == CHAR_STATE_IDLE) {
        canvas.fillCircle(fx, fy, 4, COL_AMBER);
        canvas.drawPixel(fx, fy - 2, COL_PURE_WHITE);
    } else if (state == CHAR_STATE_PLAYING) {
        // Auspicious glowing aura
        canvas.drawCircle(fx, fy, 14, COL_GOLD);
        canvas.fillCircle(fx, fy, 8, COL_AMBER);
        canvas.fillCircle(fx, fy - 2, 5, COL_WARM_WHITE);
        canvas.drawPixel(fx, fy - 4, COL_PURE_WHITE);

        // Golden sparkle motes rising
        for (int i = 0; i < 5; ++i) {
            int mx = dx + (int)(sin(now * 0.004f + i * 2) * 22.0f);
            int my = dy - 12 - ((now / 35 + i * 12) % 40);
            canvas.drawPixel(mx, my, COL_GOLD);
        }
    }
}

#endif // ARDUINO

} // namespace controller
} // namespace phuljhari
