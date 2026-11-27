#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
    EMPTY_HOLDER_UNCALIBRATED,
    SCALE_UNSTABLE,
    NO_CUP_DETECTED,
    CUP_ALREADY_FULL
};

inline const char* cup_interlock_reason(CupInterlockStatus status) {
    switch (status) {
        case CupInterlockStatus::OK:
            return "ok";
        case CupInterlockStatus::EMPTY_HOLDER_UNCALIBRATED:
            return "empty_holder_uncalibrated";
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
    if (cfg.no_flow_timeout_ms != 0 &&
        (cfg.no_flow_timeout_ms < kMinNoFlowTimeoutMs ||
         cfg.no_flow_timeout_ms > kMaxNoFlowTimeoutMs)) {
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

// Live interlock wrapper that verifies the empty holder reference was calibrated
// (tare_offset changes whenever a grind tares with a vessel on the fork, so gross
// weight cannot be derived without an explicit empty-holder reference).
inline CupInterlockStatus evaluate_calibrated_cup_interlock(float gross_weight_g,
                                                            float weight_range_g,
                                                            const RemoteSafetyConfig& cfg) {
    if (!std::isfinite(weight_range_g) || weight_range_g < 0.0f || weight_range_g > kMaxInterlockRangeG) {
        return CupInterlockStatus::SCALE_UNSTABLE;
    }
    if (!cfg.empty_holder_calibrated) {
        return CupInterlockStatus::EMPTY_HOLDER_UNCALIBRATED;
    }
    return evaluate_cup_interlock(gross_weight_g, weight_range_g, cfg);
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

struct SafetyPatch {
    bool has_require_cup_on_remote = false;
    bool require_cup_on_remote = false;
    bool has_cup_min_weight_g = false;
    float cup_min_weight_g = 0.0f;
    bool has_cup_max_weight_g = false;
    float cup_max_weight_g = 0.0f;
    bool has_remote_max_grind_time_s = false;
    uint16_t remote_max_grind_time_s = 0;
    bool has_no_flow_timeout_ms = false;
    uint16_t no_flow_timeout_ms = 0;
    bool has_no_flow_min_weight_g = false;
    float no_flow_min_weight_g = 0.0f;
    bool has_wifi_tx_power_dbm = false;
    uint8_t wifi_tx_power_dbm = 0;
    bool has_adaptive_grind_enabled = false;
    bool adaptive_grind_enabled = true;
    bool has_adaptive_coast_enabled = false;
    bool adaptive_coast_enabled = true;
    bool has_reset_adaptive_coast = false;
    bool reset_adaptive_coast = false;

    bool any_safety_config_field() const {
        return has_require_cup_on_remote || has_cup_min_weight_g || has_cup_max_weight_g ||
               has_remote_max_grind_time_s || has_no_flow_timeout_ms ||
               has_no_flow_min_weight_g || has_wifi_tx_power_dbm;
    }

    bool any_adaptive_field() const {
        return has_adaptive_grind_enabled || has_adaptive_coast_enabled || has_reset_adaptive_coast;
    }

    bool any_field() const {
        return any_safety_config_field() || any_adaptive_field();
    }

    void apply_to_config(RemoteSafetyConfig& cfg) const {
        if (has_require_cup_on_remote) cfg.require_cup_on_remote = require_cup_on_remote;
        if (has_cup_min_weight_g) cfg.cup_min_weight_g = cup_min_weight_g;
        if (has_cup_max_weight_g) cfg.cup_max_weight_g = cup_max_weight_g;
        if (has_remote_max_grind_time_s) cfg.remote_max_grind_time_s = remote_max_grind_time_s;
        if (has_no_flow_timeout_ms) cfg.no_flow_timeout_ms = no_flow_timeout_ms;
        if (has_no_flow_min_weight_g) cfg.no_flow_min_weight_g = no_flow_min_weight_g;
        if (has_wifi_tx_power_dbm) cfg.wifi_tx_power_dbm = wifi_tx_power_dbm;
    }
};

namespace detail {

inline void skip_json_ws(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        ++p;
    }
}

inline bool parse_json_bool_token(const char*& p, bool& out) {
    if (std::strncmp(p, "true", 4) == 0) {
        p += 4;
        out = true;
        return true;
    }
    if (std::strncmp(p, "false", 5) == 0) {
        p += 5;
        out = false;
        return true;
    }
    return false;
}

inline bool parse_json_float_token(const char*& p, float& out) {
    if (*p == '\0') return false;
    char* end = nullptr;
    const float v = std::strtof(p, &end);
    if (!end || end == p || !std::isfinite(v)) {
        return false;
    }
    p = end;
    out = v;
    return true;
}

inline bool parse_json_uint_token(const char*& p, uint32_t max_value, uint32_t& out) {
    if (*p < '0' || *p > '9') {
        return false;
    }
    uint64_t parsed = 0;
    while (*p >= '0' && *p <= '9') {
        parsed = parsed * 10ULL + static_cast<uint8_t>(*p - '0');
        if (parsed > max_value) {
            return false;
        }
        ++p;
    }
    out = static_cast<uint32_t>(parsed);
    return true;
}

}  // namespace detail

// Strict, zero-allocation JSON object parser for POST /api/v1/safety payloads.
// Rejects unknown keys, type mismatches, trailing commas, and empty objects.
inline bool parse_safety_json_patch(const char* json, SafetyPatch& out) {
    out = SafetyPatch{};
    if (!json) {
        return false;
    }
    const char* p = json;
    detail::skip_json_ws(p);
    if (*p != '{') {
        return false;
    }
    ++p;
    detail::skip_json_ws(p);
    if (*p == '}') {
        return false;  // Empty object provides no recognized safety fields
    }

    while (true) {
        detail::skip_json_ws(p);
        if (*p != '"') {
            return false;
        }
        ++p;
        char key[36]{};
        size_t key_len = 0;
        while (*p != '\0' && *p != '"') {
            if (static_cast<unsigned char>(*p) < 0x20 || *p == '\\' || key_len + 1 >= sizeof(key)) {
                return false;
            }
            key[key_len++] = *p++;
        }
        if (*p != '"' || key_len == 0) {
            return false;
        }
        ++p;
        detail::skip_json_ws(p);
        if (*p != ':') {
            return false;
        }
        ++p;
        detail::skip_json_ws(p);

        if (std::strcmp(key, "require_cup_on_remote") == 0) {
            if (!detail::parse_json_bool_token(p, out.require_cup_on_remote)) return false;
            out.has_require_cup_on_remote = true;
        } else if (std::strcmp(key, "cup_min_weight_g") == 0) {
            if (!detail::parse_json_float_token(p, out.cup_min_weight_g)) return false;
            out.has_cup_min_weight_g = true;
        } else if (std::strcmp(key, "cup_max_weight_g") == 0) {
            if (!detail::parse_json_float_token(p, out.cup_max_weight_g)) return false;
            out.has_cup_max_weight_g = true;
        } else if (std::strcmp(key, "remote_max_grind_time_s") == 0) {
            uint32_t v = 0;
            if (!detail::parse_json_uint_token(p, UINT16_MAX, v)) return false;
            out.remote_max_grind_time_s = static_cast<uint16_t>(v);
            out.has_remote_max_grind_time_s = true;
        } else if (std::strcmp(key, "no_flow_timeout_ms") == 0) {
            uint32_t v = 0;
            if (!detail::parse_json_uint_token(p, UINT16_MAX, v)) return false;
            out.no_flow_timeout_ms = static_cast<uint16_t>(v);
            out.has_no_flow_timeout_ms = true;
        } else if (std::strcmp(key, "no_flow_min_weight_g") == 0) {
            if (!detail::parse_json_float_token(p, out.no_flow_min_weight_g)) return false;
            out.has_no_flow_min_weight_g = true;
        } else if (std::strcmp(key, "wifi_tx_power_dbm") == 0) {
            uint32_t v = 0;
            if (!detail::parse_json_uint_token(p, 255U, v)) return false;
            out.wifi_tx_power_dbm = static_cast<uint8_t>(v);
            out.has_wifi_tx_power_dbm = true;
        } else if (std::strcmp(key, "adaptive_grind_enabled") == 0) {
            if (!detail::parse_json_bool_token(p, out.adaptive_grind_enabled)) return false;
            out.has_adaptive_grind_enabled = true;
        } else if (std::strcmp(key, "adaptive_coast_enabled") == 0) {
            if (!detail::parse_json_bool_token(p, out.adaptive_coast_enabled)) return false;
            out.has_adaptive_coast_enabled = true;
        } else if (std::strcmp(key, "reset_adaptive_coast") == 0) {
            if (!detail::parse_json_bool_token(p, out.reset_adaptive_coast)) return false;
            out.has_reset_adaptive_coast = true;
        } else {
            return false;  // Unknown field
        }

        detail::skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            ++p;
            detail::skip_json_ws(p);
            return *p == '\0' && out.any_field();
        }
        return false;
    }
}

}  // namespace remote_safety
