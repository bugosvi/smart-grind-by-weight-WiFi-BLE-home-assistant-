#pragma once
#include "grinding_screen_base.h"
#include "../../controllers/grind_events.h"
#include <cstdint>

// Vector-only LVGL 9 grinding screen displaying coffee falling from a grinder
// chute into a 58mm Italian espresso portafilter with a live dual-curve mini-chart
// (weight + flow rate g/s). Uses pure LVGL primitives (~6-8KB Flash, zero images).
class GrindingScreenPortafilter : public IGrindingScreen {
public:
    GrindingScreenPortafilter() = default;
    ~GrindingScreenPortafilter() override;

    void create() override;
    void show() override;
    void hide() override;
    void update_profile_name(const char* name) override;
    void update_target_weight(float weight) override;
    void update_target_weight_text(const char* text) override;
    void update_target_time(float seconds);
    void update_current_weight(float weight) override;
    void update_tare_display() override;
    void update_progress(int percent) override;
    bool is_visible() const override { return visible_; }
    lv_obj_t* get_screen() const override { return screen_; }

    void add_chart_data_point(float current_weight, float flow_rate, uint32_t current_time_ms) override;
    void reset_chart_data();
    void set_time_mode(bool enabled);
    void set_manual_mode(bool enabled);
    void update_phase(GrindPhase phase, bool paused = false);
    void set_completed(bool completed, bool overshoot = false);
    void set_error_state(bool error);

private:
    static constexpr int kNumParticles = 14;
    static constexpr uint16_t kMiniChartPoints = 60;
    static constexpr uint32_t kChartSampleIntervalMs = 150;
    static constexpr uint32_t kAnimPeriodMs = 40; // ~25 FPS while visible

    // Pixel heights of the coffee fill and of the dome above it (0 = hidden).
    struct MoundShape {
        int16_t fill_h = 0;
        int16_t dome_h = 0;
    };

    struct Particle {
        lv_obj_t* obj = nullptr;
        float x = 140.0f;
        float y = 24.0f;
        float vx = 0.0f;
        float vy = 2.0f;
        uint8_t size = 2;
        bool active = false;
    };

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* profile_label_ = nullptr;
    lv_obj_t* info_label_ = nullptr;
    lv_obj_t* stage_ = nullptr;

    // Clipped coffee mound widgets strictly inside the 60x30 basket
    lv_obj_t* mound_clip_ = nullptr;
    lv_obj_t* mound_fill_ = nullptr;
    lv_obj_t* mound_dome_ = nullptr;

    // Portafilter contour lines & taring indicator
    lv_obj_t* basket_shell_line_ = nullptr;
    lv_obj_t* tare_spinner_ = nullptr;

    // Live weight readout with the target beside it + mini-chart + status badge
    lv_obj_t* weight_label_ = nullptr;
    lv_obj_t* side_label_ = nullptr;
    const lv_font_t* weight_font_ = &lv_font_montserrat_56;
    lv_obj_t* chart_ = nullptr;
    lv_chart_series_t* weight_series_ = nullptr;
    lv_chart_series_t* flow_series_ = nullptr;
    lv_obj_t* flow_legend_label_ = nullptr;
    lv_obj_t* status_badge_label_ = nullptr;

    Particle particles_[kNumParticles];
    lv_timer_t* anim_timer_ = nullptr;

    // Static LVGL 9 precise point arrays (must outlive lv_line widgets)
    lv_point_precise_t basket_shell_pts_[6]{};
    lv_point_precise_t left_spout_pts_[3]{};
    lv_point_precise_t right_spout_pts_[3]{};
    lv_point_precise_t chute_left_pts_[2]{};
    lv_point_precise_t chute_right_pts_[2]{};

    bool visible_ = false;
    bool time_mode_ = false;
    bool manual_mode_ = false;
    bool taring_ = false;
    bool paused_ = false;
    bool completed_ = false;
    bool overshoot_ = false;
    bool error_ = false;
    bool phase_tracking_active_ = false;
    GrindPhase current_phase_ = GrindPhase::IDLE;
    uint8_t pulse_display_count_ = 0;
    float target_weight_g_ = 18.0f;
    float target_time_s_ = 0.0f;
    float current_weight_g_ = 0.0f;
    float current_flow_gps_ = 0.0f;
    int displayed_progress_ = 0;
    int32_t chart_y_max_ = 185;
    float mound_fraction_ = 0.0f;   // Drawn fill level, eased toward the measured one
    MoundShape mound_shape_{};
    bool mound_shape_applied_ = false;
    uint32_t last_status_color_ = 0;
    uint32_t last_chart_sample_ms_ = 0;
    uint32_t last_flow_sample_ms_ = 0;
    uint32_t rng_state_ = 0x5A17C0FEu;
    int16_t tare_angle_ = 0;

    char displayed_weight_text_[16]{};
    char displayed_info_text_[64]{};
    char target_side_text_[20]{};
    char displayed_side_text_[20]{};
    char displayed_flow_text_[24]{};
    char displayed_status_text_[20]{};

    static void anim_timer_cb(lv_timer_t* timer);
    void step_animation();
    float mound_target_fraction() const;
    static MoundShape mound_shape(float fraction);
    float mound_surface_y() const;
    void snap_mound_to_target();
    void advance_mound();
    void apply_mound_shape(const MoundShape& shape);
    void refresh_visual_accent();
    void set_status_badge(const char* text, uint32_t color_hex);
    void set_weight_text(const char* text);
    void set_target_side_text(const char* text);
    void refresh_side_label();
    void layout_weight_row();
    void hide_all_particles();
    void reset_particle(Particle& p, float flow_norm);
    uint32_t next_rand();
    void clear_clickable_recursive(lv_obj_t* obj);
};
