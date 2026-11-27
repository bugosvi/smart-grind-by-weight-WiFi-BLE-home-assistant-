#include "screen_timeout_controller.h"
#include "screensaver_controller.h"

#include <Arduino.h>

#include "../../config/constants.h"
#include "../../config/logging.h"
#include "../../hardware/display_manager.h"
#include "../../hardware/hardware_manager.h"
#include "../../system/display_idle_policy.h"
#include "../../system/morning_note_store.h"
#include "../ui_manager.h"

ScreenTimeoutController::ScreenTimeoutController(UIManager* manager)
    : ui_manager_(manager)
    , timing_settings_(ScreensaverSettings::load_timing())
    , settings_applied_at_ms_(millis())
    , last_weight_activity_ms_(settings_applied_at_ms_)
    , screen_dimmed_(false)
    , panel_off_(false) {}

void ScreenTimeoutController::register_events() {}

float ScreenTimeoutController::dimmed_brightness() const {
    float dimmed = USER_SCREEN_BRIGHTNESS_DIMMED;
    if (ui_manager_ && ui_manager_->menu_controller_) {
        dimmed = ui_manager_->menu_controller_->get_screensaver_brightness();
    }
    return dimmed;
}

void ScreenTimeoutController::refresh_morning_note_if_needed(uint32_t now_ms) {
    constexpr uint32_t kNoteCheckIntervalMs = 1000;
    const uint32_t current_rev = morning_note_store.revision();
    if (last_note_check_ms_ != 0 &&
        current_rev == last_seen_note_revision_ &&
        now_ms - last_note_check_ms_ < kNoteCheckIntervalMs) {
        return;
    }
    note_due_cached_ = morning_note_store.is_due_now();
    last_seen_note_revision_ = morning_note_store.revision();
    last_note_check_ms_ = now_ms;
}

void ScreenTimeoutController::update() {
    if (!ui_manager_) {
        return;
    }

    auto* hardware = ui_manager_->hardware_manager;
    if (!hardware) {
        return;
    }

    auto* display = hardware->get_display();
    if (!display) {
        return;
    }

    if (is_protected_state()) {
        preview_active_ = false;
        restore_normal_display(display);
        return;
    }

    uint32_t now = millis();
    refresh_settings_if_needed(now);
    refresh_morning_note_if_needed(now);

    auto* touch_driver = display->get_touch_driver();
    if (!touch_driver) {
        return;
    }

    uint32_t ms_since_touch = touch_driver->get_ms_since_last_touch();
    auto* sensor = hardware->get_weight_sensor();
    uint32_t idle_timeout_ms = ScreensaverSettings::idle_timeout_ms(timing_settings_);
    uint32_t display_off_timeout_ms =
        ScreensaverSettings::display_off_timeout_ms(timing_settings_);
    const uint32_t ms_since_settings_applied = now - settings_applied_at_ms_;
    float weight_delta_g = 0.0f;
    if (sensor && sensor->get_weight_delta(1000U, &weight_delta_g) &&
        display_weight_activity_detected(weight_delta_g, USER_WEIGHT_ACTIVITY_THRESHOLD_G)) {
        last_weight_activity_ms_ = now;
    }

    // Handle on-demand Morning Note preview requested from the Web Studio
    if (const uint16_t preview_s = morning_note_store.consume_preview_request(); preview_s > 0) {
        const bool on_ready = ui_manager_->state_machine &&
                              ui_manager_->state_machine->is_state(UIState::READY);
        if (on_ready && screensaver_controller_) {
            restore_normal_display(display);
            if (screensaver_controller_->show_morning_note(false)) {
                preview_active_ = true;
                preview_started_at_ms_ = now;
                preview_end_ms_ = now + static_cast<uint32_t>(preview_s) * 1000U;
                preview_start_weight_ms_ = last_weight_activity_ms_;
                active_note_revision_ = last_seen_note_revision_;
                LOG_BLE("[NOTE] Previewing morning note for %us\n", static_cast<unsigned>(preview_s));
                return;
            }
        }
    }

    if (preview_active_) {
        const uint32_t preview_elapsed_ms = now - preview_started_at_ms_;
        const bool expired = static_cast<int32_t>(now - preview_end_ms_) >= 0;
        const bool fresh_touch = (preview_elapsed_ms > 150U) && (ms_since_touch < preview_elapsed_ms);
        const bool fresh_weight = last_weight_activity_ms_ != preview_start_weight_ms_;
        const bool overlay_gone = !screensaver_controller_ ||
                                  !screensaver_controller_->is_showing_morning_note();
        if (expired || fresh_touch || fresh_weight || overlay_gone) {
            preview_active_ = false;
            restore_normal_display(display);
            settings_applied_at_ms_ = now;
        }
        return;
    }

    // Use the age of the most recent real activity. Comparing the ends of a
    // short weight window rejects isolated scale noise that could otherwise
    // wake the panel repeatedly from a long min/max history.
    const uint32_t ms_since_weight = now - last_weight_activity_ms_;
    const uint32_t inactive_ms = min(min(ms_since_touch, ms_since_weight),
                                     ms_since_settings_applied);
    const DisplayIdleState idle_state = display_idle_state(
        inactive_ms,
        idle_timeout_ms,
        timing_settings_.display_off_enabled,
        static_cast<uint32_t>(timing_settings_.display_off_delay_s) * 1000U);
    const bool should_dim = idle_state != DisplayIdleState::ACTIVE;
    const bool should_turn_off = idle_state == DisplayIdleState::PANEL_OFF;

    if (!should_dim) {
        if (screen_dimmed_ || panel_off_ ||
            (screensaver_controller_ && screensaver_controller_->is_showing_morning_note())) {
            restore_normal_display(display);
        }
        return;
    }

    // Scheduled Morning Note takes priority while idle (including waking an
    // overnight powered-off AMOLED panel at dimmed brightness with pixel-shift).
    // Load the note overlay before enabling panel power so waking from sleep
    // never flashes ReadyScreen or strands the panel on if loading fails.
    if (note_due_cached_ && screensaver_controller_) {
        bool note_ready =
            screensaver_controller_->is_showing_morning_note() &&
            active_note_revision_ == last_seen_note_revision_;
        if (!note_ready) {
            if (screensaver_controller_->is_visible()) {
                screensaver_controller_->hide();
            }
            if (screensaver_controller_->show_morning_note(true)) {
                active_note_revision_ = last_seen_note_revision_;
                note_ready = true;
                LOG_BLE("[NOTE] Displaying morning note overlay (dimmed, pixel-shift ON)\n");
            }
        }
        if (note_ready) {
            display->set_brightness(dimmed_brightness());
            screen_dimmed_ = true;
            if (panel_off_ || !display->is_panel_powered_on()) {
                display->set_panel_power(true);
                panel_off_ = false;
                LOG_BLE("[NOTE] Woke AMOLED panel for scheduled morning note\n");
            }
            return;
        }
    }

    // Note window ended or was cleared while idle: exit morning note cleanly.
    if (screensaver_controller_ && screensaver_controller_->is_showing_morning_note()) {
        screensaver_controller_->hide();
        active_note_revision_ = 0;
        if (!should_turn_off &&
            screensaver_controller_->is_sleep_enabled() &&
            screensaver_controller_->has_image()) {
            screensaver_controller_->show();
        }
    }

    if (should_turn_off && !panel_off_) {
        if (!screen_dimmed_) {
            display->set_brightness(dimmed_brightness());
            screen_dimmed_ = true;
            if (screensaver_controller_ &&
                screensaver_controller_->is_sleep_enabled() &&
                screensaver_controller_->has_image()) {
                screensaver_controller_->show();
            }
        }
        display->set_panel_power(false);
        panel_off_ = true;
        LOG_BLE("[DISPLAY] Panel off after %lus idle (%lus after screensaver)\n",
                static_cast<unsigned long>(display_off_timeout_ms / 1000U),
                static_cast<unsigned long>(timing_settings_.display_off_delay_s));
    } else if (should_dim && !screen_dimmed_) {
        display->set_brightness(dimmed_brightness());
        screen_dimmed_ = true;

        // Show screensaver image if enabled
        if (screensaver_controller_ &&
            screensaver_controller_->is_sleep_enabled() &&
            screensaver_controller_->has_image()) {
            screensaver_controller_->show();
        }
        LOG_BLE("[DISPLAY] Idle timeout reached after %lus; dimmed with screensaver=%s\n",
                static_cast<unsigned long>(timing_settings_.idle_timeout_s),
                screensaver_controller_ && screensaver_controller_->is_visible() ? "ON" : "OFF");
    }
}

bool ScreenTimeoutController::apply_runtime_settings(bool verify_storage) {
    ScreensaverTimingSettings loaded;
    if (verify_storage) {
        if (!ScreensaverSettings::load_timing_checked(loaded)) return false;
    } else {
        loaded = ScreensaverSettings::load_timing();
    }
    timing_settings_ = loaded;
    settings_applied_at_ms_ = millis();
    last_weight_activity_ms_ = settings_applied_at_ms_;
    last_settings_refresh_ms_ = settings_applied_at_ms_;
    LOG_BLE("[DISPLAY] Runtime settings applied; screensaver=%lus panel-off=%s delay=%lus\n",
            static_cast<unsigned long>(timing_settings_.idle_timeout_s),
            timing_settings_.display_off_enabled ? "ON" : "OFF",
            static_cast<unsigned long>(timing_settings_.display_off_delay_s));

    if (!ui_manager_ || !ui_manager_->hardware_manager) return false;
    DisplayManager* display = ui_manager_->hardware_manager->get_display();
    if (!display) return false;
    restore_normal_display(display);
    return true;
}

void ScreenTimeoutController::refresh_settings_if_needed(uint32_t now_ms) {
    constexpr uint32_t kRefreshIntervalMs = 5000;

    if (last_settings_refresh_ms_ != 0 &&
        now_ms - last_settings_refresh_ms_ < kRefreshIntervalMs) {
        return;
    }

    timing_settings_ = ScreensaverSettings::load_timing();
    last_settings_refresh_ms_ = now_ms;
}

void ScreenTimeoutController::restore_normal_display(DisplayManager* display) {
    bool screensaver_visible = screensaver_controller_ && screensaver_controller_->is_visible();
    if (screensaver_visible) {
        screensaver_controller_->hide();
    }

    if (screen_dimmed_ || panel_off_ || screensaver_visible) {
        float normal = USER_SCREEN_BRIGHTNESS_NORMAL;
        if (ui_manager_ && ui_manager_->menu_controller_) {
            normal = ui_manager_->menu_controller_->get_normal_brightness();
        }
        display->set_brightness(normal);
    }

    if (panel_off_ || !display->is_panel_powered_on()) {
        display->set_panel_power(true);
    }

    active_note_revision_ = 0;
    screen_dimmed_ = false;
    panel_off_ = false;
}

bool ScreenTimeoutController::is_protected_state() const {
    if (!ui_manager_) {
        return false;
    }

    bool ota_active = ui_manager_->bluetooth_manager &&
                      ui_manager_->bluetooth_manager->is_updating();
    if (ota_active) {
        return true;
    }

    if (!ui_manager_->state_machine) {
        return false;
    }

    UIState state = ui_manager_->state_machine->get_current_state();
    return state == UIState::GRINDING ||
           state == UIState::OTA_UPDATE ||
           state == UIState::OTA_UPDATE_FAILED;
}
