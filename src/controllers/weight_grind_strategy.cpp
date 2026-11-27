#include "weight_grind_strategy.h"

#include "grind_controller.h"
#include "../hardware/WeightSensor.h"
#include "../hardware/grinder.h"
#include "../logging/grind_logging.h"
#include "../config/constants.h"
#include <Arduino.h>

void WeightGrindStrategy::on_enter(const GrindSessionDescriptor&, GrindStrategyContext&, const GrindLoopData&) {
    // No additional setup required; controller handled initialization.
}

bool WeightGrindStrategy::update(const GrindSessionDescriptor&,
                                 GrindStrategyContext& context,
                                 const GrindLoopData& loop_data) {
    auto* controller = context.controller;
    if (!controller) {
        return false;
    }

    switch (controller->phase) {
        case GrindPhase::PREDICTIVE:
            run_predictive_phase(*controller, loop_data);
            return true;
        case GrindPhase::PULSE_DECISION:
            run_pulse_decision_phase(*controller, loop_data);
            return true;
        case GrindPhase::PULSE_EXECUTE:
            run_pulse_execute_phase(*controller, loop_data);
            return true;
        case GrindPhase::PULSE_SETTLING:
            run_pulse_settling_phase(*controller, loop_data);
            return true;
        default:
            return false;
    }
}

void WeightGrindStrategy::on_exit(const GrindSessionDescriptor&, GrindStrategyContext&) {
    // No teardown required for weight strategy yet.
}

int WeightGrindStrategy::progress_percent(const GrindSessionDescriptor&,
                                          const GrindController&) const {
    // Weight-based progress remains computed directly by the controller.
    return 0;
}

float WeightGrindStrategy::get_clamped_pulse_flow_rate(const GrindController& controller) const {
    float flow_rate = controller.pulse_flow_rate;
    const bool adaptive_on = controller.adaptive_coast_state_.algorithm_enabled;
    const float min_gps = adaptive_on ? adaptive_grind::kAdaptivePulseFlowMinGps : GRIND_FLOW_RATE_MIN_SANE_GPS;
    const float max_gps = adaptive_on ? adaptive_grind::kAdaptivePulseFlowMaxGps : GRIND_FLOW_RATE_MAX_SANE_GPS;

    if (!std::isfinite(flow_rate) || flow_rate < min_gps) {
        flow_rate = adaptive_on ? min_gps : GRIND_PULSE_FLOW_RATE_FALLBACK_GPS;
    } else if (flow_rate > max_gps) {
        flow_rate = max_gps;
    }

    return flow_rate;
}

float WeightGrindStrategy::calculate_pulse_duration_ms(const GrindController& controller,
                                                       float error_grams) const {
    float clamped_flow_rate = get_clamped_pulse_flow_rate(controller);

    // Calculate the productive grinding time needed (excludes startup latency)
    float productive_duration_ms = (error_grams / clamped_flow_rate) * 1000.0f;

    // Motor latency is the base time needed to start the system
    float motor_latency_ms = controller.get_motor_response_latency();

    // Clamp productive duration to valid range (0 to max additional time)
    float clamped_productive_ms = max(0.0f, min(productive_duration_ms, GRIND_MOTOR_MAX_PULSE_DURATION_MS));

    // Total pulse = latency (startup) + productive grinding time
    float final_duration = motor_latency_ms + clamped_productive_ms;

    return final_duration;
}

void WeightGrindStrategy::run_predictive_phase(GrindController& controller,
                                               const GrindLoopData& loop_data) const {
    if (!controller.weight_sensor) {
        return;
    }

    const bool adaptive_on = controller.adaptive_coast_state_.algorithm_enabled;
    const float predictive_delta_g = loop_data.current_weight - controller.predictive_start_weight_;

    if (!controller.flow_start_confirmed) {
        const float current_flow_rate =
            controller.weight_sensor->get_flow_rate(adaptive_grind::kFlowStartDetectionWindowMs);

        // At 10 SPS (100ms/sample), require settled motor + 500ms flow window +
        // real net weight gain (>= 0.20g) when adaptive mode is active so a single
        // motor-kick transient can never false-confirm flow or disarm the watchdog.
        const bool flow_confirmed = adaptive_on
            ? (controller.grinder->is_motor_settled() &&
               predictive_delta_g >= adaptive_grind::kFlowStartMinDeltaG &&
               current_flow_rate >= GRIND_FLOW_DETECTION_THRESHOLD_GPS)
            : (current_flow_rate >= GRIND_FLOW_DETECTION_THRESHOLD_GPS);

        if (flow_confirmed) {
            controller.grind_latency_ms = loop_data.now - controller.phase_start_time;
            controller.flow_start_confirmed = true;
            controller.queue_log_message("[PREDICTIVE] Flow start CONFIRMED! Latency: %.1fms, Flow: %.2fg/s, dW: %.2fg\n",
                                         controller.grind_latency_ms, current_flow_rate, predictive_delta_g);
        } else {
            const uint32_t predictive_elapsed_ms = loop_data.now - controller.phase_start_time;
            if (remote_safety::check_no_flow_watchdog(
                    predictive_elapsed_ms,
                    predictive_delta_g,
                    controller.flow_start_confirmed,
                    controller.active_no_flow_timeout_ms_,
                    controller.active_no_flow_min_weight_g_)) {
                controller.abort_no_flow(loop_data);
                return;
            }
        }
    }

    if (controller.flow_start_confirmed) {
        if (adaptive_on) {
            const uint32_t flow_start_time =
                controller.phase_start_time + static_cast<uint32_t>(controller.grind_latency_ms);
            const uint32_t elapsed_since_flow_ms =
                (loop_data.now > flow_start_time) ? (loop_data.now - flow_start_time) : 0;
            const uint32_t flow_window_ms =
                adaptive_grind::compute_progressive_flow_window_ms(elapsed_since_flow_ms);

            if (flow_window_ms > 0) {
                const float current_flow_rate = controller.weight_sensor->get_flow_rate(flow_window_ms);

                if (current_flow_rate > GRIND_FLOW_DETECTION_THRESHOLD_GPS) {
                    controller.last_predictive_flow_gps_ = current_flow_rate;
                    controller.last_predictive_flow_window_ms_ = flow_window_ms;
                    controller.motor_stop_target_weight =
                        adaptive_grind::compute_motor_stop_target_weight(
                            controller.grind_latency_ms,
                            controller.get_coast_ratio(),
                            current_flow_rate,
                            controller.adaptive_coast_state_);
                }
            }
        } else {
            // Exact 1:1 stock upstream predictive stop calculation
            const uint32_t flow_rate_calc_window_ms = 1500;
            if (loop_data.now > (controller.phase_start_time + controller.grind_latency_ms + flow_rate_calc_window_ms)) {
                const float current_flow_rate = controller.weight_sensor->get_flow_rate(flow_rate_calc_window_ms);
                if (current_flow_rate > GRIND_FLOW_DETECTION_THRESHOLD_GPS) {
                    controller.motor_stop_target_weight =
                        ((controller.grind_latency_ms * controller.get_coast_ratio()) /
                         static_cast<float>(SYS_MS_PER_SECOND)) * current_flow_rate;
                }
            }
        }
    }

    // Only allow motor stop decision after motor has settled to avoid startup transients
    if (controller.grinder->is_motor_settled() &&
        loop_data.current_weight >= (controller.target_weight - controller.motor_stop_target_weight)) {
        controller.grinder->stop();
        controller.predictive_end_weight = loop_data.current_weight;
        controller.pulse_flow_rate = controller.weight_sensor->get_flow_rate_95th_percentile(2500);
        controller.switch_phase(GrindPhase::PULSE_SETTLING, loop_data);
    }
}

void WeightGrindStrategy::run_pulse_decision_phase(GrindController& controller,
                                                   const GrindLoopData& loop_data) const {
    if (!controller.weight_sensor) {
        return;
    }

    const bool adaptive_on = controller.adaptive_coast_state_.algorithm_enabled;

    // Guarantee every sample in the 500ms precision settling window was captured
    // strictly AFTER entering PULSE_DECISION (never stale pre-pulse readings).
    if (adaptive_on &&
        (loop_data.now - controller.phase_start_time) < GRIND_SCALE_PRECISION_SETTLING_TIME_MS) {
        return;
    }

    float settled_weight;
    if (!controller.weight_sensor->check_settling_complete(GRIND_SCALE_PRECISION_SETTLING_TIME_MS, &settled_weight)) {
        return;
    }

    if (adaptive_on) {
        // Stage a coast observation once predictive settles, only when the predictive
        // flow window is >= 1000ms. The controller commits it at completion if
        // adaptive_grind::should_commit_coast_observation() accepts the shot result.
        if (controller.pulse_attempts == 0 && !controller.predictive_coast_learned_this_shot_) {
            controller.predictive_coast_learned_this_shot_ = true;
            if (controller.last_predictive_flow_window_ms_ >= adaptive_grind::kMinCoastLearnFlowWindowMs &&
                controller.last_predictive_flow_gps_ >= 0.4f) {
                controller.pending_coast_valid_ = true;
                controller.pending_coast_end_g_ = controller.predictive_end_weight;
                controller.pending_coast_settled_g_ = settled_weight;
                controller.pending_coast_flow_gps_ = controller.last_predictive_flow_gps_;
            }
        } else if (controller.pulse_attempts > 0) {
            const int last_idx = controller.pulse_attempts - 1;
            if (last_idx >= 0 && last_idx < GRIND_MAX_PULSE_ATTEMPTS) {
                controller.pulse_history[last_idx].end_weight = settled_weight;
                const float delivered_g = settled_weight - controller.pulse_history[last_idx].start_weight;
                const float prev_flow = controller.pulse_flow_rate;
                controller.pulse_flow_rate = adaptive_grind::update_pulse_flow_estimate(
                    controller.pulse_flow_rate,
                    delivered_g,
                    controller.pulse_history[last_idx].duration_ms,
                    controller.get_motor_response_latency());
                controller.queue_log_message("[ADAPTIVE] Pulse #%d delivered %.3fg in %.0fms -> pulse_flow %.2f -> %.2fg/s\n",
                                             controller.pulse_attempts,
                                             delivered_g,
                                             controller.pulse_history[last_idx].duration_ms,
                                             prev_flow,
                                             controller.pulse_flow_rate);
            }
        }
    }

    if (controller.target_weight - settled_weight < GRIND_ACCURACY_TOLERANCE_G ||
        controller.pulse_attempts >= GRIND_MAX_PULSE_ATTEMPTS) {
        controller.switch_phase(GrindPhase::FINAL_SETTLING, loop_data);
        return;
    }

    // In adaptive mode, aim halfway inside [target - tol, target] to eliminate Zeno crawl
    // while keeping 15mg (> 10mg settling noise) headroom against clumps.
    float error = adaptive_on
        ? adaptive_grind::compute_pulse_target_error(
              controller.target_weight, settled_weight, GRIND_ACCURACY_TOLERANCE_G)
        : ((controller.target_weight - GRIND_ACCURACY_TOLERANCE_G) - settled_weight);

    controller.pulse_history[controller.pulse_attempts].start_weight = settled_weight;
    controller.pulse_history[controller.pulse_attempts].end_weight = settled_weight;

    controller.current_pulse_duration_ms = calculate_pulse_duration_ms(controller, error);
    controller.pulse_history[controller.pulse_attempts].duration_ms = controller.current_pulse_duration_ms;

    controller.switch_phase(GrindPhase::PULSE_EXECUTE, loop_data);
    controller.grinder->start_pulse_rmt(static_cast<uint32_t>(controller.current_pulse_duration_ms));

    controller.pulse_attempts++;
}

void WeightGrindStrategy::run_pulse_execute_phase(GrindController& controller,
                                                  const GrindLoopData& loop_data) const {
    if (controller.grinder && controller.grinder->is_pulse_complete()) {
        controller.switch_phase(GrindPhase::PULSE_SETTLING, loop_data);
    }
}

void WeightGrindStrategy::run_pulse_settling_phase(GrindController& controller,
                                                   const GrindLoopData& loop_data) const {
    if (!controller.weight_sensor) {
        return;
    }

    if (controller.adaptive_coast_state_.algorithm_enabled) {
        // Wait for coast clearance (shorter after correction pulses than after full predictive run),
        // then hand off to PULSE_DECISION which enforces a full strictly-post-coast 500ms settling check.
        const uint32_t min_wait_ms = adaptive_grind::compute_settling_delay_ms(
            controller.grind_latency_ms,
            GRIND_MOTOR_SETTLING_TIME_MS,
            controller.pulse_attempts);

        if (loop_data.now - controller.phase_start_time >= min_wait_ms) {
            controller.switch_phase(GrindPhase::PULSE_DECISION, loop_data);
        }
    } else {
        // Exact 1:1 stock upstream settling phase
        if (loop_data.now - controller.phase_start_time >= controller.grind_latency_ms + GRIND_MOTOR_SETTLING_TIME_MS) {
            if (controller.weight_sensor->check_settling_complete(GRIND_MOTOR_SETTLING_TIME_MS)) {
                controller.switch_phase(GrindPhase::PULSE_DECISION, loop_data);
            }
        }
    }
}
