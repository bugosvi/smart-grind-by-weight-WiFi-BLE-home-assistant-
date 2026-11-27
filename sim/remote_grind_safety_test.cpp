#include "controllers/remote_grind_safety.h"

#include <cassert>
#include <cmath>
#include <cstring>

using namespace remote_safety;

int main() {
    // 1. Target weight sanity bounds (4.0g .. 30.0g)
    assert(!validate_remote_target_weight(3.9f));
    assert(validate_remote_target_weight(4.0f));
    assert(validate_remote_target_weight(10.0f));
    assert(validate_remote_target_weight(18.5f));
    assert(validate_remote_target_weight(30.0f));
    assert(!validate_remote_target_weight(30.1f));
    assert(!validate_remote_target_weight(NAN));

    // 2. Default config must be valid (opt-in cup interlock, unrestricted TX power)
    RemoteSafetyConfig cup_cfg;
    assert(!cup_cfg.require_cup_on_remote);
    assert(cup_cfg.wifi_tx_power_dbm == 0);
    cup_cfg.cup_min_weight_g = 80.0f;
    cup_cfg.cup_max_weight_g = 115.0f;  // e.g. 100g cup + 15g margin
    assert(validate_safety_config(cup_cfg));

    // Valid optional TX power caps (8..20 dBm)
    RemoteSafetyConfig tx_cfg = cup_cfg;
    tx_cfg.wifi_tx_power_dbm = 11;
    assert(validate_safety_config(tx_cfg));
    tx_cfg.wifi_tx_power_dbm = 20;
    assert(validate_safety_config(tx_cfg));

    // Invalid configs must fail validation
    RemoteSafetyConfig bad_cfg = cup_cfg;
    bad_cfg.cup_max_weight_g = 80.0f;  // max <= min
    assert(!validate_safety_config(bad_cfg));

    bad_cfg = cup_cfg;
    bad_cfg.remote_max_grind_time_s = 5;  // below minimum
    assert(!validate_safety_config(bad_cfg));

    bad_cfg = cup_cfg;
    bad_cfg.wifi_tx_power_dbm = 5;  // below minimum non-zero cap
    assert(!validate_safety_config(bad_cfg));

    bad_cfg = cup_cfg;
    bad_cfg.wifi_tx_power_dbm = 25;  // above maximum
    assert(!validate_safety_config(bad_cfg));

    // No-flow timeout: 0 (disabled) is valid; non-zero must be within [1500, 10000]
    RemoteSafetyConfig noflow_cfg = cup_cfg;
    noflow_cfg.no_flow_timeout_ms = 0;
    assert(validate_safety_config(noflow_cfg));
    noflow_cfg.no_flow_timeout_ms = 1000;  // below minimum non-zero
    assert(!validate_safety_config(noflow_cfg));
    noflow_cfg.no_flow_timeout_ms = 12000; // above maximum
    assert(!validate_safety_config(noflow_cfg));

    // 3. Cup interlock: unstable scale rejected first
    assert(evaluate_cup_interlock(95.0f, 1.2f, cup_cfg) == CupInterlockStatus::SCALE_UNSTABLE);
    assert(std::strcmp(cup_interlock_reason(CupInterlockStatus::SCALE_UNSTABLE), "scale_unstable") == 0);

    // 4. Cup interlock: missing cup rejected
    assert(evaluate_cup_interlock(0.2f, 0.05f, cup_cfg) == CupInterlockStatus::NO_CUP_DETECTED);
    assert(evaluate_cup_interlock(79.9f, 0.10f, cup_cfg) == CupInterlockStatus::NO_CUP_DETECTED);
    assert(std::strcmp(cup_interlock_reason(CupInterlockStatus::NO_CUP_DETECTED), "no_cup_detected") == 0);

    // 5. Cup interlock: already-full cup rejected (prevents double-grind)
    assert(evaluate_cup_interlock(118.0f, 0.05f, cup_cfg) == CupInterlockStatus::CUP_ALREADY_FULL);
    assert(std::strcmp(cup_interlock_reason(CupInterlockStatus::CUP_ALREADY_FULL), "cup_already_full") == 0);

    // 6. Cup interlock: valid empty cup accepted
    assert(evaluate_cup_interlock(100.0f, 0.08f, cup_cfg) == CupInterlockStatus::OK);
    assert(std::strcmp(cup_interlock_reason(CupInterlockStatus::OK), "ok") == 0);

    // 7. Portafilter profile (e.g. 420g naked portafilter: 390g..435g)
    RemoteSafetyConfig pf_cfg;
    pf_cfg.cup_min_weight_g = 390.0f;
    pf_cfg.cup_max_weight_g = 435.0f;
    assert(validate_safety_config(pf_cfg));
    assert(evaluate_cup_interlock(100.0f, 0.05f, pf_cfg) == CupInterlockStatus::NO_CUP_DETECTED);
    assert(evaluate_cup_interlock(420.0f, 0.15f, pf_cfg) == CupInterlockStatus::OK);
    assert(evaluate_cup_interlock(438.0f, 0.15f, pf_cfg) == CupInterlockStatus::CUP_ALREADY_FULL);

    // 8. Raw ADC to gross weight conversion
    const float gross = gross_weight_from_raw(540000, 100000, 4400.0f);
    assert(std::fabs(gross - 100.0f) < 0.01f);
    assert(gross_weight_from_raw(540000, 100000, 0.0f) == 0.0f);

    // 9. No-Flow Watchdog (3000ms, < 0.5g without flow confirmation)
    assert(!check_no_flow_watchdog(2999, 0.1f, false, 3000, 0.5f));
    assert(check_no_flow_watchdog(3000, 0.1f, false, 3000, 0.5f));
    assert(check_no_flow_watchdog(3500, 0.49f, false, 3000, 0.5f));
    assert(!check_no_flow_watchdog(3500, 0.50f, false, 3000, 0.5f));
    assert(!check_no_flow_watchdog(4000, 0.2f, true, 3000, 0.5f));
    assert(!check_no_flow_watchdog(5000, 0.0f, false, 0, 0.5f));  // disabled

    return 0;
}
