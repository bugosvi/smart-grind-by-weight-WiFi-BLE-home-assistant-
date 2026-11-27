#pragma once

#include <cmath>
#include <cstdint>

//==============================================================================
// REMOTE GRIND SAFETY & INTERLOCK LOGIC
//==============================================================================
// Pure, hardware-independent safety validation for automated / remote grinds
// (Home Assistant, Web API) and real-time No-Flow Watchdog protection.
// Keep this header free of Arduino/ESP-IDF dependencies so it can be verified
// directly by host unit tests in sim/.

namespace remote_safety {

// NVS namespace and keys (ESP32 NVS keys must be <= 15 characters)
inline constexpr const char* kNvsNamespace = "safety";
inline constexpr const char* kKeyRequireCup = "req_cup";
inline constexpr const char* kKeyEmptyRaw = "empty_raw";
inline constexpr const char* kKeyEmptyCal = "empty_cal";
inline constexpr const char* kKeyCupMinG = "cup_min_g";
inline constexpr const char* kKeyCupMaxG = "cup_max_g";
inline constexpr const char* kKeyMaxTimeS = "rem_max_s";
inline constexpr const char* kKeyNoFlowMs = "noflow_ms";
inline constexpr const char* kKeyNoFlowG = "noflow_g";
inline constexpr const char* kKeyWifiTxDbm = "tx_dbm";

static_assert(sizeof("req_cup") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("empty_raw") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("empty_cal") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("cup_min_g") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("cup_max_g") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("rem_max_s") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("noflow_ms") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("noflow_g") - 1 <= 15, "NVS key exceeds 15 chars");
static_assert(sizeof("tx_dbm") - 1 <= 15, "NVS key exceeds 15 chars");

// Hard safety sanity bounds
inline constexpr float kMinRemoteTargetWeightG = 4.0f;
inline constexpr float kMaxRemoteTargetWeightG = 30.0f;
inline constexpr float kMaxInterlockRangeG = 1.0f;
inline constexpr float kMinCupThresholdG = 5.0f;
inline constexpr float kMaxCupThresholdG = 950.0f;
inline constexpr uint16_t kMinRemoteMaxTimeS = 10;
inline constexpr uint16_t kMaxRemoteMaxTimeS = 60;
inline constexpr uint16_t kMinNoFlowTimeoutMs = 1500;
inline constexpr uint16_t kMaxNoFlowTimeoutMs = 10000;
inline constexpr float kMinNoFlowWeightG = 0.1f;
inline constexpr float kMaxNoFlowWeightG = 3.0f;
inline constexpr uint8_t kMinWifiTxPowerDbm = 8;
inline constexpr uint8_t kMaxWifiTxPowerDbm = 20;

struct RemoteSafetyConfig {
    bool require_cup_on_remote = false;    // Opt-in; Home Assistant can enforce cup weight by default
    bool empty_holder_calibrated = false;
    int32_t empty_holder_raw = 0;
    float cup_min_weight_g = 50.0f;        // e.g. 50-80g for dosing cup, 350-450g for portafilter
    float cup_max_weight_g = 600.0f;       // e.g. empty_vessel_weight + 15g to prevent double-grind
    uint16_t remote_max_grind_time_s = 25; // Stricter timeout for unattended remote grinds
    uint16_t no_flow_timeout_ms = 3000;    // 3s No-Flow Watchdog in PREDICTIVE phase
    float no_flow_min_weight_g = 0.5f;     // Must gain >= 0.5g before no_flow_timeout_ms
    uint8_t wifi_tx_power_dbm = 0;         // 0 = unrestricted stock default; 8..20 caps Wi-Fi TX power
};

enum class CupInterlockStatus : uint8_t {
    OK = 0,
    SCALE_UNSTABLE,
    NO_CUP_DETECTED,
    CUP_ALREADY_FULL
};

inline const char* cup_interlock_reason(CupInterlockStatus status) {
    switch (status) {
        case CupInterlockStatus::OK:
            return "ok";
        case CupInterlockStatus::SCALE_UNSTABLE:
            return "scale_unstable";
        case CupInterlockStatus::NO_CUP_DETECTED:
            return "no_cup_detected";
        case CupInterlockStatus::CUP_ALREADY_FULL:
            return "cup_already_full";
    }
    return "unknown";
}

inline bool validate_remote_target_weight(float target_weight_g) {
    return std::isfinite(target_weight_g) &&
           target_weight_g >= kMinRemoteTargetWeightG &&
           target_weight_g <= kMaxRemoteTargetWeightG;
}

inline bool validate_safety_config(const RemoteSafetyConfig& cfg) {
    if (!std::isfinite(cfg.cup_min_weight_g) || !std::isfinite(cfg.cup_max_weight_g) ||
        !std::isfinite(cfg.no_flow_min_weight_g)) {
        return false;
    }
    if (cfg.cup_min_weight_g < kMinCupThresholdG || cfg.cup_min_weight_g > kMaxCupThresholdG) {
        return false;
    }
    if (cfg.cup_max_weight_g <= cfg.cup_min_weight_g || cfg.cup_max_weight_g > kMaxCupThresholdG) {
        return false;
    }
    if (cfg.remote_max_grind_time_s < kMinRemoteMaxTimeS ||
        cfg.remote_max_grind_time_s > kMaxRemoteMaxTimeS) {
        return false;
    }
    if (cfg.no_flow_timeout_ms < kMinNoFlowTimeoutMs ||
        cfg.no_flow_timeout_ms > kMaxNoFlowTimeoutMs) {
        return false;
    }
    if (cfg.no_flow_min_weight_g < kMinNoFlowWeightG ||
        cfg.no_flow_min_weight_g > kMaxNoFlowWeightG) {
        return false;
    }
    if (cfg.wifi_tx_power_dbm != 0 &&
        (cfg.wifi_tx_power_dbm < kMinWifiTxPowerDbm || cfg.wifi_tx_power_dbm > kMaxWifiTxPowerDbm)) {
        return false;
    }
    return true;
}

inline float gross_weight_from_raw(int32_t current_raw, int32_t empty_raw, float cal_factor) {
    if (!std::isfinite(cal_factor) || std::fabs(cal_factor) < 1e-3f) {
        return 0.0f;
    }
    return static_cast<float>(current_raw - empty_raw) / cal_factor;
}

inline CupInterlockStatus evaluate_cup_interlock(float gross_weight_g,
                                                 float weight_range_g,
                                                 const RemoteSafetyConfig& cfg) {
    if (!std::isfinite(gross_weight_g) || !std::isfinite(weight_range_g) ||
        weight_range_g < 0.0f || weight_range_g > kMaxInterlockRangeG) {
        return CupInterlockStatus::SCALE_UNSTABLE;
    }
    if (gross_weight_g < cfg.cup_min_weight_g) {
        return CupInterlockStatus::NO_CUP_DETECTED;
    }
    if (gross_weight_g > cfg.cup_max_weight_g) {
        return CupInterlockStatus::CUP_ALREADY_FULL;
    }
    return CupInterlockStatus::OK;
}

inline bool check_no_flow_watchdog(uint32_t predictive_elapsed_ms,
                                   float delivered_weight_g,
                                   bool flow_start_confirmed,
                                   uint16_t no_flow_timeout_ms,
                                   float no_flow_min_weight_g) {
    if (no_flow_timeout_ms == 0) {
        return false;
    }
    if (flow_start_confirmed) {
        return false;
    }
    if (predictive_elapsed_ms < no_flow_timeout_ms) {
        return false;
    }
    return !std::isfinite(delivered_weight_g) || delivered_weight_g < no_flow_min_weight_g;
}

}  // namespace remote_safety
