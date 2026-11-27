#pragma once
#include "grinding_screen_base.h"
#include "grinding_screen_arc.h"
#include "grinding_screen_chart.h"
#include "grinding_screen_portafilter.h"
#include <Preferences.h>
#include <cmath>
#include "../../controllers/grind_mode.h"

// Unified grinding screen that wraps all layout implementations
class GrindingScreen : public IGrindingScreen {
private:
    IGrindingScreen* active_screen;
    GrindScreenLayout current_layout;
    GrindingScreenArc arc_screen;
    GrindingScreenChart chart_screen;
    GrindingScreenPortafilter portafilter_screen;
    Preferences* preferences;
    GrindMode current_mode;

    IGrindingScreen* screen_for_layout(GrindScreenLayout layout);
    
public:
    GrindingScreen();
    void init(Preferences* prefs);
    void set_layout(GrindScreenLayout layout);
    GrindScreenLayout get_layout() const { return current_layout; }
    
    // IGrindingScreen implementation - delegates to active screen
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
    bool is_visible() const override;
    lv_obj_t* get_screen() const override;
    void add_chart_data_point(float current_weight, float flow_rate, uint32_t current_time_ms) override;
    void reset_chart_data();
    void set_mode(GrindMode mode);
    void set_chart_time_prediction(uint32_t predicted_time_ms);
    void update_phase(GrindPhase phase, bool paused = false);
    void set_completed(bool completed, bool overshoot = false, float final_weight_g = NAN);
    void set_error_state(bool error);

    // ADDED: Public accessors for individual screen objects
    lv_obj_t* get_arc_screen_obj() const { return arc_screen.get_screen(); }
    lv_obj_t* get_chart_screen_obj() const { return chart_screen.get_screen(); }
    lv_obj_t* get_portafilter_screen_obj() const { return portafilter_screen.get_screen(); }
};
