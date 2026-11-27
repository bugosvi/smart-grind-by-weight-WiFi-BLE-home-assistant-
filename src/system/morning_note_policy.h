#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

//==============================================================================
// SCHEDULED MORNING NOTE POLICY & JSON CODEC
//==============================================================================
// Pure, hardware-independent scheduling, AMOLED burn-in bounds validation, and
// strict zero-allocation JSON parsing for the grinder's Morning Note overlay.
// Keep this header free of Arduino/ESP-IDF dependencies so it is verified
// directly by host unit tests in sim/.

namespace morning_note {

inline constexpr uint64_t kMinValidEpoch = 1700000000ULL;  // 2023-11-14 UTC
inline constexpr uint64_t kMaxWindowSeconds = 6ULL * 3600ULL;
inline constexpr uint16_t kMaxWindowMinutes = 360U;
// An IMMEDIATE note has no schedule window, so its total time on screen is
// bounded instead, by the same limit a scheduled window gets.
inline constexpr uint32_t kMaxImmediateDisplaySeconds = static_cast<uint32_t>(kMaxWindowSeconds);
inline constexpr uint16_t kMinutesPerDay = 1440U;
inline constexpr int16_t kMinTzOffsetMin = -720;
inline constexpr int16_t kMaxTzOffsetMin = 840;
inline constexpr uint16_t kDefaultPreviewDurationS = 10U;
inline constexpr uint16_t kMaxPreviewDurationS = 30U;

enum class ScheduleMode : uint8_t {
    IMMEDIATE = 0,
    ONE_SHOT_WINDOW = 1,
    DAILY_WINDOW = 2,
};

struct MorningNoteConfig {
    bool enabled = false;
    ScheduleMode mode = ScheduleMode::IMMEDIATE;
    uint64_t start_epoch = 0;     // UTC seconds for ONE_SHOT_WINDOW
    uint64_t end_epoch = 0;       // UTC seconds for ONE_SHOT_WINDOW
    uint16_t start_min = 390;     // 06:30 local for DAILY_WINDOW
    uint16_t end_min = 660;       // 11:00 local for DAILY_WINDOW
    int16_t tz_offset_min = 0;    // Local UTC offset in minutes (-720..+840)
    bool clear_on_grind = true;   // Auto-disarm after a completed grind
    uint32_t dismissed_day = 0;   // Window day ID dismissed for DAILY_WINDOW
    char headline[64] = "";
    char body[256] = "";
    char signature[64] = "";
    char theme[16] = "espresso";
};

struct EvaluationResult {
    bool is_due = false;
    bool expired_one_shot = false;
    uint32_t active_window_day = 0;
};

inline const char* mode_to_string(ScheduleMode mode) {
    switch (mode) {
        case ScheduleMode::IMMEDIATE:
            return "immediate";
        case ScheduleMode::ONE_SHOT_WINDOW:
            return "one_shot";
        case ScheduleMode::DAILY_WINDOW:
            return "daily";
    }
    return "immediate";
}

inline bool parse_mode_string(const char* value, ScheduleMode& out) {
    if (!value) return false;
    if (std::strcmp(value, "immediate") == 0) {
        out = ScheduleMode::IMMEDIATE;
        return true;
    }
    if (std::strcmp(value, "one_shot") == 0) {
        out = ScheduleMode::ONE_SHOT_WINDOW;
        return true;
    }
    if (std::strcmp(value, "daily") == 0) {
        out = ScheduleMode::DAILY_WINDOW;
        return true;
    }
    return false;
}

inline bool is_valid_theme(const char* theme) {
    if (!theme) return false;
    return std::strcmp(theme, "espresso") == 0 ||
           std::strcmp(theme, "sage") == 0 ||
           std::strcmp(theme, "crema") == 0 ||
           std::strcmp(theme, "noir") == 0;
}

inline uint16_t daily_window_span_minutes(uint16_t start_min, uint16_t end_min) {
    if (start_min >= kMinutesPerDay || end_min >= kMinutesPerDay || start_min == end_min) {
        return 0;
    }
    if (end_min > start_min) {
        return static_cast<uint16_t>(end_min - start_min);
    }
    return static_cast<uint16_t>((kMinutesPerDay - start_min) + end_min);
}

inline bool validate_morning_note_config(const MorningNoteConfig& cfg) {
    if (!is_valid_theme(cfg.theme)) {
        return false;
    }
    if (cfg.tz_offset_min < kMinTzOffsetMin || cfg.tz_offset_min > kMaxTzOffsetMin) {
        return false;
    }
    if (cfg.mode == ScheduleMode::ONE_SHOT_WINDOW) {
        if (cfg.enabled) {
            if (cfg.start_epoch < kMinValidEpoch || cfg.end_epoch <= cfg.start_epoch) {
                return false;
            }
            const uint64_t span = cfg.end_epoch - cfg.start_epoch;
            if (span < 60ULL || span > kMaxWindowSeconds) {
                return false;
            }
        } else if (cfg.end_epoch != 0 || cfg.start_epoch != 0) {
            if (cfg.end_epoch <= cfg.start_epoch ||
                (cfg.end_epoch - cfg.start_epoch) > kMaxWindowSeconds) {
                return false;
            }
        }
    } else if (cfg.mode == ScheduleMode::DAILY_WINDOW) {
        const uint16_t span_min = daily_window_span_minutes(cfg.start_min, cfg.end_min);
        if (span_min < 1U || span_min > kMaxWindowMinutes) {
            return false;
        }
    }
    return true;
}

inline EvaluationResult evaluate_morning_note_due(const MorningNoteConfig& cfg,
                                                  uint64_t now_epoch,
                                                  bool has_valid_clock,
                                                  bool has_image) {
    EvaluationResult result{};
    if (!cfg.enabled || !has_image || !validate_morning_note_config(cfg)) {
        return result;
    }

    switch (cfg.mode) {
        case ScheduleMode::IMMEDIATE:
            result.is_due = true;
            return result;

        case ScheduleMode::ONE_SHOT_WINDOW:
            if (!has_valid_clock || now_epoch < kMinValidEpoch) {
                return result;
            }
            if (now_epoch >= cfg.end_epoch) {
                result.expired_one_shot = true;
                return result;
            }
            if (now_epoch >= cfg.start_epoch) {
                result.is_due = true;
            }
            return result;

        case ScheduleMode::DAILY_WINDOW: {
            if (!has_valid_clock || now_epoch < kMinValidEpoch) {
                return result;
            }
            const int64_t local_epoch =
                static_cast<int64_t>(now_epoch) + static_cast<int64_t>(cfg.tz_offset_min) * 60LL;
            if (local_epoch < 0) {
                return result;
            }
            const uint32_t local_day = static_cast<uint32_t>(local_epoch / 86400LL);
            const uint16_t minute_of_day =
                static_cast<uint16_t>((local_epoch % 86400LL) / 60LL);

            bool in_window = false;
            uint32_t window_day = local_day;
            if (cfg.start_min < cfg.end_min) {
                in_window = (minute_of_day >= cfg.start_min && minute_of_day < cfg.end_min);
            } else {
                // Overnight window (e.g. 22:00..02:00)
                if (minute_of_day >= cfg.start_min) {
                    in_window = true;
                    window_day = local_day;
                } else if (minute_of_day < cfg.end_min) {
                    in_window = true;
                    window_day = local_day > 0 ? (local_day - 1U) : 0U;
                }
            }

            if (in_window) {
                result.active_window_day = window_day;
                if (cfg.dismissed_day == 0 || cfg.dismissed_day != window_day) {
                    result.is_due = true;
                }
            }
            return result;
        }
    }
    return result;
}

// Mutates cfg when a coffee grind completes while clear_on_grind is enabled.
// Returns true if cfg changed and should be persisted.
inline bool apply_grind_completion(MorningNoteConfig& cfg,
                                   uint64_t now_epoch,
                                   bool has_valid_clock,
                                   bool has_image) {
    if (!cfg.enabled || !cfg.clear_on_grind) {
        return false;
    }
    const EvaluationResult eval = evaluate_morning_note_due(cfg, now_epoch, has_valid_clock, has_image);
    if (!eval.is_due) {
        return false;
    }
    if (cfg.mode == ScheduleMode::DAILY_WINDOW) {
        if (eval.active_window_day != 0 && cfg.dismissed_day != eval.active_window_day) {
            cfg.dismissed_day = eval.active_window_day;
            return true;
        }
        return false;
    }
    cfg.enabled = false;
    return true;
}

// Disarms an IMMEDIATE note once it has spent its on-screen budget, so a note
// nobody clears (no grind, clear_on_grind off, owner away) cannot keep a static
// frame on the AMOLED indefinitely. Scheduled modes are bounded by their window.
// Returns true if cfg changed and should be persisted.
inline bool apply_immediate_display_limit(MorningNoteConfig& cfg, uint32_t displayed_seconds) {
    if (!cfg.enabled || cfg.mode != ScheduleMode::IMMEDIATE ||
        displayed_seconds < kMaxImmediateDisplaySeconds) {
        return false;
    }
    cfg.enabled = false;
    return true;
}

// Safe JSON string escaper into a fixed caller buffer (preserves valid UTF-8).
inline size_t json_escape_into(const char* src, char* dst, size_t dst_size) {
    if (!dst || dst_size == 0) return 0;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }
    size_t out = 0;
    auto push = [&](char c) -> bool {
        if (out + 1 >= dst_size) return false;
        dst[out++] = c;
        return true;
    };
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(src); *p != '\0'; ++p) {
        const unsigned char ch = *p;
        if (ch == '"' || ch == '\\') {
            if (out + 2 >= dst_size) break;
            dst[out++] = '\\';
            dst[out++] = static_cast<char>(ch);
        } else if (ch == '\n') {
            if (out + 2 >= dst_size) break;
            dst[out++] = '\\';
            dst[out++] = 'n';
        } else if (ch == '\r') {
            if (out + 2 >= dst_size) break;
            dst[out++] = '\\';
            dst[out++] = 'r';
        } else if (ch == '\t') {
            if (out + 2 >= dst_size) break;
            dst[out++] = '\\';
            dst[out++] = 't';
        } else if (ch >= 0x20U) {
            if (!push(static_cast<char>(ch))) break;
        }
    }
    dst[out] = '\0';
    return out;
}

namespace detail {

inline void skip_ws(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        ++p;
    }
}

inline bool parse_bool(const char*& p, bool& out) {
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

inline bool parse_uint64(const char*& p, uint64_t max_value, uint64_t& out) {
    if (*p < '0' || *p > '9') return false;
    uint64_t parsed = 0;
    while (*p >= '0' && *p <= '9') {
        const uint8_t digit = static_cast<uint8_t>(*p - '0');
        if (parsed > (max_value - digit) / 10ULL) return false;
        parsed = parsed * 10ULL + digit;
        ++p;
    }
    out = parsed;
    return true;
}

inline bool parse_int32(const char*& p, int32_t min_value, int32_t max_value, int32_t& out) {
    bool negative = false;
    if (*p == '-') {
        negative = true;
        ++p;
    }
    if (*p < '0' || *p > '9') return false;
    int64_t parsed = 0;
    while (*p >= '0' && *p <= '9') {
        parsed = parsed * 10LL + static_cast<int64_t>(*p - '0');
        if (parsed > 2147483648LL) return false;
        ++p;
    }
    if (negative) parsed = -parsed;
    if (parsed < min_value || parsed > max_value) return false;
    out = static_cast<int32_t>(parsed);
    return true;
}

inline int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

inline bool parse_hex4(const char*& p, uint16_t& out) {
    uint16_t value = 0;
    for (int i = 0; i < 4; ++i) {
        const int d = hex_digit(p[i]);
        if (d < 0) return false;
        value = static_cast<uint16_t>((value << 4) | d);
    }
    p += 4;
    out = value;
    return true;
}

inline bool append_utf8_codepoint(uint32_t cp, char* dst, size_t dst_size, size_t& len) {
    if (cp <= 0x7FU) {
        if (len + 1 >= dst_size) return false;
        dst[len++] = static_cast<char>(cp);
    } else if (cp <= 0x7FFU) {
        if (len + 2 >= dst_size) return false;
        dst[len++] = static_cast<char>(0xC0U | (cp >> 6));
        dst[len++] = static_cast<char>(0x80U | (cp & 0x3FU));
    } else if (cp <= 0xFFFFU) {
        if (len + 3 >= dst_size) return false;
        dst[len++] = static_cast<char>(0xE0U | (cp >> 12));
        dst[len++] = static_cast<char>(0x80U | ((cp >> 6) & 0x3FU));
        dst[len++] = static_cast<char>(0x80U | (cp & 0x3FU));
    } else if (cp <= 0x10FFFFU) {
        if (len + 4 >= dst_size) return false;
        dst[len++] = static_cast<char>(0xF0U | (cp >> 18));
        dst[len++] = static_cast<char>(0x80U | ((cp >> 12) & 0x3FU));
        dst[len++] = static_cast<char>(0x80U | ((cp >> 6) & 0x3FU));
        dst[len++] = static_cast<char>(0x80U | (cp & 0x3FU));
    } else {
        return false;
    }
    return true;
}

// Parses a JSON string value into dst (supports UTF-8 & \uXXXX including surrogate pairs).
// Rejects malformed escapes or strings exceeding dst_size - 1 bytes.
inline bool parse_json_string(const char*& p, char* dst, size_t dst_size) {
    if (!dst || dst_size == 0 || *p != '"') return false;
    ++p;
    size_t len = 0;
    while (*p != '\0' && *p != '"') {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (ch < 0x20U) return false;
        if (ch == '\\') {
            ++p;
            char escaped = '\0';
            switch (*p) {
                case '"': escaped = '"'; ++p; break;
                case '\\': escaped = '\\'; ++p; break;
                case '/': escaped = '/'; ++p; break;
                case 'n': escaped = '\n'; ++p; break;
                case 'r': escaped = '\r'; ++p; break;
                case 't': escaped = '\t'; ++p; break;
                case 'b': escaped = '\b'; ++p; break;
                case 'f': escaped = '\f'; ++p; break;
                case 'u': {
                    ++p;
                    uint16_t unit = 0;
                    if (!parse_hex4(p, unit)) return false;
                    uint32_t cp = unit;
                    if (unit >= 0xD800U && unit <= 0xDBFFU) {
                        if (p[0] != '\\' || p[1] != 'u') return false;
                        p += 2;
                        uint16_t low = 0;
                        if (!parse_hex4(p, low) || low < 0xDC00U || low > 0xDFFFU) return false;
                        cp = 0x10000U + ((static_cast<uint32_t>(unit - 0xD800U) << 10) |
                                         static_cast<uint32_t>(low - 0xDC00U));
                    } else if (unit >= 0xDC00U && unit <= 0xDFFFU) {
                        return false;
                    }
                    if (!append_utf8_codepoint(cp, dst, dst_size, len)) return false;
                    continue;
                }
                default:
                    return false;
            }
            if (len + 1 >= dst_size) return false;
            dst[len++] = escaped;
        } else {
            if (len + 1 >= dst_size) return false;
            dst[len++] = static_cast<char>(ch);
            ++p;
        }
    }
    if (*p != '"') return false;
    ++p;
    dst[len] = '\0';
    return true;
}

}  // namespace detail

// Strict zero-allocation JSON parser for POST /api/v1/note and /note.json.
// Starts from `base` so partial patches preserve omitted fields, unless
// callers pass a fresh default MorningNoteConfig{}.
inline bool parse_morning_note_json(const char* json,
                                    const MorningNoteConfig& base,
                                    MorningNoteConfig& out,
                                    uint64_t* client_epoch_out = nullptr,
                                    bool* has_client_epoch_out = nullptr,
                                    bool allow_dismissed_day = false) {
    out = base;
    if (client_epoch_out) *client_epoch_out = 0;
    if (has_client_epoch_out) *has_client_epoch_out = false;
    if (!json) return false;

    const char* p = json;
    detail::skip_ws(p);
    if (*p != '{') return false;
    ++p;
    detail::skip_ws(p);
    if (*p == '}') return false;

    bool any_field = false;
    while (true) {
        detail::skip_ws(p);
        char key[32]{};
        if (!detail::parse_json_string(p, key, sizeof(key)) || key[0] == '\0') {
            return false;
        }
        detail::skip_ws(p);
        if (*p != ':') return false;
        ++p;
        detail::skip_ws(p);

        if (std::strcmp(key, "enabled") == 0) {
            if (!detail::parse_bool(p, out.enabled)) return false;
            any_field = true;
        } else if (std::strcmp(key, "mode") == 0) {
            char mode_str[20]{};
            if (!detail::parse_json_string(p, mode_str, sizeof(mode_str))) return false;
            if (!parse_mode_string(mode_str, out.mode)) return false;
            any_field = true;
        } else if (std::strcmp(key, "start_epoch") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, 4102444800ULL, v)) return false;
            out.start_epoch = v;
            any_field = true;
        } else if (std::strcmp(key, "end_epoch") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, 4102444800ULL, v)) return false;
            out.end_epoch = v;
            any_field = true;
        } else if (std::strcmp(key, "start_min") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, kMinutesPerDay - 1U, v)) return false;
            out.start_min = static_cast<uint16_t>(v);
            any_field = true;
        } else if (std::strcmp(key, "end_min") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, kMinutesPerDay - 1U, v)) return false;
            out.end_min = static_cast<uint16_t>(v);
            any_field = true;
        } else if (std::strcmp(key, "tz_offset_min") == 0) {
            int32_t v = 0;
            if (!detail::parse_int32(p, kMinTzOffsetMin, kMaxTzOffsetMin, v)) return false;
            out.tz_offset_min = static_cast<int16_t>(v);
            any_field = true;
        } else if (std::strcmp(key, "clear_on_grind") == 0) {
            if (!detail::parse_bool(p, out.clear_on_grind)) return false;
            any_field = true;
        } else if (std::strcmp(key, "headline") == 0) {
            if (!detail::parse_json_string(p, out.headline, sizeof(out.headline))) return false;
            any_field = true;
        } else if (std::strcmp(key, "body") == 0) {
            if (!detail::parse_json_string(p, out.body, sizeof(out.body))) return false;
            any_field = true;
        } else if (std::strcmp(key, "signature") == 0) {
            if (!detail::parse_json_string(p, out.signature, sizeof(out.signature))) return false;
            any_field = true;
        } else if (std::strcmp(key, "theme") == 0) {
            if (!detail::parse_json_string(p, out.theme, sizeof(out.theme))) return false;
            if (!is_valid_theme(out.theme)) return false;
            any_field = true;
        } else if (std::strcmp(key, "client_epoch") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, 4102444800ULL, v)) return false;
            if (client_epoch_out) *client_epoch_out = v;
            if (has_client_epoch_out) *has_client_epoch_out = true;
            any_field = true;
        } else if (allow_dismissed_day && std::strcmp(key, "dismissed_day") == 0) {
            uint64_t v = 0;
            if (!detail::parse_uint64(p, UINT32_MAX, v)) return false;
            out.dismissed_day = static_cast<uint32_t>(v);
            any_field = true;
        } else {
            return false;  // Reject unknown keys
        }

        detail::skip_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            ++p;
            detail::skip_ws(p);
            return *p == '\0' && any_field;
        }
        return false;
    }
}

}  // namespace morning_note
