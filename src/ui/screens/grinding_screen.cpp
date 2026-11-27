#include "grinding_screen.h"
#include <Preferences.h>

IGrindingScreen* GrindingScreen::screen_for_layout(GrindScreenLayout layout) {
    switch (layout) {
        case GrindScreenLayout::NERDY_CHART:
            return static_cast<IGrindingScreen*>(&chart_screen);
        case GrindScreenLayout::PORTAFILTER:
            return static_cast<IGrindingScreen*>(&portafilter_screen);
        case GrindScreenLayout::MINIMAL_ARC:
        default:
            return static_cast<IGrindingScreen*>(&arc_screen);
    }
}

GrindingScreen::GrindingScreen() : current_layout(GrindScreenLayout::MINIMAL_ARC), preferences(nullptr), current_mode(GrindMode::WEIGHT) {
    // Layout will be loaded in init() when preferences are available
    active_screen = static_cast<IGrindingScreen*>(&arc_screen); // Default to arc screen
}

void GrindingScreen::init(Preferences* prefs) {
    preferences = prefs;
    
    // Load saved layout preference using the provided preferences instance
    if (preferences && preferences->isKey("grind_layout")) {
        int saved_layout = preferences->getInt("grind_layout", static_cast<int>(GrindScreenLayout::MINIMAL_ARC));
        if (saved_layout >= static_cast<int>(GrindScreenLayout::MINIMAL_ARC) &&
            saved_layout <= static_cast<int>(GrindScreenLayout::PORTAFILTER)) {
            current_layout = static_cast<GrindScreenLayout>(saved_layout);
        } else {
            current_layout = GrindScreenLayout::MINIMAL_ARC;
        }
    } else {
        current_layout = GrindScreenLayout::MINIMAL_ARC;
    }
    
    active_screen = screen_for_layout(current_layout);
}

void GrindingScreen::set_layout(GrindScreenLayout layout) {
    if (current_layout == layout) return;
    
    bool was_visible = is_visible();
    
    // Hide current screen
    if (active_screen) {
        active_screen->hide();
    }
    
    // Switch to new layout
    current_layout = layout;
    active_screen = screen_for_layout(layout);
    
    // Show new screen if it was visible
    if (was_visible) {
        active_screen->show();
    }
    
    // Save preference using the provided preferences instance
    if (preferences) {
        preferences->putInt("grind_layout", static_cast<int>(layout));
    }
}

// Delegate all calls to active screen
void GrindingScreen::create() {
    arc_screen.create();
    chart_screen.create();
    portafilter_screen.create();
    const bool time_enabled = (current_mode == GrindMode::TIME);
    arc_screen.set_time_mode(time_enabled);
    chart_screen.set_time_mode(time_enabled);
    portafilter_screen.set_time_mode(time_enabled);
    
    // Hide all inactive screens initially
    arc_screen.hide();
    chart_screen.hide();
    portafilter_screen.hide();
}

void GrindingScreen::show() {
    if (active_screen) active_screen->show();
}

void GrindingScreen::hide() {
    if (active_screen) active_screen->hide();
}

void GrindingScreen::update_profile_name(const char* name) {
    arc_screen.update_profile_name(name);
    chart_screen.update_profile_name(name);
    portafilter_screen.update_profile_name(name);
}

void GrindingScreen::update_target_weight(float weight) {
    arc_screen.update_target_weight(weight);
    chart_screen.update_target_weight(weight);
    portafilter_screen.update_target_weight(weight);
}

void GrindingScreen::update_target_weight_text(const char* text) {
    arc_screen.update_target_weight_text(text);
    chart_screen.update_target_weight_text(text);
    portafilter_screen.update_target_weight_text(text);
}

void GrindingScreen::update_target_time(float seconds) {
    arc_screen.update_target_time(seconds);
    chart_screen.update_target_time(seconds);
    portafilter_screen.update_target_time(seconds);
}

void GrindingScreen::update_current_weight(float weight) {
    arc_screen.update_current_weight(weight);
    chart_screen.update_current_weight(weight);
    portafilter_screen.update_current_weight(weight);
}

void GrindingScreen::update_tare_display() {
    arc_screen.update_tare_display();
    chart_screen.update_tare_display();
    portafilter_screen.update_tare_display();
}

void GrindingScreen::update_progress(int percent) {
    arc_screen.update_progress(percent);
    chart_screen.update_progress(percent);
    portafilter_screen.update_progress(percent);
}

bool GrindingScreen::is_visible() const {
    return active_screen ? active_screen->is_visible() : false;
}

lv_obj_t* GrindingScreen::get_screen() const {
    return active_screen ? active_screen->get_screen() : nullptr;
}

void GrindingScreen::add_chart_data_point(float current_weight, float flow_rate, uint32_t current_time_ms) {
    // Always feed both chart-backed screens even when inactive so switching
    // layouts mid-grind shows complete history immediately.
    chart_screen.add_chart_data_point(current_weight, flow_rate, current_time_ms);
    portafilter_screen.add_chart_data_point(current_weight, flow_rate, current_time_ms);
}

void GrindingScreen::reset_chart_data() {
    chart_screen.reset_chart_data();
    portafilter_screen.reset_chart_data();
}

void GrindingScreen::set_mode(GrindMode mode) {
    current_mode = mode;
    bool time_enabled = (mode == GrindMode::TIME);
    arc_screen.set_time_mode(time_enabled);
    chart_screen.set_time_mode(time_enabled);
    portafilter_screen.set_time_mode(time_enabled);
    portafilter_screen.set_manual_mode(mode == GrindMode::MANUAL);
}

void GrindingScreen::set_chart_time_prediction(uint32_t predicted_time_ms) {
    chart_screen.set_chart_time_prediction(predicted_time_ms);
}

void GrindingScreen::update_phase(GrindPhase phase, bool paused) {
    portafilter_screen.update_phase(phase, paused);
}

void GrindingScreen::set_completed(bool completed, bool overshoot) {
    portafilter_screen.set_completed(completed, overshoot);
}

void GrindingScreen::set_error_state(bool error) {
    portafilter_screen.set_error_state(error);
}
