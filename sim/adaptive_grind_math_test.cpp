#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "controllers/adaptive_grind_math.h"

using namespace adaptive_grind;

namespace {

struct GrinderPhysics {
    const char* name;
    float target_g;
    float startup_latency_ms;   // True time until coffee hits cup
    float steady_flow_gps;      // True steady-state flow rate (g/s)
    float true_coast_time_ms;   // True post-stop coast time (ms)
    float motor_latency_ms;     // Relay + spin-up latency (ms)
    float pulse_efficiency;     // Short-pulse flow ratio vs steady_flow (e.g. 0.45)
};

struct ShotResult {
    float final_weight_g;
    float error_g;              // final - target (+ means overshoot)
    int pulses;
    float total_settling_s;
};

// Simulate baseline ("old") zero-shot algorithm from weight_grind_strategy.cpp
ShotResult simulate_old(const GrinderPhysics& p) {
    // Old flow start uses 500ms window -> inflates detected latency by ~170ms
    const float detected_latency_ms = p.startup_latency_ms + 170.0f;
    const float coast_ratio = 1.0f;

    // Predictive stop weight: if dose finishes before detected_latency + 1500ms, stays at 1.0g default
    float stop_lead_g = 1.0f;
    const float time_to_reach_default_ms =
        p.startup_latency_ms + ((p.target_g - 1.0f) / p.steady_flow_gps) * 1000.0f;
    if (time_to_reach_default_ms > detected_latency_ms + 1500.0f) {
        stop_lead_g = (detected_latency_ms * coast_ratio / 1000.0f) * p.steady_flow_gps;
    }

    const float motor_stop_weight = std::max(0.0f, p.target_g - stop_lead_g);
    const float actual_coast_g = (p.true_coast_time_ms / 1000.0f) * p.steady_flow_gps;
    float weight = motor_stop_weight + actual_coast_g;

    // Old settling wait after predictive: detected_latency + 200ms + 200ms + 500ms
    float settling_ms = detected_latency_ms + 200.0f + 200.0f + 500.0f;

    // Old pulse flow rate: 95th percentile (~1.1x steady flow), clamped [1.0, 3.0], NEVER updated
    const float fixed_pulse_flow = std::clamp(p.steady_flow_gps * 1.10f, 1.0f, 3.0f);

    int pulses = 0;
    const float tolerance = 0.03f;
    while ((p.target_g - weight) >= tolerance && pulses < 10) {
        const float err = (p.target_g - tolerance) - weight;
        if (err <= 0.0f) break;
        const float prod_ms = std::clamp((err / fixed_pulse_flow) * 1000.0f, 0.0f, 250.0f);
        const float delivered = (prod_ms / 1000.0f) * (p.steady_flow_gps * p.pulse_efficiency);
        weight += delivered;
        pulses++;
        // Old waits detected_latency + 200 + 200 + 500 per pulse!
        settling_ms += detected_latency_ms + 200.0f + 200.0f + 500.0f;
    }

    return {weight, weight - p.target_g, pulses, settling_ms / 1000.0f};
}

// Simulate improved adaptive zero-shot algorithm
ShotResult simulate_new(const GrinderPhysics& p, AdaptiveCoastState& coast_state) {
    // Faster 250ms flow start detection -> cuts latency bias in half (~85ms)
    const float detected_latency_ms = p.startup_latency_ms + 85.0f;
    const float coast_ratio = 1.0f;

    // Progressive window starts updating at 500ms post flow start
    float stop_lead_g = 1.0f;
    const float time_to_reach_default_ms =
        p.startup_latency_ms + ((p.target_g - 1.0f) / p.steady_flow_gps) * 1000.0f;
    if (time_to_reach_default_ms > detected_latency_ms + 500.0f) {
        stop_lead_g = compute_motor_stop_target_weight(
            detected_latency_ms, coast_ratio, p.steady_flow_gps, coast_state);
    }

    const float motor_stop_weight = std::max(0.0f, p.target_g - stop_lead_g);
    const float actual_coast_g = (p.true_coast_time_ms / 1000.0f) * p.steady_flow_gps;
    float weight = motor_stop_weight + actual_coast_g;

    // Post-predictive settling wait + single 500ms precision check
    float settling_ms =
        static_cast<float>(compute_settling_delay_ms(detected_latency_ms, 200, 0)) + 500.0f;

    // Learn coast after predictive settles
    update_learned_coast_from_predictive(coast_state, motor_stop_weight, weight, p.steady_flow_gps);

    // Initial pulse flow starts at 95th percentile (~1.1x steady), then adapts after each pulse!
    float pulse_flow = std::clamp(p.steady_flow_gps * 1.10f, kAdaptivePulseFlowMinGps, kAdaptivePulseFlowMaxGps);

    int pulses = 0;
    const float tolerance = 0.03f;
    while ((p.target_g - weight) >= tolerance && pulses < 10) {
        const float err = compute_pulse_target_error(p.target_g, weight, tolerance);
        if (err <= 0.0f) break;
        const float prod_ms = std::clamp((err / pulse_flow) * 1000.0f, 0.0f, 250.0f);
        const float total_ms = p.motor_latency_ms + prod_ms;
        const float delivered = (prod_ms / 1000.0f) * (p.steady_flow_gps * p.pulse_efficiency);
        weight += delivered;
        pulses++;

        // Update pulse flow from actual delivered weight
        pulse_flow = update_pulse_flow_estimate(pulse_flow, delivered, total_ms, p.motor_latency_ms);

        // Post-pulse shorter settling wait + single 500ms precision check
        settling_ms +=
            static_cast<float>(compute_settling_delay_ms(detected_latency_ms, 200, pulses)) + 500.0f;
    }

    return {weight, weight - p.target_g, pulses, settling_ms / 1000.0f};
}

} // namespace

int main() {
    // 1. Progressive flow window unit tests
    assert(compute_progressive_flow_window_ms(0) == 0);
    assert(compute_progressive_flow_window_ms(499) == 0);
    assert(compute_progressive_flow_window_ms(500) == 500);
    assert(compute_progressive_flow_window_ms(900) == 900);
    assert(compute_progressive_flow_window_ms(2500) == 1500);

    // 2. Cold chute latency clamping in pure zero-shot mode
    AdaptiveCoastState empty_state;
    const float cold_stop = compute_motor_stop_target_weight(1200.0f, 1.0f, 2.0f, empty_state);
    // Clamped to 650ms -> 0.65 * 2.0 = 1.30g (instead of 2.40g!)
    assert(std::fabs(cold_stop - 1.30f) < 0.01f);

    // 2b. Zeno-free pulse target error aims inside acceptance band
    const float pulse_err = compute_pulse_target_error(18.00f, 17.95f, 0.03f);
    assert(std::fabs(pulse_err - 0.0425f) < 0.001f);

    // 3. Cross-shot coast learning & auto-reset on grind/bean change (>25% flow shift)
    AdaptiveCoastState state;
    assert(update_learned_coast_from_predictive(state, 17.0f, 17.64f, 1.6f));
    assert(state.sample_count == 1);
    assert(std::fabs(state.learned_coast_time_ms - 400.0f) < 1.0f);
    assert(!should_reset_learned_coast(state, 1.8f));  // +12.5% -> keep learned
    assert(should_reset_learned_coast(state, 2.2f));   // +37.5% -> auto-reset to zero-shot!

    // Disabled state ignores updates and stays pure zero-shot
    AdaptiveCoastState disabled_state;
    disabled_state.enabled = false;
    assert(!update_learned_coast_from_predictive(disabled_state, 17.0f, 17.64f, 1.6f));
    assert(disabled_state.sample_count == 0);

    // 4. Within-shot adaptive pulse flow learning
    // Predicted 2.0 g/s, actually delivered 0.10g in 100ms productive (= 1.0 g/s observed)
    // Conservative observed = 1.15 g/s; blended = 0.5*2.0 + 0.5*1.15 = 1.575 g/s
    const float adapted = update_pulse_flow_estimate(2.0f, 0.10f, 150.0f, 50.0f);
    assert(std::fabs(adapted - 1.575f) < 0.01f);

    // Zero-delivery meaningful pulse gently decays flow by 0.85x
    const float decayed = update_pulse_flow_estimate(2.0f, 0.0f, 110.0f, 50.0f);
    assert(std::fabs(decayed - 1.70f) < 0.01f);

    // Tiny pulse (<20ms productive) does not corrupt flow estimate
    assert(std::fabs(update_pulse_flow_estimate(2.0f, 0.0f, 60.0f, 50.0f) - 2.0f) < 0.001f);

    // 5. Settling delay differentiation (post-predictive vs post-pulse)
    assert(compute_settling_delay_ms(900.0f, 200, 0) == 850); // capped 650 + 200
    assert(compute_settling_delay_ms(900.0f, 200, 1) == 500); // capped 300 + 200

    // 6. Closed-loop physics benchmark across realistic Eureka Specialita scenarios
    const GrinderPhysics scenarios[] = {
        {"Cold Morning Chute (18g)",  18.0f, 850.0f, 1.6f, 380.0f, 50.0f, 0.48f},
        {"Warm Double Shot #2 (18g)", 18.0f, 420.0f, 1.6f, 380.0f, 50.0f, 0.48f},
        {"Fine Light Roast (18g)",    18.0f, 550.0f, 0.95f, 340.0f, 50.0f, 0.45f},
        {"Single Shot (8g)",           8.0f, 450.0f, 1.8f, 360.0f, 50.0f, 0.50f},
        {"Coarse Filter Change (20g)",20.0f, 320.0f, 2.7f, 420.0f, 50.0f, 0.55f},
    };

    AdaptiveCoastState sim_state;
    std::printf("\n%-28s | %-21s | %-21s\n", "Scenario", "OLD (pulses/err/s)", "NEW (pulses/err/s)");
    std::printf("------------------------------------------------------------------------------\n");
    for (const auto& s : scenarios) {
        const ShotResult old_r = simulate_old(s);
        const ShotResult new_r = simulate_new(s, sim_state);
        std::printf("%-28s | %2dp / %+.3fg / %4.1fs | %2dp / %+.3fg / %4.1fs\n",
                    s.name,
                    old_r.pulses, old_r.error_g, old_r.total_settling_s,
                    new_r.pulses, new_r.error_g, new_r.total_settling_s);
        // Critical invariants: ZERO overshoot (<= 0.01g), dramatically fewer pulses, faster settling!
        assert(new_r.error_g <= 0.01f);
        assert(std::fabs(new_r.error_g) <= 0.03f);
        assert(new_r.pulses < old_r.pulses);
        assert(new_r.total_settling_s < old_r.total_settling_s);
    }

    std::printf("\nadaptive_grind_math_test PASSED\n");
    return 0;
}
