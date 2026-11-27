#pragma once

#include <cstdint>
#include "../../system/screensaver_settings.h"

class UIManager;
class ScreensaverController;
class DisplayManager;

// Implements automatic screen dimming based on touch/weight activity

class ScreenTimeoutController {
public:
    explicit ScreenTimeoutController(UIManager* manager);

    void register_events();
    void update();

    // Apply web/menu preference changes immediately and begin a fresh idle
    // interval so activity recorded before the save cannot postpone the new
    // timeout.
    bool apply_runtime_settings(bool verify_storage = false);

    void set_screensaver_controller(ScreensaverController* controller) {
        screensaver_controller_ = controller;
    }

private:
    void refresh_settings_if_needed(uint32_t now_ms);
    void refresh_morning_note_if_needed(uint32_t now_ms);
    bool try_show_due_morning_note(uint32_t now_ms);
    void account_morning_note_display(uint32_t now_ms);
    void show_idle_screensaver();
    float dimmed_brightness() const;
    void restore_normal_display(DisplayManager* display);
    bool is_protected_state() const;

    UIManager* ui_manager_;
    ScreensaverController* screensaver_controller_ = nullptr;
    ScreensaverTimingSettings timing_settings_;
    uint32_t last_settings_refresh_ms_ = 0;
    uint32_t settings_applied_at_ms_ = 0;
    uint32_t last_weight_activity_ms_ = 0;
    uint32_t last_note_check_ms_ = 0;
    uint32_t last_seen_note_revision_ = 0;
    uint32_t active_note_revision_ = 0;
    // On-screen time of the current note revision (enforces the show-now limit)
    uint32_t note_display_revision_ = 0;
    uint32_t note_display_ms_ = 0;
    uint32_t note_display_tick_ms_ = 0;
    // Back-off after a failed overlay load so a persistent failure cannot
    // reallocate and re-read the 255 KB frame on every UI tick.
    uint32_t note_load_failed_revision_ = 0;
    uint32_t note_load_failed_at_ms_ = 0;
    uint32_t preview_started_at_ms_ = 0;
    uint32_t preview_end_ms_ = 0;
    uint32_t preview_start_weight_ms_ = 0;
    bool note_due_cached_ = false;
    bool preview_active_ = false;
    bool screen_dimmed_;
    bool panel_off_;
};
