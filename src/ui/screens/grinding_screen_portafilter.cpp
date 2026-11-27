#include "grinding_screen_portafilter.h"

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../config/constants.h"

namespace {

// Stage layout constants (280x142 animation stage, dead-center at CX = 140)
constexpr int16_t kScreenW = 280;
constexpr int16_t kStageY = 82;
constexpr int16_t kStageH = 142;
constexpr int16_t kCenterX = 140;

// Chute mouth coordinates inside stage
constexpr int16_t kChuteTopY = 2;
constexpr int16_t kChuteMouthY = 20;

// Sleek 60x30 58mm portafilter basket coordinates inside stage
constexpr int16_t kRimY = 92;
constexpr int16_t kBasketBottomY = 122;
constexpr int16_t kBasketTopHalfW = 30;  // 110 .. 170 (60px)
constexpr int16_t kBasketBotHalfW = 24;  // 116 .. 164 (48px)

// Clipped interior box for coffee mound (strictly inside basket walls, 2px inset)
constexpr int16_t kMoundClipX = kCenterX - kBasketTopHalfW + 2; // 112
constexpr int16_t kMoundClipY = kRimY - 22;                     // 70 (allows 20px centered barista dome)
constexpr int16_t kMoundClipW = (kBasketTopHalfW - 2) * 2;      // 56
constexpr int16_t kMoundClipH = (kBasketBottomY - kMoundClipY); // 52

// Coffee mound: the flat fill reaches the rim at kMoundFillFullFrac of the
// dose while the dome above it grows from kMoundDomeStartFrac to the full dose.
constexpr float kMoundFillFullFrac = 0.78f;
constexpr float kMoundDomeStartFrac = 0.25f;
constexpr float kMoundDomeMaxH = 32.0f;
constexpr int16_t kMoundDomeMinH = 2;
constexpr int16_t kMoundClipTopInset = 2;
// The drawn level eases toward the measured level every animation frame, so
// 10 Hz scale readings and scale noise do not show as steps or flicker.
constexpr float kMoundEaseRate = 0.22f;
constexpr float kMoundSnapFrac = 0.002f;

// Finish pulse: one ring that expands from the basket and fades when a grind
// completes. Drawn on the screen (not the stage) so it is not clipped.
constexpr int16_t kFinishRingCenterY = kStageY + (kRimY + kBasketBottomY) / 2;
constexpr int16_t kFinishRingStartD = 40;
constexpr int16_t kFinishRingEndD = 76;
constexpr int16_t kFinishRingWidth = 3;
constexpr int8_t kFinishRingFrames = 15;  // 600 ms at the 40 ms animation period

constexpr uint32_t kColorInTolerance = 0x00E676;
constexpr uint32_t kColorOvershoot = 0xFFB300;

constexpr uint32_t kCoffeeShades[4] = {0x5C3317, 0x7A4622, 0x9C6134, 0xBA7B47};

}  // namespace

GrindingScreenPortafilter::~GrindingScreenPortafilter() {
    if (anim_timer_ && lv_is_initialized()) {
        lv_timer_delete(anim_timer_);
    }
    anim_timer_ = nullptr;
}

void GrindingScreenPortafilter::create() {
    screen_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen_, LV_PCT(100), LV_PCT(78));
    lv_obj_align(screen_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_style_pad_all(screen_, 0, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_CLICKABLE);

    // --- HEADER: Positioned below StatusIndicatorController top bar (Y=10..34) ---
    profile_label_ = lv_label_create(screen_);
    lv_label_set_text(profile_label_, "DOUBLE");
    lv_obj_set_style_text_font(profile_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(profile_label_, lv_color_hex(THEME_COLOR_SECONDARY), 0);
    lv_obj_align(profile_label_, LV_ALIGN_TOP_MID, 0, 38);

    target_label_ = lv_label_create(screen_);
    lv_label_set_text(target_label_, "Target: 18.0g");
    lv_obj_set_style_text_font(target_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(target_label_, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);
    lv_label_set_long_mode(target_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(target_label_, 240);
    lv_obj_set_style_text_align(target_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(target_label_, LV_ALIGN_TOP_MID, 0, 64);
    std::snprintf(displayed_target_text_, sizeof(displayed_target_text_), "Target: 18.0g");

    // --- ANIMATION STAGE (280 x 142) ---
    stage_ = lv_obj_create(screen_);
    lv_obj_set_size(stage_, kScreenW, kStageH);
    lv_obj_set_pos(stage_, 0, kStageY);
    lv_obj_set_style_bg_opa(stage_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stage_, 0, 0);
    lv_obj_set_style_pad_all(stage_, 0, 0);
    lv_obj_clear_flag(stage_, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Eureka Metallic Chute Nozzle (centered at kCenterX = 140)
    lv_obj_t* chute_body = lv_obj_create(stage_);
    lv_obj_set_size(chute_body, 28, 20);
    lv_obj_set_pos(chute_body, kCenterX - 14, kChuteTopY);
    lv_obj_set_style_bg_color(chute_body, lv_color_hex(0x4E545E), 0);
    lv_obj_set_style_bg_grad_color(chute_body, lv_color_hex(0x282C32), 0);
    lv_obj_set_style_bg_grad_dir(chute_body, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_color(chute_body, lv_color_hex(0x8E97A3), 0);
    lv_obj_set_style_border_width(chute_body, 1, 0);
    lv_obj_set_style_radius(chute_body, 4, 0);
    lv_obj_set_style_pad_all(chute_body, 0, 0);
    lv_obj_clear_flag(chute_body, LV_OBJ_FLAG_SCROLLABLE);

    chute_left_pts_[0] = {kCenterX - 18, kChuteTopY};
    chute_left_pts_[1] = {kCenterX - 12, kChuteMouthY};
    lv_obj_t* chute_l = lv_line_create(stage_);
    lv_line_set_points(chute_l, chute_left_pts_, 2);
    lv_obj_set_style_line_width(chute_l, 2, 0);
    lv_obj_set_style_line_color(chute_l, lv_color_hex(0x9AA3AE), 0);

    chute_right_pts_[0] = {kCenterX + 18, kChuteTopY};
    chute_right_pts_[1] = {kCenterX + 12, kChuteMouthY};
    lv_obj_t* chute_r = lv_line_create(stage_);
    lv_line_set_points(chute_r, chute_right_pts_, 2);
    lv_obj_set_style_line_width(chute_r, 2, 0);
    lv_obj_set_style_line_color(chute_r, lv_color_hex(0x9AA3AE), 0);

    // Chute mouth dark slot
    lv_obj_t* chute_mouth = lv_obj_create(stage_);
    lv_obj_set_size(chute_mouth, 18, 4);
    lv_obj_set_pos(chute_mouth, kCenterX - 9, kChuteMouthY - 2);
    lv_obj_set_style_bg_color(chute_mouth, lv_color_hex(0x140C07), 0);
    lv_obj_set_style_border_width(chute_mouth, 0, 0);
    lv_obj_set_style_radius(chute_mouth, 2, 0);
    lv_obj_clear_flag(chute_mouth, LV_OBJ_FLAG_SCROLLABLE);

    // 2. Falling Coffee Grounds Particles (pure tiny LVGL rectangles)
    for (int i = 0; i < kNumParticles; ++i) {
        Particle& p = particles_[i];
        p.size = static_cast<uint8_t>(2 + (i % 2));
        p.obj = lv_obj_create(stage_);
        lv_obj_set_size(p.obj, p.size, p.size + 1);
        lv_obj_set_style_bg_color(p.obj, lv_color_hex(kCoffeeShades[i % 4]), 0);
        lv_obj_set_style_bg_opa(p.obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(p.obj, 0, 0);
        lv_obj_set_style_radius(p.obj, 1, 0);
        lv_obj_set_style_pad_all(p.obj, 0, 0);
        lv_obj_clear_flag(p.obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(p.obj, LV_OBJ_FLAG_HIDDEN);
        p.active = false;
    }

    // 3. Strictly-Clipped Espresso Mound Container inside the 60x30 basket
    mound_clip_ = lv_obj_create(stage_);
    lv_obj_set_pos(mound_clip_, kMoundClipX, kMoundClipY);
    lv_obj_set_size(mound_clip_, kMoundClipW, kMoundClipH);
    lv_obj_set_style_bg_opa(mound_clip_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mound_clip_, 0, 0);
    lv_obj_set_style_pad_all(mound_clip_, 0, 0);
    lv_obj_set_style_radius(mound_clip_, 8, 0);
    lv_obj_set_style_clip_corner(mound_clip_, true, 0);
    lv_obj_clear_flag(mound_clip_, LV_OBJ_FLAG_SCROLLABLE);

    // Rising barista dome (centered rounded top crowning above basket rim)
    mound_dome_ = lv_obj_create(mound_clip_);
    lv_obj_set_size(mound_dome_, kMoundClipW - 4, 20);
    lv_obj_set_pos(mound_dome_, 2, kMoundClipH);
    lv_obj_set_style_bg_color(mound_dome_, lv_color_hex(0xBC7E4A), 0);
    lv_obj_set_style_bg_grad_color(mound_dome_, lv_color_hex(0x7B4620), 0);
    lv_obj_set_style_bg_grad_dir(mound_dome_, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(mound_dome_, 0, 0);
    lv_obj_set_style_radius(mound_dome_, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(mound_dome_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mound_dome_, LV_OBJ_FLAG_HIDDEN);

    // Base espresso fill rising smoothly inside basket walls
    mound_fill_ = lv_obj_create(mound_clip_);
    lv_obj_set_size(mound_fill_, kMoundClipW - 6, 0);
    lv_obj_set_pos(mound_fill_, 3, kMoundClipH);
    lv_obj_set_style_bg_color(mound_fill_, lv_color_hex(0x8A5229), 0);
    lv_obj_set_style_bg_grad_color(mound_fill_, lv_color_hex(0x4A2610), 0);
    lv_obj_set_style_bg_grad_dir(mound_fill_, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(mound_fill_, 0, 0);
    lv_obj_set_style_radius(mound_fill_, 6, 0);
    lv_obj_clear_flag(mound_fill_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mound_fill_, LV_OBJ_FLAG_HIDDEN);

    // 4. Authentic Italian Espresso Portafilter Handle (angled subtly ~3.2 deg)
    lv_obj_t* handle_group = lv_obj_create(stage_);
    lv_obj_set_size(handle_group, 94, 24);
    lv_obj_set_pos(handle_group, kCenterX + kBasketTopHalfW - 2, kRimY - 1);
    lv_obj_set_style_bg_opa(handle_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(handle_group, 0, 0);
    lv_obj_set_style_pad_all(handle_group, 0, 0);
    lv_obj_set_style_transform_pivot_x(handle_group, 0, 0);
    lv_obj_set_style_transform_pivot_y(handle_group, 12, 0);
    lv_obj_set_style_transform_rotation(handle_group, 32, 0); // 3.2 degrees downward
    lv_obj_clear_flag(handle_group, LV_OBJ_FLAG_SCROLLABLE);

    // Chrome neck stem
    lv_obj_t* neck = lv_obj_create(handle_group);
    lv_obj_set_size(neck, 14, 9);
    lv_obj_set_pos(neck, 0, 7);
    lv_obj_set_style_bg_color(neck, lv_color_hex(0xC8D0DA), 0);
    lv_obj_set_style_bg_grad_color(neck, lv_color_hex(0x525862), 0);
    lv_obj_set_style_bg_grad_dir(neck, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(neck, 0, 0);
    lv_obj_set_style_radius(neck, 2, 0);
    lv_obj_clear_flag(neck, LV_OBJ_FLAG_SCROLLABLE);

    // Chrome ferrule collar
    lv_obj_t* ferrule = lv_obj_create(handle_group);
    lv_obj_set_size(ferrule, 6, 13);
    lv_obj_set_pos(ferrule, 13, 5);
    lv_obj_set_style_bg_color(ferrule, lv_color_hex(0xE0E6EE), 0);
    lv_obj_set_style_bg_grad_color(ferrule, lv_color_hex(0x6D7582), 0);
    lv_obj_set_style_bg_grad_dir(ferrule, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(ferrule, 0, 0);
    lv_obj_set_style_radius(ferrule, 2, 0);
    lv_obj_clear_flag(ferrule, LV_OBJ_FLAG_SCROLLABLE);

    // Walnut wood ergonomic handle body
    lv_obj_t* grip = lv_obj_create(handle_group);
    lv_obj_set_size(grip, 66, 16);
    lv_obj_set_pos(grip, 18, 4);
    lv_obj_set_style_bg_color(grip, lv_color_hex(0x764523), 0);
    lv_obj_set_style_bg_grad_color(grip, lv_color_hex(0x2B1408), 0);
    lv_obj_set_style_bg_grad_dir(grip, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(grip, lv_color_hex(0x8F5B36), 0);
    lv_obj_set_style_border_width(grip, 1, 0);
    lv_obj_set_style_radius(grip, 8, 0);
    lv_obj_clear_flag(grip, LV_OBJ_FLAG_SCROLLABLE);

    // Chrome pommel end-cap
    lv_obj_t* pommel = lv_obj_create(handle_group);
    lv_obj_set_size(pommel, 8, 14);
    lv_obj_set_pos(pommel, 79, 5);
    lv_obj_set_style_bg_color(pommel, lv_color_hex(0xE8EEF5), 0);
    lv_obj_set_style_bg_grad_color(pommel, lv_color_hex(0x5A616C), 0);
    lv_obj_set_style_bg_grad_dir(pommel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(pommel, 0, 0);
    lv_obj_set_style_radius(pommel, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(pommel, LV_OBJ_FLAG_SCROLLABLE);

    // 5. Classic Dual Espresso Spouts underneath the basket
    lv_obj_t* spout_hub = lv_obj_create(stage_);
    lv_obj_set_size(spout_hub, 14, 7);
    lv_obj_set_pos(spout_hub, kCenterX - 7, kBasketBottomY + 1);
    lv_obj_set_style_bg_color(spout_hub, lv_color_hex(0x9BA4B0), 0);
    lv_obj_set_style_bg_grad_color(spout_hub, lv_color_hex(0x4A5058), 0);
    lv_obj_set_style_bg_grad_dir(spout_hub, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(spout_hub, 0, 0);
    lv_obj_set_style_radius(spout_hub, 2, 0);
    lv_obj_clear_flag(spout_hub, LV_OBJ_FLAG_SCROLLABLE);

    left_spout_pts_[0] = {kCenterX - 4, kBasketBottomY + 5};
    left_spout_pts_[1] = {kCenterX - 12, kBasketBottomY + 7};
    left_spout_pts_[2] = {kCenterX - 14, kBasketBottomY + 14};
    lv_obj_t* left_spout = lv_line_create(stage_);
    lv_line_set_points(left_spout, left_spout_pts_, 3);
    lv_obj_set_style_line_width(left_spout, 4, 0);
    lv_obj_set_style_line_rounded(left_spout, true, 0);
    lv_obj_set_style_line_color(left_spout, lv_color_hex(0x8E97A3), 0);

    right_spout_pts_[0] = {kCenterX + 4, kBasketBottomY + 5};
    right_spout_pts_[1] = {kCenterX + 12, kBasketBottomY + 7};
    right_spout_pts_[2] = {kCenterX + 14, kBasketBottomY + 14};
    lv_obj_t* right_spout = lv_line_create(stage_);
    lv_line_set_points(right_spout, right_spout_pts_, 3);
    lv_obj_set_style_line_width(right_spout, 4, 0);
    lv_obj_set_style_line_rounded(right_spout, true, 0);
    lv_obj_set_style_line_color(right_spout, lv_color_hex(0x8E97A3), 0);

    // 6. Cutaway Stainless Basket Shell Contour (60x30)
    basket_shell_pts_[0] = {kCenterX - kBasketTopHalfW, kRimY};
    basket_shell_pts_[1] = {kCenterX - kBasketBotHalfW, kBasketBottomY - 3};
    basket_shell_pts_[2] = {kCenterX - kBasketBotHalfW + 6, kBasketBottomY + 1};
    basket_shell_pts_[3] = {kCenterX + kBasketBotHalfW - 6, kBasketBottomY + 1};
    basket_shell_pts_[4] = {kCenterX + kBasketBotHalfW, kBasketBottomY - 3};
    basket_shell_pts_[5] = {kCenterX + kBasketTopHalfW, kRimY};

    basket_shell_line_ = lv_line_create(stage_);
    lv_line_set_points(basket_shell_line_, basket_shell_pts_, 6);
    lv_obj_set_style_line_width(basket_shell_line_, 3, 0);
    lv_obj_set_style_line_rounded(basket_shell_line_, true, 0);
    lv_obj_set_style_line_color(basket_shell_line_, lv_color_hex(0xC5CCD6), 0);

    // 7. Bayonet Locking Ears
    lv_obj_t* ear_l = lv_obj_create(stage_);
    lv_obj_set_size(ear_l, 8, 5);
    lv_obj_set_pos(ear_l, kCenterX - kBasketTopHalfW - 8, kRimY - 1);
    lv_obj_set_style_bg_color(ear_l, lv_color_hex(0x9BA4B0), 0);
    lv_obj_set_style_border_width(ear_l, 0, 0);
    lv_obj_set_style_radius(ear_l, 2, 0);
    lv_obj_clear_flag(ear_l, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ear_r = lv_obj_create(stage_);
    lv_obj_set_size(ear_r, 8, 5);
    lv_obj_set_pos(ear_r, kCenterX + kBasketTopHalfW, kRimY - 1);
    lv_obj_set_style_bg_color(ear_r, lv_color_hex(0x9BA4B0), 0);
    lv_obj_set_style_border_width(ear_r, 0, 0);
    lv_obj_set_style_radius(ear_r, 2, 0);
    lv_obj_clear_flag(ear_r, LV_OBJ_FLAG_SCROLLABLE);

    // 8. Brushed Stainless Top Flange Rim
    lv_obj_t* rim = lv_obj_create(stage_);
    lv_obj_set_size(rim, (kBasketTopHalfW + 3) * 2, 6);
    lv_obj_set_pos(rim, kCenterX - kBasketTopHalfW - 3, kRimY - 3);
    lv_obj_set_style_bg_color(rim, lv_color_hex(0xE8EEF5), 0);
    lv_obj_set_style_bg_grad_color(rim, lv_color_hex(0x6E7682), 0);
    lv_obj_set_style_bg_grad_dir(rim, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(rim, 0, 0);
    lv_obj_set_style_radius(rim, 3, 0);
    lv_obj_clear_flag(rim, LV_OBJ_FLAG_SCROLLABLE);

    // 9. Taring Spinner Arc centered inside basket
    tare_spinner_ = lv_arc_create(stage_);
    lv_obj_set_size(tare_spinner_, 22, 22);
    lv_obj_set_pos(tare_spinner_, kCenterX - 11, kRimY + 5);
    lv_arc_set_bg_angles(tare_spinner_, 0, 360);
    lv_arc_set_angles(tare_spinner_, 0, 140);
    lv_obj_set_style_arc_opa(tare_spinner_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(tare_spinner_, lv_color_hex(THEME_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(tare_spinner_, 3, LV_PART_INDICATOR);
    lv_obj_remove_style(tare_spinner_, nullptr, LV_PART_KNOB);
    lv_obj_add_flag(tare_spinner_, LV_OBJ_FLAG_HIDDEN);

    // --- FINISH PULSE RING (created before the weight readout so it draws behind it) ---
    finish_ring_ = lv_obj_create(screen_);
    lv_obj_set_style_bg_opa(finish_ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(finish_ring_, kFinishRingWidth, 0);
    lv_obj_set_style_radius(finish_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(finish_ring_, 0, 0);
    lv_obj_clear_flag(finish_ring_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(finish_ring_, LV_OBJ_FLAG_HIDDEN);

    // --- LIVE WEIGHT READOUT ---
    weight_label_ = lv_label_create(screen_);
    lv_label_set_text(weight_label_, "0.0g");
    lv_obj_set_style_text_font(weight_label_, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(weight_label_, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(weight_label_, LV_ALIGN_TOP_MID, 0, 226);
    std::snprintf(displayed_weight_text_, sizeof(displayed_weight_text_), "0.0g");

    // --- LIVE DUAL-CURVE MINI-CHART (Weight + Flow Rate g/s) ---
    chart_ = lv_chart_create(screen_);
    lv_obj_set_size(chart_, 244, 64);
    lv_obj_align(chart_, LV_ALIGN_TOP_MID, 0, 266);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_, kMiniChartPoints);
    lv_chart_set_update_mode(chart_, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart_, 2, 0);
    lv_obj_set_style_bg_color(chart_, lv_color_hex(0x0D1014), LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_, lv_color_hex(0x262C34), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(chart_, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart_, lv_color_hex(0x1E232B), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart_, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart_, 0, LV_PART_INDICATOR);

    chart_y_max_ = 185;
    lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, chart_y_max_);
    lv_chart_set_axis_range(chart_, LV_CHART_AXIS_SECONDARY_Y, 0, 25);

    flow_series_ = lv_chart_add_series(chart_, lv_color_hex(THEME_COLOR_ACCENT), LV_CHART_AXIS_SECONDARY_Y);
    weight_series_ = lv_chart_add_series(chart_, lv_color_hex(THEME_COLOR_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);

    // Compact live flow legend inside top-left of mini-chart
    flow_legend_label_ = lv_label_create(chart_);
    lv_label_set_text(flow_legend_label_, "0.0 g/s");
    lv_obj_set_style_text_font(flow_legend_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flow_legend_label_, lv_color_hex(THEME_COLOR_ACCENT), 0);
    lv_obj_align(flow_legend_label_, LV_ALIGN_TOP_LEFT, 4, 0);
    std::snprintf(displayed_flow_text_, sizeof(displayed_flow_text_), "0.0 g/s");

    // Live phase/status indicator inside top-right of mini-chart
    status_badge_label_ = lv_label_create(chart_);
    lv_label_set_text(status_badge_label_, "TARING");
    lv_obj_set_style_text_font(status_badge_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_badge_label_, lv_color_hex(0x29B6F6), 0);
    lv_obj_align(status_badge_label_, LV_ALIGN_TOP_RIGHT, -4, 0);
    std::snprintf(displayed_status_text_, sizeof(displayed_status_text_), "TARING");
    last_status_color_ = 0x29B6F6;

    reset_chart_data();

    // Ensure all child widgets pass click events to screen_ so tap toggles layouts
    clear_clickable_recursive(screen_);
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_CLICKABLE);

    // Create 25 FPS animation timer (starts paused until show())
    anim_timer_ = lv_timer_create(anim_timer_cb, kAnimPeriodMs, this);
    lv_timer_pause(anim_timer_);

    visible_ = false;
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
}

void GrindingScreenPortafilter::clear_clickable_recursive(lv_obj_t* obj) {
    if (!obj) return;
    const uint32_t count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        lv_obj_clear_flag(child, LV_OBJ_FLAG_CLICKABLE);
        clear_clickable_recursive(child);
    }
}

void GrindingScreenPortafilter::show() {
    if (!screen_) return;
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_HIDDEN);
    visible_ = true;
    snap_mound_to_target();
    if (anim_timer_) {
        lv_timer_resume(anim_timer_);
    }
}

void GrindingScreenPortafilter::hide() {
    if (!screen_) return;
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
    visible_ = false;
    if (anim_timer_) {
        lv_timer_pause(anim_timer_);
    }
}

void GrindingScreenPortafilter::update_profile_name(const char* name) {
    if (profile_label_ && name) {
        lv_label_set_text(profile_label_, name);
    }
}

void GrindingScreenPortafilter::update_target_weight(float weight) {
    if (std::isfinite(weight) && weight > 0.1f) {
        target_weight_g_ = weight;
        if (chart_ && !manual_mode_) {
            const int32_t new_max = static_cast<int32_t>((target_weight_g_ + 0.5f) * 10.0f);
            if (new_max != chart_y_max_) {
                chart_y_max_ = new_max;
                lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, chart_y_max_);
            }
        }
    }
    if (time_mode_ || manual_mode_ || !target_label_) {
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Target: " SYS_WEIGHT_DISPLAY_FORMAT,
                  std::isfinite(weight) ? weight : 0.0f);
    update_target_weight_text(buf);
}

void GrindingScreenPortafilter::update_target_weight_text(const char* text) {
    if (!target_label_ || !text) return;
    if (std::strcmp(displayed_target_text_, text) == 0) {
        return;
    }
    std::snprintf(displayed_target_text_, sizeof(displayed_target_text_), "%s", text);
    lv_label_set_text(target_label_, displayed_target_text_);
}

void GrindingScreenPortafilter::update_target_time(float seconds) {
    target_time_s_ = std::isfinite(seconds) ? seconds : 0.0f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Time: %.1fs", target_time_s_);
    update_target_weight_text(buf);
}

void GrindingScreenPortafilter::update_current_weight(float weight) {
    const float safe_weight = std::isfinite(weight) ? weight : 0.0f;
    if (taring_) {
        taring_ = false;
        if (tare_spinner_) {
            lv_obj_add_flag(tare_spinner_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    current_weight_g_ = (safe_weight < 0.0f) ? 0.0f : safe_weight;

    if (!time_mode_ && weight_label_) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), SYS_WEIGHT_DISPLAY_FORMAT, safe_weight);
        if (std::strcmp(displayed_weight_text_, buf) != 0) {
            std::snprintf(displayed_weight_text_, sizeof(displayed_weight_text_), "%s", buf);
            lv_label_set_text(weight_label_, buf);
        }
    }
}

void GrindingScreenPortafilter::update_tare_display() {
    taring_ = true;
    paused_ = false;
    completed_ = false;
    overshoot_ = false;
    error_ = false;
    current_weight_g_ = 0.0f;
    current_flow_gps_ = 0.0f;
    displayed_progress_ = 0;

    if (weight_label_) {
        if (std::strcmp(displayed_weight_text_, "TARE") != 0) {
            std::snprintf(displayed_weight_text_, sizeof(displayed_weight_text_), "TARE");
            lv_label_set_text(weight_label_, "TARE");
        }
    }
    if (tare_spinner_) {
        lv_obj_clear_flag(tare_spinner_, LV_OBJ_FLAG_HIDDEN);
    }
    hide_all_particles();
    refresh_visual_accent();
    set_status_badge("TARING", 0x29B6F6);
    snap_mound_to_target();
}

void GrindingScreenPortafilter::update_progress(int percent) {
    const int clamped = std::clamp(percent, 0, 100);
    displayed_progress_ = clamped;

    if (time_mode_ && target_time_s_ > 0.0f && weight_label_) {
        const float elapsed_s = (clamped / 100.0f) * target_time_s_;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1fs", elapsed_s);
        if (std::strcmp(displayed_weight_text_, buf) != 0) {
            std::snprintf(displayed_weight_text_, sizeof(displayed_weight_text_), "%s", buf);
            lv_label_set_text(weight_label_, buf);
        }
    }
}

void GrindingScreenPortafilter::set_status_badge(const char* text, uint32_t color_hex) {
    if (!status_badge_label_ || !text) return;
    if (std::strcmp(displayed_status_text_, text) != 0) {
        std::snprintf(displayed_status_text_, sizeof(displayed_status_text_), "%s", text);
        lv_label_set_text(status_badge_label_, displayed_status_text_);
    }
    if (last_status_color_ != color_hex) {
        last_status_color_ = color_hex;
        lv_obj_set_style_text_color(status_badge_label_, lv_color_hex(color_hex), 0);
    }
}

void GrindingScreenPortafilter::hide_all_particles() {
    for (auto& p : particles_) {
        p.active = false;
        if (p.obj) {
            lv_obj_add_flag(p.obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void GrindingScreenPortafilter::refresh_visual_accent() {
    uint32_t weight_color = THEME_COLOR_TEXT_PRIMARY;
    uint32_t basket_color = 0xC5CCD6;
    uint32_t series_color = THEME_COLOR_PRIMARY;

    if (error_) {
        weight_color = 0xFF5252;
        basket_color = 0xFF5252;
        series_color = 0xFF5252;
    } else if (completed_) {
        if (overshoot_) {
            weight_color = kColorOvershoot;
            basket_color = 0xFFA000;
            series_color = 0xFFA000;
        } else {
            weight_color = kColorInTolerance;
            basket_color = 0x00C853;
            series_color = 0x00C853;
        }
    }

    if (weight_label_) {
        lv_obj_set_style_text_color(weight_label_, lv_color_hex(weight_color), 0);
    }
    if (target_label_) {
        const uint32_t target_color =
            (completed_ && deviation_shown_) ? weight_color : THEME_COLOR_TEXT_SECONDARY;
        lv_obj_set_style_text_color(target_label_, lv_color_hex(target_color), 0);
    }
    if (basket_shell_line_) {
        lv_obj_set_style_line_color(basket_shell_line_, lv_color_hex(basket_color), 0);
    }
    if (chart_ && weight_series_) {
        lv_chart_set_series_color(chart_, weight_series_, lv_color_hex(series_color));
    }
}

void GrindingScreenPortafilter::update_phase(GrindPhase phase, bool paused) {
    phase_tracking_active_ = true;
    const bool entering_pulse =
        (phase == GrindPhase::PULSE_EXECUTE && current_phase_ != GrindPhase::PULSE_EXECUTE) ||
        (phase == GrindPhase::TIME_ADDITIONAL_PULSE && current_phase_ != GrindPhase::TIME_ADDITIONAL_PULSE);
    if (entering_pulse && pulse_display_count_ < 99) {
        ++pulse_display_count_;
    }
    if (phase == GrindPhase::TIME_ADDITIONAL_PULSE) {
        completed_ = false;
        overshoot_ = false;
        error_ = false;
        if (deviation_shown_) {
            // The dose is changing; its deviation returns once re-measured.
            deviation_shown_ = false;
            update_target_weight(target_weight_g_);
        }
        refresh_visual_accent();
    }

    current_phase_ = phase;
    paused_ = paused;
    if (paused_) {
        hide_all_particles();
    }

    switch (phase) {
        case GrindPhase::INITIALIZING:
        case GrindPhase::SETUP:
        case GrindPhase::TARING:
        case GrindPhase::TARE_CONFIRM:
            set_status_badge("TARING", 0x29B6F6);
            break;
        case GrindPhase::PRIME:
        case GrindPhase::PRIME_SETTLING:
            set_status_badge("PRIMING", 0xFFB300);
            break;
        case GrindPhase::PURGE_CONFIRM:
            set_status_badge("PURGE", 0xFFB300);
            break;
        case GrindPhase::PREDICTIVE:
        case GrindPhase::MANUAL_GRINDING:
            set_status_badge("GRINDING", 0xFFA726);
            break;
        case GrindPhase::TIME_GRINDING:
            if (paused_) {
                set_status_badge("PAUSED", 0xFFB300);
            } else {
                set_status_badge("GRINDING", 0xFFA726);
            }
            break;
        case GrindPhase::PULSE_EXECUTE:
        case GrindPhase::TIME_ADDITIONAL_PULSE: {
            char pulse_buf[20];
            std::snprintf(pulse_buf, sizeof(pulse_buf), "PULSE #%u",
                          static_cast<unsigned>(std::max<uint8_t>(1, pulse_display_count_)));
            set_status_badge(pulse_buf, 0x29B6F6);
            break;
        }
        case GrindPhase::PULSE_SETTLING:
        case GrindPhase::PULSE_DECISION:
        case GrindPhase::FINAL_SETTLING:
            set_status_badge("SETTLING...", 0x4FC3F7);
            break;
        case GrindPhase::COMPLETED:
            set_status_badge(overshoot_ ? "OVERSHOOT" : "READY",
                             overshoot_ ? 0xFFB300 : 0x00E676);
            break;
        case GrindPhase::TIMEOUT:
            set_status_badge("STOPPED", 0xFF5252);
            break;
        case GrindPhase::IDLE:
        default:
            break;
    }
}

void GrindingScreenPortafilter::set_completed(bool completed, bool overshoot, float final_weight_g) {
    const bool just_completed = completed && !completed_;
    completed_ = completed;
    overshoot_ = completed ? overshoot : false;

    // A finished weight grind adds its deviation from the target to the target line.
    const bool show_deviation =
        completed_ && !time_mode_ && !manual_mode_ && std::isfinite(final_weight_g);
    if (show_deviation) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Target: " SYS_WEIGHT_DISPLAY_FORMAT "  %+.2fg",
                      target_weight_g_, final_weight_g - target_weight_g_);
        update_target_weight_text(buf);
    } else if (deviation_shown_) {
        update_target_weight(target_weight_g_);
    }
    deviation_shown_ = show_deviation;
    if (just_completed) {
        start_finish_pulse();
    } else if (!completed_) {
        stop_finish_pulse();
    }

    if (completed_) {
        error_ = false;
        paused_ = false;
        taring_ = false;
        current_flow_gps_ = 0.0f;
        if (tare_spinner_) {
            lv_obj_add_flag(tare_spinner_, LV_OBJ_FLAG_HIDDEN);
        }
        hide_all_particles();
        set_status_badge(overshoot_ ? "OVERSHOOT" : "READY",
                         overshoot_ ? 0xFFB300 : 0x00E676);
    }
    refresh_visual_accent();
}

void GrindingScreenPortafilter::set_error_state(bool error) {
    error_ = error;
    if (error_) {
        completed_ = false;
        overshoot_ = false;
        paused_ = false;
        taring_ = false;
        current_flow_gps_ = 0.0f;
        if (tare_spinner_) {
            lv_obj_add_flag(tare_spinner_, LV_OBJ_FLAG_HIDDEN);
        }
        hide_all_particles();
        set_status_badge("STOPPED", 0xFF5252);
    }
    refresh_visual_accent();
}

void GrindingScreenPortafilter::add_chart_data_point(float current_weight,
                                                     float flow_rate,
                                                     uint32_t current_time_ms) {
    const float safe_weight = std::isfinite(current_weight) ? std::max(0.0f, current_weight) : 0.0f;
    const float safe_flow = std::isfinite(flow_rate) ? std::clamp(flow_rate, 0.0f, 3.5f) : 0.0f;
    current_flow_gps_ = safe_flow;
    last_flow_sample_ms_ = current_time_ms;

    if (flow_legend_label_) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f g/s", safe_flow);
        if (std::strcmp(displayed_flow_text_, buf) != 0) {
            std::snprintf(displayed_flow_text_, sizeof(displayed_flow_text_), "%s", buf);
            lv_label_set_text(flow_legend_label_, buf);
        }
    }

    if (!chart_ || !weight_series_ || !flow_series_) {
        return;
    }

    if (manual_mode_) {
        const int32_t needed_max = static_cast<int32_t>(std::max(20.0f, safe_weight + 2.0f) * 10.0f);
        if (needed_max > chart_y_max_) {
            chart_y_max_ = needed_max;
            lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, chart_y_max_);
        }
    }

    if (last_chart_sample_ms_ != 0 &&
        (current_time_ms - last_chart_sample_ms_) < kChartSampleIntervalMs) {
        return;
    }
    last_chart_sample_ms_ = current_time_ms;

    const int32_t w_val = static_cast<int32_t>(safe_weight * 10.0f);
    const int32_t f_val = static_cast<int32_t>(std::min(2.5f, safe_flow) * 10.0f);
    lv_chart_set_next_value(chart_, weight_series_, w_val);
    lv_chart_set_next_value(chart_, flow_series_, f_val);
}

void GrindingScreenPortafilter::reset_chart_data() {
    last_chart_sample_ms_ = 0;
    last_flow_sample_ms_ = 0;
    current_flow_gps_ = 0.0f;
    current_weight_g_ = 0.0f;
    displayed_progress_ = 0;
    pulse_display_count_ = 0;
    paused_ = false;
    error_ = false;
    overshoot_ = false;
    phase_tracking_active_ = false;
    current_phase_ = GrindPhase::IDLE;
    set_completed(false, false);
    set_status_badge(manual_mode_ ? "GRINDING" : "TARING",
                     manual_mode_ ? 0xFFA726 : 0x29B6F6);

    if (chart_) {
        chart_y_max_ = manual_mode_
                           ? 200
                           : static_cast<int32_t>((std::max(4.0f, target_weight_g_) + 0.5f) * 10.0f);
        lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, chart_y_max_);
        if (weight_series_ && flow_series_) {
            lv_chart_set_all_values(chart_, weight_series_, 0);
            lv_chart_set_all_values(chart_, flow_series_, 0);
        }
    }
    if (flow_legend_label_) {
        std::snprintf(displayed_flow_text_, sizeof(displayed_flow_text_), "0.0 g/s");
        lv_label_set_text(flow_legend_label_, displayed_flow_text_);
    }
    hide_all_particles();
    snap_mound_to_target();
}

void GrindingScreenPortafilter::set_time_mode(bool enabled) {
    time_mode_ = enabled;
}

void GrindingScreenPortafilter::set_manual_mode(bool enabled) {
    manual_mode_ = enabled;
    if (manual_mode_ && chart_) {
        chart_y_max_ = 200;
        lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, chart_y_max_);
    }
}

float GrindingScreenPortafilter::mound_target_fraction() const {
    if (taring_) {
        return 0.0f;
    }
    if (manual_mode_) {
        const float manual_ref = std::max(20.0f, current_weight_g_ + 2.0f);
        return std::clamp(current_weight_g_ / manual_ref, 0.0f, 1.0f);
    }
    if (!time_mode_ && target_weight_g_ > 0.1f) {
        return std::clamp(current_weight_g_ / target_weight_g_, 0.0f, 1.0f);
    }
    return std::clamp(displayed_progress_ / 100.0f, 0.0f, 1.0f);
}

GrindingScreenPortafilter::MoundShape GrindingScreenPortafilter::mound_shape(float fraction) {
    MoundShape shape;
    if (fraction <= 0.01f) {
        return shape;
    }
    const int16_t basket_h = kBasketBottomY - kRimY;
    const float fill_frac = std::min(1.0f, fraction / kMoundFillFullFrac);
    shape.fill_h = std::max<int16_t>(3, static_cast<int16_t>(std::round(fill_frac * basket_h)));

    const float dome_frac =
        std::clamp((fraction - kMoundDomeStartFrac) / (1.0f - kMoundDomeStartFrac), 0.0f, 1.0f);
    const int16_t dome_h = static_cast<int16_t>(std::round(dome_frac * kMoundDomeMaxH));
    shape.dome_h = (dome_h >= kMoundDomeMinH) ? dome_h : 0;
    return shape;
}

float GrindingScreenPortafilter::mound_surface_y() const {
    const int16_t surface = kBasketBottomY - mound_shape_.fill_h - (mound_shape_.dome_h / 2);
    return static_cast<float>(std::max<int16_t>(surface, kMoundClipY + kMoundClipTopInset));
}

void GrindingScreenPortafilter::snap_mound_to_target() {
    mound_fraction_ = mound_target_fraction();
    apply_mound_shape(mound_shape(mound_fraction_));
}

void GrindingScreenPortafilter::advance_mound() {
    const float target = mound_target_fraction();
    const float remaining = target - mound_fraction_;
    if (taring_ || std::fabs(remaining) < kMoundSnapFrac) {
        mound_fraction_ = target;
    } else {
        mound_fraction_ += remaining * kMoundEaseRate;
    }
    apply_mound_shape(mound_shape(mound_fraction_));
}

void GrindingScreenPortafilter::apply_mound_shape(const MoundShape& shape) {
    if (!mound_fill_ || !mound_dome_) return;
    if (mound_shape_applied_ && shape.fill_h == mound_shape_.fill_h &&
        shape.dome_h == mound_shape_.dome_h) {
        return; // Avoid redundant LVGL invalidations
    }
    mound_shape_ = shape;
    mound_shape_applied_ = true;

    if (shape.fill_h == 0) {
        lv_obj_add_flag(mound_fill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mound_dome_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const int16_t fill_top = kMoundClipH - shape.fill_h;
    lv_obj_clear_flag(mound_fill_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(mound_fill_, kMoundClipW - 6, shape.fill_h);
    lv_obj_set_pos(mound_fill_, 3, fill_top);

    if (shape.dome_h > 0) {
        const int16_t dome_top =
            std::max<int16_t>(kMoundClipTopInset, fill_top - (shape.dome_h / 2));
        lv_obj_clear_flag(mound_dome_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(mound_dome_, kMoundClipW - 4, shape.dome_h);
        lv_obj_set_pos(mound_dome_, 2, dome_top);
    } else {
        lv_obj_add_flag(mound_dome_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrindingScreenPortafilter::start_finish_pulse() {
    if (!finish_ring_) return;
    lv_obj_set_style_border_color(
        finish_ring_, lv_color_hex(overshoot_ ? kColorOvershoot : kColorInTolerance), 0);
    finish_frame_ = 0;
}

void GrindingScreenPortafilter::stop_finish_pulse() {
    finish_frame_ = -1;
    if (finish_ring_) {
        lv_obj_add_flag(finish_ring_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrindingScreenPortafilter::advance_finish_pulse() {
    if (finish_frame_ < 0 || !finish_ring_) return;
    if (finish_frame_ >= kFinishRingFrames) {
        stop_finish_pulse();
        return;
    }
    // Ease-out growth with a linear fade, so the ring slows as it disappears.
    const float progress = static_cast<float>(finish_frame_) / kFinishRingFrames;
    const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
    const int16_t diameter = static_cast<int16_t>(
        kFinishRingStartD + eased * (kFinishRingEndD - kFinishRingStartD));
    lv_obj_set_size(finish_ring_, diameter, diameter);
    lv_obj_set_pos(finish_ring_, kCenterX - diameter / 2, kFinishRingCenterY - diameter / 2);
    lv_obj_set_style_border_opa(finish_ring_,
                                static_cast<lv_opa_t>(LV_OPA_COVER * (1.0f - progress)), 0);
    lv_obj_clear_flag(finish_ring_, LV_OBJ_FLAG_HIDDEN);
    ++finish_frame_;
}

uint32_t GrindingScreenPortafilter::next_rand() {
    // Fast xorshift32 (zero heap, zero libc rand contention)
    uint32_t x = rng_state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state_ = x;
    return x;
}

void GrindingScreenPortafilter::reset_particle(Particle& p, float flow_norm) {
    // Spawn strictly inside chute opening (kCenterX - 5 .. kCenterX + 5)
    const float jitter_x = (static_cast<float>(next_rand() % 1000) / 1000.0f - 0.5f) * 10.0f;
    const float speed_scale = 0.75f + flow_norm * 0.65f;
    p.x = static_cast<float>(kCenterX) + jitter_x - (p.size * 0.5f);
    p.y = static_cast<float>(kChuteMouthY) + static_cast<float>(next_rand() % 4);
    p.vy = (2.0f + (static_cast<float>(next_rand() % 100) / 70.0f)) * speed_scale;
    p.vx = (static_cast<float>(next_rand() % 100) / 100.0f - 0.5f) * 0.28f;
    p.active = true;
    if (p.obj) {
        lv_obj_set_pos(p.obj, static_cast<int16_t>(p.x), static_cast<int16_t>(p.y));
        lv_obj_clear_flag(p.obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrindingScreenPortafilter::anim_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<GrindingScreenPortafilter*>(lv_timer_get_user_data(timer));
    if (self) {
        self->step_animation();
    }
}

void GrindingScreenPortafilter::step_animation() {
    if (!visible_) return;

    advance_mound();
    advance_finish_pulse();

    // Spin taring arc smoothly while taring
    if (taring_ && tare_spinner_) {
        tare_angle_ = static_cast<int16_t>((tare_angle_ + 18) % 360);
        lv_arc_set_rotation(tare_spinner_, tare_angle_);
    }

    // Decay displayed flow to 0 if no chart flow sample arrived in >350ms (settling/stopped)
    const uint32_t now = millis();
    float active_flow = current_flow_gps_;
    if (completed_ || error_ || paused_ || taring_ ||
        (last_flow_sample_ms_ != 0 && (now - last_flow_sample_ms_) > 350)) {
        active_flow = 0.0f;
    }

    // In TIME mode, drive steady coffee animation strictly while an unpaused motor phase is active
    if (time_mode_ && !completed_ && !error_ && !paused_ && !taring_) {
        if (phase_tracking_active_) {
            if (current_phase_ == GrindPhase::TIME_GRINDING) {
                active_flow = std::max(active_flow, 1.4f);
            } else if (current_phase_ == GrindPhase::TIME_ADDITIONAL_PULSE) {
                active_flow = std::max(active_flow, 1.2f);
            }
        } else if (displayed_progress_ > 0 && displayed_progress_ < 100) {
            active_flow = std::max(active_flow, 1.4f);
        }
    }

    const bool emitting = active_flow > 0.12f;
    const float flow_norm = std::clamp(active_flow / 1.6f, 0.0f, 1.5f);
    const uint32_t emit_threshold = static_cast<uint32_t>(std::min(950.0f, 250.0f + flow_norm * 650.0f));
    const float surface_y = mound_surface_y();

    for (auto& p : particles_) {
        if (!p.obj) continue;
        if (!p.active) {
            if (emitting && (next_rand() % 1000) < emit_threshold) {
                reset_particle(p, flow_norm);
            }
            continue;
        }
        p.vy += 0.25f;
        p.y += p.vy;
        p.x += p.vx;
        if (p.y >= surface_y) {
            if (emitting) {
                reset_particle(p, flow_norm);
            } else {
                p.active = false;
                lv_obj_add_flag(p.obj, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            lv_obj_set_pos(p.obj, static_cast<int16_t>(p.x), static_cast<int16_t>(p.y));
        }
    }
}
