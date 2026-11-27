#include <cassert>
#include <cstdio>
#include <cstring>

#include "system/morning_note_policy.h"

int main() {
    using namespace morning_note;

    // 1. Default config is valid but disabled
    {
        MorningNoteConfig cfg{};
        assert(validate_morning_note_config(cfg));
        const auto eval = evaluate_morning_note_due(cfg, 1750000000ULL, true, true);
        assert(!eval.is_due);
        assert(!eval.expired_one_shot);
    }

    // 2. IMMEDIATE mode works even without NTP clock sync, requires image
    {
        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::IMMEDIATE;
        assert(validate_morning_note_config(cfg));
        assert(!evaluate_morning_note_due(cfg, 0, false, false).is_due);
        assert(evaluate_morning_note_due(cfg, 0, false, true).is_due);
    }

    // 3. ONE_SHOT_WINDOW bounds & transitions (before / during / after / >6h rejected)
    {
        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::ONE_SHOT_WINDOW;
        cfg.start_epoch = 1764225000ULL;                // 06:30
        cfg.end_epoch = cfg.start_epoch + 4ULL * 3600U; // 10:30 (4h)
        assert(validate_morning_note_config(cfg));

        // No valid clock -> not due
        assert(!evaluate_morning_note_due(cfg, cfg.start_epoch + 60, false, true).is_due);

        // Before start -> not due, not expired
        auto before = evaluate_morning_note_due(cfg, cfg.start_epoch - 1, true, true);
        assert(!before.is_due && !before.expired_one_shot);

        // Inside window -> due
        auto inside = evaluate_morning_note_due(cfg, cfg.start_epoch + 120, true, true);
        assert(inside.is_due && !inside.expired_one_shot);

        // At/after end -> expired_one_shot
        auto after = evaluate_morning_note_due(cfg, cfg.end_epoch, true, true);
        assert(!after.is_due && after.expired_one_shot);

        // Exceeding 6-hour AMOLED burn-in limit -> invalid
        cfg.end_epoch = cfg.start_epoch + kMaxWindowSeconds + 1ULL;
        assert(!validate_morning_note_config(cfg));
    }

    // 4. DAILY_WINDOW daytime + timezone offset + clear_on_grind for today only
    {
        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::DAILY_WINDOW;
        cfg.start_min = 6 * 60 + 30;  // 06:30 local
        cfg.end_min = 11 * 60;        // 11:00 local (4.5h)
        cfg.tz_offset_min = 120;      // UTC+2 (Israel standard time)
        cfg.clear_on_grind = true;
        assert(validate_morning_note_config(cfg));

        // 2026-11-27 00:00:00 UTC -> 1795737600
        const uint64_t day_utc = 1795737600ULL;
        // 04:29 UTC == 06:29 local -> not due
        assert(!evaluate_morning_note_due(cfg, day_utc + 4 * 3600 + 29 * 60, true, true).is_due);
        // 04:30 UTC == 06:30 local -> due
        const uint64_t morning_utc = day_utc + 4 * 3600 + 30 * 60;
        const auto morning_eval = evaluate_morning_note_due(cfg, morning_utc, true, true);
        assert(morning_eval.is_due);

        // Grind completes at 07:15 local -> clears for today only
        assert(apply_grind_completion(cfg, morning_utc + 45 * 60, true, true));
        assert(cfg.enabled);  // Daily schedule remains armed for tomorrow
        assert(!evaluate_morning_note_due(cfg, morning_utc + 60 * 60, true, true).is_due);

        // Next morning 06:30 local -> due again!
        assert(evaluate_morning_note_due(cfg, morning_utc + 86400ULL, true, true).is_due);
    }

    // 5. DAILY_WINDOW overnight wrap (22:00..02:00) + dismiss spans midnight
    {
        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::DAILY_WINDOW;
        cfg.start_min = 22 * 60;  // 22:00
        cfg.end_min = 2 * 60;     // 02:00 (4h span)
        cfg.tz_offset_min = -300; // UTC-5
        cfg.clear_on_grind = true;
        assert(daily_window_span_minutes(cfg.start_min, cfg.end_min) == 240U);
        assert(validate_morning_note_config(cfg));

        const uint64_t day_local_midnight_utc = 1795737600ULL + 5 * 3600;
        const uint64_t at_23_30_local = day_local_midnight_utc + 23 * 3600 + 30 * 60;
        const uint64_t at_00_30_next = at_23_30_local + 3600;
        assert(evaluate_morning_note_due(cfg, at_23_30_local, true, true).is_due);
        assert(apply_grind_completion(cfg, at_23_30_local, true, true));
        // Still suppressed at 00:30 local (same overnight window!)
        assert(!evaluate_morning_note_due(cfg, at_00_30_next, true, true).is_due);
        // Active again next evening at 22:05 local
        assert(evaluate_morning_note_due(cfg, day_local_midnight_utc + 86400ULL + 22 * 3600 + 5 * 60, true, true).is_due);

        // Overnight window > 6h (22:00..05:00 = 7h) rejected
        cfg.end_min = 5 * 60;
        assert(!validate_morning_note_config(cfg));
    }

    // 6. ONE_SHOT_WINDOW clear_on_grind disables the note
    {
        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::ONE_SHOT_WINDOW;
        cfg.start_epoch = 1764225000ULL;
        cfg.end_epoch = cfg.start_epoch + 3600ULL;
        cfg.clear_on_grind = true;
        // Grind before window starts -> must NOT disarm tomorrow's note!
        assert(!apply_grind_completion(cfg, cfg.start_epoch - 600, true, true));
        assert(cfg.enabled);
        // Grind during window -> disarms note
        assert(apply_grind_completion(cfg, cfg.start_epoch + 300, true, true));
        assert(!cfg.enabled);
    }

    // 7. Strict JSON parsing with UTF-8 Hebrew + emojis + \uXXXX surrogate pairs
    {
        const char* payload =
            "{\n"
            "  \"enabled\": true,\n"
            "  \"mode\": \"one_shot\",\n"
            "  \"start_epoch\": 1764225000,\n"
            "  \"end_epoch\": 1764241200,\n"
            "  \"tz_offset_min\": 120,\n"
            "  \"clear_on_grind\": true,\n"
            "  \"headline\": \"בוקר טוב אפרת \\u2615\",\n"
            "  \"body\": \"היי אהובתי בוקר טוב,\\nבואי נכין קפה \\uD83D\\uDE0D\",\n"
            "  \"signature\": \"באהבה\",\n"
            "  \"theme\": \"espresso\",\n"
            "  \"client_epoch\": 1764190000\n"
            "}";
        MorningNoteConfig parsed{};
        uint64_t client_epoch = 0;
        bool has_client_epoch = false;
        assert(parse_morning_note_json(payload, MorningNoteConfig{}, parsed,
                                       &client_epoch, &has_client_epoch));
        assert(parsed.enabled);
        assert(parsed.mode == ScheduleMode::ONE_SHOT_WINDOW);
        assert(parsed.start_epoch == 1764225000ULL);
        assert(parsed.end_epoch == 1764241200ULL);
        assert(parsed.tz_offset_min == 120);
        assert(parsed.clear_on_grind);
        assert(std::strcmp(parsed.theme, "espresso") == 0);
        assert(std::strstr(parsed.headline, "בוקר טוב אפרת") != nullptr);
        assert(std::strstr(parsed.headline, "\xE2\x98\x95") != nullptr);  // ☕ UTF-8
        assert(std::strstr(parsed.body, "\n") != nullptr);
        assert(std::strstr(parsed.body, "\xF0\x9F\x98\x8D") != nullptr);  // 😍 UTF-8
        assert(has_client_epoch && client_epoch == 1764190000ULL);
        assert(validate_morning_note_config(parsed));

        // Round-trip escape check
        char escaped[512]{};
        json_escape_into(parsed.body, escaped, sizeof(escaped));
        assert(std::strstr(escaped, "\\n") != nullptr);
        assert(std::strstr(escaped, "בואי נכין קפה") != nullptr);
    }

    // 8. Malformed / unknown-key JSON rejection
    {
        MorningNoteConfig out{};
        assert(!parse_morning_note_json("{}", MorningNoteConfig{}, out));
        assert(!parse_morning_note_json("{\"unknown\":true}", MorningNoteConfig{}, out));
        assert(!parse_morning_note_json("{\"mode\":\"weekly\"}", MorningNoteConfig{}, out));
        assert(!parse_morning_note_json("{\"theme\":\"neon_purple\"}", MorningNoteConfig{}, out));
        assert(!parse_morning_note_json("{\"enabled\":true,}", MorningNoteConfig{}, out));
        assert(!parse_morning_note_json("{\"dismissed_day\":42}", MorningNoteConfig{}, out, nullptr, nullptr, false));
        assert(parse_morning_note_json("{\"dismissed_day\":42}", MorningNoteConfig{}, out, nullptr, nullptr, true));
        assert(out.dismissed_day == 42U);
    }

    // 9. IMMEDIATE notes are disarmed after the same screen time a window allows
    {
        assert(kMaxImmediateDisplaySeconds == kMaxWindowSeconds);

        MorningNoteConfig cfg{};
        cfg.enabled = true;
        cfg.mode = ScheduleMode::IMMEDIATE;
        cfg.clear_on_grind = false;  // Nothing else would ever clear this note
        assert(!apply_immediate_display_limit(cfg, 0));
        assert(!apply_immediate_display_limit(cfg, kMaxImmediateDisplaySeconds - 1U));
        assert(cfg.enabled);
        assert(evaluate_morning_note_due(cfg, 0, false, true).is_due);
        assert(apply_immediate_display_limit(cfg, kMaxImmediateDisplaySeconds));
        assert(!cfg.enabled);
        assert(!evaluate_morning_note_due(cfg, 0, false, true).is_due);
        assert(!apply_immediate_display_limit(cfg, kMaxImmediateDisplaySeconds));  // Already disarmed

        // Scheduled modes are bounded by their own window and are never disarmed here
        MorningNoteConfig daily{};
        daily.enabled = true;
        daily.mode = ScheduleMode::DAILY_WINDOW;
        assert(!apply_immediate_display_limit(daily, kMaxImmediateDisplaySeconds * 4U));
        assert(daily.enabled);
    }

    std::printf("morning_note_policy_test: PASS\n");
    return 0;
}
