#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace adaptive_grind {

// Pure C++17 math helpers for the zero-shot + adaptive grind-by-weight algorithm.
// Kept free of Arduino / ESP-IDF dependencies so host unit tests and closed-loop
// physics simulations can validate every change directly.

static constexpr const char* kPrefsNamespace = "adgrind";
static constexpr const char* kPrefKeyAlgorithmOn = "ad_on";
static constexpr const char* kPrefKeyEnabled = "enabled";
static constexpr const char* kPrefKeyCoastMs = "coast_ms";
static constexpr const char* kPrefKeyRefFlow = "ref_flow";
static constexpr const char* kPrefKeySamples = "samples";

static constexpr std::size_t kMaxNvsKeyLen = 15;
static_assert(sizeof("adgrind") - 1 <= kMaxNvsKeyLen, "NVS namespace <= 15 chars");
static_assert(sizeof("ad_on") - 1 <= kMaxNvsKeyLen, "NVS key <= 15 chars");
static_assert(sizeof("enabled") - 1 <= kMaxNvsKeyLen, "NVS key <= 15 chars");
static_assert(sizeof("coast_ms") - 1 <= kMaxNvsKeyLen, "NVS key <= 15 chars");
static_assert(sizeof("ref_flow") - 1 <= kMaxNvsKeyLen, "NVS key <= 15 chars");
static_assert(sizeof("samples") - 1 <= kMaxNvsKeyLen, "NVS key <= 15 chars");

// Flow-start detection & progressive predictive window constants.
// HX711 runs at 10 SPS (100ms/sample), so flow-start detection requires a 500ms
// window (5 samples) plus minimum net weight gain to reject motor-kick spikes.
static constexpr uint32_t kFlowStartDetectionWindowMs = 500;
static constexpr float kFlowStartMinDeltaG = 0.20f;
static constexpr uint32_t kProgressiveFlowMinWindowMs = 500;
static constexpr uint32_t kProgressiveFlowMaxWindowMs = 1500;
static constexpr uint32_t kMinCoastLearnFlowWindowMs = 1000;

// Coast time bounds (prevents cold/empty morning chute startup delay from
// inflating post-stop coast prediction and forcing many tiny pulses)
static constexpr float kEffectiveLatencyMinMs = 150.0f;
static constexpr float kEffectiveLatencyMaxMs = 650.0f;
static constexpr float kLearnedCoastMinMs = 100.0f;
static constexpr float kLearnedCoastMaxMs = 900.0f;
static constexpr float kLearnedCoastSafetyMargin = 1.12f;        // 12% conservative undershoot buffer
static constexpr float kLearnedCoastQuantizationGuardMs = 100.0f; // 1 full HX711 sample period @ 10 SPS
static constexpr float kCoastLearnAlpha = 0.30f;                 // Gentle EMA across shots
static constexpr float kFlowChangeResetRatio = 0.25f;            // >25% flow shift resets learned coast

// Within-shot adaptive pulse learning bounds (clump-safe)
static constexpr float kAdaptivePulseFlowMinGps = 0.85f;
static constexpr float kAdaptivePulseFlowMaxGps = 3.00f;
static constexpr float kAdaptivePulseSafetyFactor = 1.15f; // Higher est. flow -> shorter, safer pulse
static constexpr float kAdaptivePulseAlpha = 0.50f;
static constexpr float kMinProductiveMsForFlowUpdate = 20.0f;
static constexpr float kMinDeliveredGramsForFlowUpdate = 0.02f;
static constexpr float kZeroDeliveryProductiveThresholdMs = 40.0f;
static constexpr float kZeroDeliveryDecayFactor = 0.85f;
// Aim halfway inside [target - tol, target] (15mg headroom > 10mg settling noise)
// to prevent Zeno crawl while staying clump-safe against overshoot.
static constexpr float kPulseTargetToleranceFraction = 0.50f;

// Post-stop settling wait caps
static constexpr float kPostPredictiveMaxCoastWaitMs = 650.0f;
static constexpr float kPostPulseMaxCoastWaitMs = 300.0f;

struct AdaptiveCoastState {
    bool algorithm_enabled = true;      // Master Kill-Switch: false = 1:1 stock upstream algorithm
    bool enabled = true;                // Cross-shot coast learning enable
    float learned_coast_time_ms = 0.0f; // 0 = unlearned (pure zero-shot)
    float reference_flow_gps = 0.0f;
    uint32_t sample_count = 0;

    void reset_learned() {
        learned_coast_time_ms = 0.0f;
        reference_flow_gps = 0.0f;
        sample_count = 0;
    }
};

inline bool is_finite_positive(float v) {
    return std::isfinite(v) && v > 0.0f;
}

// Compute the sliding window length (ms) for predictive flow estimation.
// Returns 0 until enough post-flow-start data exists (kProgressiveFlowMinWindowMs),
// then smoothly expands from 500ms up to 1500ms so even short single-dose shots
// get accurate predictive stop weights instead of staying blind on the 1.0g default.
inline uint32_t compute_progressive_flow_window_ms(uint32_t elapsed_since_flow_start_ms) {
    if (elapsed_since_flow_start_ms < kProgressiveFlowMinWindowMs) {
        return 0;
    }
    return std::min(elapsed_since_flow_start_ms, kProgressiveFlowMaxWindowMs);
}

// Returns true when the current shot's flow rate diverged significantly from the
// reference flow rate of previously learned shots (e.g., user changed grind setting
// or beans), signalling an automatic fallback to pure zero-shot.
inline bool should_reset_learned_coast(const AdaptiveCoastState& state,
                                       float current_flow_gps) {
    if (state.sample_count == 0 || !is_finite_positive(state.reference_flow_gps) ||
        !is_finite_positive(current_flow_gps)) {
        return false;
    }
    const float rel_diff =
        std::fabs(current_flow_gps - state.reference_flow_gps) / state.reference_flow_gps;
    return rel_diff > kFlowChangeResetRatio;
}

// Compute the predictive motor-stop lead weight (grams before target at which to
// stop the motor). Uses bounded zero-shot latency when unlearned or after a flow
// regime change; uses conservatively-margined learned coast time on repeat shots
// with matching flow rate.
inline float compute_motor_stop_target_weight(float measured_latency_ms,
                                              float coast_ratio,
                                              float current_flow_gps,
                                              const AdaptiveCoastState& coast_state) {
    if (!is_finite_positive(current_flow_gps)) {
        return 1.0f;
    }

    const float clamped_ratio =
        std::isfinite(coast_ratio) ? std::clamp(coast_ratio, 0.7f, 1.5f) : 1.0f;

    float effective_coast_ms = 0.0f;
    const bool can_use_learned =
        coast_state.enabled &&
        coast_state.sample_count > 0 &&
        is_finite_positive(coast_state.learned_coast_time_ms) &&
        !should_reset_learned_coast(coast_state, current_flow_gps);

    if (can_use_learned) {
        // Apply user's coast_ratio, conservative undershoot margin, and 1 full 10 SPS
        // HX711 sample period (100ms) so sampling quantization cannot cause overshoot.
        effective_coast_ms = std::clamp(
            (coast_state.learned_coast_time_ms * kLearnedCoastSafetyMargin +
             kLearnedCoastQuantizationGuardMs) *
                clamped_ratio,
            kLearnedCoastMinMs,
            kLearnedCoastMaxMs);
    } else {
        // Pure zero-shot with latency clamping so a cold/empty morning chute
        // startup delay does not massively over-predict post-stop coast, plus a
        // 50ms half-sample guard for 10 SPS HX711 quantization on fast coarse grinds.
        const float safe_latency_ms =
            std::isfinite(measured_latency_ms)
                ? std::clamp(measured_latency_ms + 50.0f,
                             kEffectiveLatencyMinMs,
                             kEffectiveLatencyMaxMs)
                : kEffectiveLatencyMinMs;
        effective_coast_ms = safe_latency_ms * clamped_ratio;
    }

    return (effective_coast_ms / 1000.0f) * current_flow_gps;
}

// Compute the pulse target error (grams) aiming halfway inside the acceptance
// band [target - tolerance, target]. Aiming at (target - tolerance) directly
// causes a Zeno crawl where a conservative controller asymptotically approaches
// the stopping threshold without crossing it.
inline float compute_pulse_target_error(float target_weight_g,
                                        float settled_weight_g,
                                        float tolerance_g) {
    const float safe_tol = (std::isfinite(tolerance_g) && tolerance_g > 0.0f) ? tolerance_g : 0.03f;
    const float aim_weight_g = target_weight_g - (safe_tol * kPulseTargetToleranceFraction);
    return std::max(0.0f, aim_weight_g - settled_weight_g);
}

// Decide whether the coast observation staged after PREDICTIVE may be committed.
// Successful shots always teach. An overshoot teaches only when the predictive
// stage alone already landed past the target: that is direct evidence the coast
// was underestimated, and ignoring it would repeat the same overshoot on every
// following shot. Overshoots produced later by a correction pulse are not coast
// evidence and stay excluded, as do aborted, timed-out and max-pulse sessions.
inline bool should_commit_coast_observation(bool shot_succeeded,
                                            bool shot_overshot,
                                            float predictive_settled_weight_g,
                                            float target_weight_g,
                                            float tolerance_g) {
    if (shot_succeeded) {
        return true;
    }
    if (!shot_overshot || !std::isfinite(predictive_settled_weight_g) ||
        !std::isfinite(target_weight_g) || !std::isfinite(tolerance_g)) {
        return false;
    }
    return (predictive_settled_weight_g - target_weight_g) > tolerance_g;
}

// Update cross-shot coast time estimate right after the PREDICTIVE phase settles
// (before the first correction pulse). Safe against anomalies / negative deltas.
// `predictive_overshot` selects fast-attack learning: too much coffee cannot be
// taken back, while a too-early stop only costs one correction pulse, so an
// observed overshoot is adopted in full instead of being averaged in slowly.
inline bool update_learned_coast_from_predictive(AdaptiveCoastState& state,
                                                 float predictive_end_weight_g,
                                                 float settled_weight_g,
                                                 float predictive_flow_gps,
                                                 bool predictive_overshot = false) {
    if (!state.enabled) {
        return false;
    }
    if (!std::isfinite(predictive_end_weight_g) || !std::isfinite(settled_weight_g) ||
        !is_finite_positive(predictive_flow_gps) || predictive_flow_gps < 0.4f) {
        return false;
    }

    const float actual_coast_g = settled_weight_g - predictive_end_weight_g;
    if (actual_coast_g <= 0.05f) {
        return false;
    }

    const float observed_coast_ms = std::clamp(
        (actual_coast_g / predictive_flow_gps) * 1000.0f,
        kLearnedCoastMinMs,
        kLearnedCoastMaxMs);

    if (state.sample_count == 0 || should_reset_learned_coast(state, predictive_flow_gps)) {
        state.learned_coast_time_ms = observed_coast_ms;
        state.reference_flow_gps = predictive_flow_gps;
        state.sample_count = 1;
    } else {
        const float blended_coast_ms =
            (1.0f - kCoastLearnAlpha) * state.learned_coast_time_ms +
            kCoastLearnAlpha * observed_coast_ms;
        state.learned_coast_time_ms =
            predictive_overshot ? std::max(blended_coast_ms, observed_coast_ms) : blended_coast_ms;
        state.reference_flow_gps =
            (1.0f - kCoastLearnAlpha) * state.reference_flow_gps +
            kCoastLearnAlpha * predictive_flow_gps;
        state.sample_count += 1;
    }
    return true;
}

// Update within-shot pulse flow rate estimate after a correction pulse settles.
// Pure zero-shot within the active grind session: learns the real short-pulse
// delivery efficiency of the burrs while preserving an overshoot-safe margin.
inline float update_pulse_flow_estimate(float current_pulse_flow_gps,
                                        float delivered_grams,
                                        float pulse_duration_ms,
                                        float motor_latency_ms,
                                        float min_sane_gps = kAdaptivePulseFlowMinGps,
                                        float max_sane_gps = kAdaptivePulseFlowMaxGps) {
    const float safe_current =
        is_finite_positive(current_pulse_flow_gps)
            ? std::clamp(current_pulse_flow_gps, min_sane_gps, max_sane_gps)
            : 1.5f;

    if (!std::isfinite(delivered_grams) || !std::isfinite(pulse_duration_ms) ||
        !std::isfinite(motor_latency_ms)) {
        return safe_current;
    }

    const float productive_ms = pulse_duration_ms - motor_latency_ms;
    if (productive_ms < kMinProductiveMsForFlowUpdate) {
        return safe_current;
    }

    if (delivered_grams < kMinDeliveredGramsForFlowUpdate) {
        // Meaningful pulse produced almost no coffee: gently lower estimated
        // flow rate so the next pulse is slightly longer instead of stalling.
        if (productive_ms >= kZeroDeliveryProductiveThresholdMs) {
            return std::clamp(safe_current * kZeroDeliveryDecayFactor, min_sane_gps, max_sane_gps);
        }
        return safe_current;
    }

    const float observed_gps = delivered_grams / (productive_ms / 1000.0f);
    // Multiply by kAdaptivePulseSafetyFactor (>1) so higher estimated flow yields
    // slightly shorter, overshoot-safe pulses.
    const float conservative_observed_gps =
        std::clamp(observed_gps * kAdaptivePulseSafetyFactor, min_sane_gps, max_sane_gps);

    const float blended =
        (1.0f - kAdaptivePulseAlpha) * safe_current +
        kAdaptivePulseAlpha * conservative_observed_gps;

    return std::clamp(blended, min_sane_gps, max_sane_gps);
}

// Compute minimum wait time (ms) in PULSE_SETTLING before entering PULSE_DECISION.
// Post-predictive needs full coast clearance; post-pulse short bursts carry far
// less burr inertia and do not need a full cold-start latency wait.
inline uint32_t compute_settling_delay_ms(float grind_latency_ms,
                                          uint32_t motor_settling_time_ms,
                                          int pulse_attempts) {
    const float safe_latency = std::isfinite(grind_latency_ms) && grind_latency_ms > 0.0f
                                   ? grind_latency_ms
                                   : 0.0f;
    const float capped_coast_ms = (pulse_attempts <= 0)
                                      ? std::min(safe_latency, kPostPredictiveMaxCoastWaitMs)
                                      : std::min(safe_latency, kPostPulseMaxCoastWaitMs);
    return static_cast<uint32_t>(capped_coast_ms) + motor_settling_time_ms;
}

} // namespace adaptive_grind
