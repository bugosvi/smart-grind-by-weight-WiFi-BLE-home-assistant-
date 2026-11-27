#include "device_api.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <LittleFS.h>
#include <esp_random.h>

#include "../controllers/grind_controller.h"
#include "../controllers/profile_controller.h"
#include "../config/constants.h"
#include "../hardware/WeightSensor.h"
#include "../hardware/grinder.h"
#include "../hardware/hardware_manager.h"
#include "../system/screensaver_settings.h"
#include "../system/operation_interlock.h"
#include "device_web_server.h"
#include "gaggimate_status_client.h"
#include "network_manager.h"

DeviceApi device_api;

namespace {
bool contains_json_string(const char* json, const char* key, const char* value) {
    if (!json || !key || !value) return false;
    const String needle = "\"" + String(key) + "\"";
    const char* cursor = strstr(json, needle.c_str());
    if (!cursor) return false;
    cursor += needle.length();
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != '\"') return false;
    const size_t value_length = strlen(value);
    return strncmp(cursor, value, value_length) == 0 && cursor[value_length] == '\"';
}

bool extract_json_uint(const char* json, const char* key, uint32_t& value) {
    if (!json || !key) return false;
    const String needle = "\"" + String(key) + "\"";
    const char* cursor = strstr(json, needle.c_str());
    if (!cursor) return false;
    cursor += needle.length();
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor < '0' || *cursor > '9') return false;

    uint64_t parsed = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        parsed = parsed * 10U + static_cast<uint8_t>(*cursor - '0');
        if (parsed > 0xFFFFFFFFULL) return false;
        ++cursor;
    }
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor != ',' && *cursor != '}') return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool extract_json_float(const char* json, const char* key, float& value) {
    if (!json || !key) return false;
    const String needle = "\"" + String(key) + "\"";
    const char* cursor = strstr(json, needle.c_str());
    if (!cursor) return false;
    cursor += needle.length();
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    char* end = nullptr;
    const float parsed = strtof(cursor, &end);
    if (!end || end == cursor || !std::isfinite(parsed)) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end != ',' && *end != '}') return false;
    value = parsed;
    return true;
}

bool extract_json_bool(const char* json, const char* key, bool& value) {
    if (!json || !key) return false;
    const String needle = "\"" + String(key) + "\"";
    const char* cursor = strstr(json, needle.c_str());
    if (!cursor) return false;
    cursor += needle.length();
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (strncmp(cursor, "true", 4) == 0) {
        cursor += 4;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
        if (*cursor != ',' && *cursor != '}') return false;
        value = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0) {
        cursor += 5;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
        if (*cursor != ',' && *cursor != '}') return false;
        value = false;
        return true;
    }
    return false;
}

bool has_json_key(const char* json, const char* key) {
    if (!json || !key) return false;
    const String needle = "\"" + String(key) + "\"";
    const char* cursor = strstr(json, needle.c_str());
    if (!cursor) return false;
    cursor += needle.length();
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    return *cursor == ':';
}

bool parse_strict_float(const String& raw, float& out) {
    String trimmed = raw;
    trimmed.trim();
    if (trimmed.isEmpty()) return false;
    char* end = nullptr;
    const float parsed = strtof(trimmed.c_str(), &end);
    if (!end || end == trimmed.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
    out = parsed;
    return true;
}

bool parse_strict_uint32(const String& raw, uint32_t max_value, uint32_t& out) {
    String trimmed = raw;
    trimmed.trim();
    if (trimmed.isEmpty() || trimmed[0] == '-' || trimmed[0] == '+') return false;
    char* end = nullptr;
    const unsigned long parsed = strtoul(trimmed.c_str(), &end, 10);
    if (!end || end == trimmed.c_str() || *end != '\0' || parsed > max_value) return false;
    out = static_cast<uint32_t>(parsed);
    return true;
}

bool parse_strict_bool(const String& raw, bool& out) {
    String trimmed = raw;
    trimmed.trim();
    trimmed.toLowerCase();
    if (trimmed == "1" || trimmed == "true" || trimmed == "on") {
        out = true;
        return true;
    }
    if (trimmed == "0" || trimmed == "false" || trimmed == "off") {
        out = false;
        return true;
    }
    return false;
}

struct SafetyRequestBody {
    char json[512];
    size_t len;
    bool overflow;
};

inline float finite_or_zero(float value) {
    return std::isfinite(value) ? value : 0.0f;
}

bool websocket_origin_allowed(AsyncWebServerRequest* request) {
    if (!request || !request->hasHeader("Origin")) return true;
    const AsyncWebHeader* origin_header = request->getHeader("Origin");
    if (!origin_header) return false;
    return origin_header->value() == ("http://" + request->host());
}

bool valid_local_host(const String& host) {
    if (host.isEmpty() || host.length() > 63) return false;
    for (size_t index = 0; index < host.length(); ++index) {
        const char value = host[index];
        if (!isalnum(static_cast<unsigned char>(value)) && value != '.' && value != '-') {
            return false;
        }
    }
    return true;
}

const char* api_phase_name(const GrindController& controller) {
    switch (controller.get_phase()) {
        case GrindPhase::IDLE:
            return "IDLE";
        case GrindPhase::INITIALIZING:
        case GrindPhase::SETUP:
        case GrindPhase::TARING:
        case GrindPhase::TARE_CONFIRM:
            return "PREPARING";
        case GrindPhase::PRIME:
        case GrindPhase::PRIME_SETTLING:
        case GrindPhase::PURGE_CONFIRM:
            return "PRIMING";
        case GrindPhase::PREDICTIVE:
        case GrindPhase::PULSE_EXECUTE:
        case GrindPhase::TIME_ADDITIONAL_PULSE:
            return "GRINDING";
        case GrindPhase::TIME_GRINDING:
            return controller.is_grind_paused() ? "PAUSED" : "GRINDING";
        case GrindPhase::MANUAL_GRINDING:
            return "GRINDING";
        case GrindPhase::PULSE_DECISION:
        case GrindPhase::PULSE_SETTLING:
            return "COASTING";
        case GrindPhase::FINAL_SETTLING:
            return "FINAL_SETTLING";
        case GrindPhase::COMPLETED:
            return "COMPLETED";
        case GrindPhase::TIMEOUT:
            return "TIMEOUT";
    }
    return "IDLE";
}
}

void DeviceApi::init(AsyncWebServer* server, HardwareManager* hardware,
                     GrindController* grind_controller, ProfileController* profile_controller) {
    if (initialized_ || !server || !hardware || !grind_controller || !profile_controller) return;
    hardware_ = hardware;
    grind_controller_ = grind_controller;
    profile_controller_ = profile_controller;
    command_queue_ = xQueueCreate(8, sizeof(Command));
    settings_mutex_ = xSemaphoreCreateMutex();
    if (!command_queue_ || !settings_mutex_) {
        hardware_ = nullptr;
        grind_controller_ = nullptr;
        profile_controller_ = nullptr;
        return;
    }
    websocket_.onEvent([this](AsyncWebSocket* ws, AsyncWebSocketClient* client,
                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handle_event(ws, client, type, arg, data, len);
    });
    websocket_.handleHandshake(websocket_origin_allowed);
    server->addHandler(&websocket_);
    configure_settings_routes(server);
    // Avoid mistaking a previous boot's result for a newly queued request.
    next_settings_id_ = esp_random();
    load_safety_config();
    refresh_settings_cache();
    initialized_ = true;
}

bool form_bool(const String& value) {
    return value == "1" || value == "true" || value == "on";
}

void DeviceApi::update() {
    if (!initialized_) return;
    if (settings_cache_dirty_.exchange(false)) refresh_settings_cache();
    websocket_.cleanupClients(MAX_CLIENTS);
    const uint32_t now = millis();
    if (now - last_publish_ms_ < PUBLISH_INTERVAL_MS) return;
    last_publish_ms_ = now;

    bool any_client = false;
    for (size_t index = 0; index < MAX_CLIENTS; ++index) {
        if (client_ids_[index].load() != 0) {
            any_client = true;
            break;
        }
    }
    if (!any_client) return;

    const String message = build_state_message();
    for (size_t index = 0; index < MAX_CLIENTS; ++index) {
        auto& slot = client_ids_[index];
        const uint32_t id = slot.load();
        if (id == 0) {
            backpressure_skips_[index].store(0);
            continue;
        }
        if (!websocket_.hasClient(id)) {
            slot.store(0);
            backpressure_skips_[index].store(0);
            continue;
        }
        if (!websocket_.availableForWrite(id)) {
            uint8_t skipped = backpressure_skips_[index].load();
            if (skipped < MAX_CONSECUTIVE_BACKPRESSURE_SKIPS) ++skipped;
            backpressure_skips_[index].store(skipped);
            if (skipped >= MAX_CONSECUTIVE_BACKPRESSURE_SKIPS) {
                LOG_BLE("[WEB] Closing WebSocket client %lu after sustained backpressure\n",
                        static_cast<unsigned long>(id));
                websocket_.close(id, 1013, "client remained too slow");
                slot.store(0);
                backpressure_skips_[index].store(0);
            }
            continue;
        }
        backpressure_skips_[index].store(0);
        websocket_.text(id, message);
    }
}

bool DeviceApi::process_commands() {
    if (!initialized_) return false;
    if (applying_settings_id_) return true;
    bool settings_changed = false;
    Command command{};
    while (xQueueReceive(command_queue_, &command, 0) == pdTRUE) {
        // Keep the idle check and its action together with controller updates.
        const auto control_lock = grind_controller_->lock_control();
        switch (command.action) {
            case CommandAction::START: {
                if (device_web_server.is_ota_active() || device_web_server.is_ota_preparing()) {
                    send_ack(command, "start", false, "firmware update is active");
                    break;
                }
                if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                    send_ack(command, "start", false, "grinder is not idle");
                    break;
                }
                const auto profile = profile_controller_->snapshot();
                const GrindMode mode = profile.mode;
                const auto safety = get_safety_config();
                WeightSensor* sensor = hardware_->get_weight_sensor();
                if (mode == GrindMode::WEIGHT &&
                    (!sensor || sensor->has_hardware_fault() || !sensor->has_recent_sample())) {
                    send_ack(command, "start", false, "load cell is not ready");
                    break;
                }
                float target_weight = profile.profiles[profile.current_profile].weight;
                if (command.has_target_weight) {
                    if (!remote_safety::validate_remote_target_weight(command.target_weight)) {
                        send_ack(command, "start", false, "invalid_weight");
                        break;
                    }
                    target_weight = command.target_weight;
                } else if (mode == GrindMode::WEIGHT &&
                           !remote_safety::validate_remote_target_weight(target_weight)) {
                    send_ack(command, "start", false, "invalid_weight");
                    break;
                }
                // Clients may request stricter cup enforcement, never weaken device-level safety.
                const bool require_cup = safety.require_cup_on_remote ||
                                         (command.has_require_cup && command.require_cup);
                if (require_cup) {
                    float gross_g = 0.0f;
                    float range_g = 0.0f;
                    const auto interlock = evaluate_live_cup_interlock(&gross_g, &range_g);
                    if (interlock != remote_safety::CupInterlockStatus::OK) {
                        LOG_BLE("[SAFETY] Remote start rejected: %s (gross=%.1fg, range=%.2fg, min=%.1fg, max=%.1fg)\n",
                                remote_safety::cup_interlock_reason(interlock),
                                gross_g, range_g, safety.cup_min_weight_g, safety.cup_max_weight_g);
                        send_ack(command, "start", false, remote_safety::cup_interlock_reason(interlock));
                        break;
                    }
                }
                grind_controller_->set_grind_profile_id(profile.current_profile);
                const uint32_t target_time_ms = static_cast<uint32_t>(
                    profile.profiles[profile.current_profile].time_seconds * 1000.0f + 0.5f);
                RemoteGrindOptions remote_opts;
                remote_opts.is_remote = true;
                remote_opts.skip_purge_confirm = true;
                remote_opts.max_grind_time_s = safety.remote_max_grind_time_s;
                remote_opts.no_flow_timeout_ms = safety.no_flow_timeout_ms;
                remote_opts.no_flow_min_weight_g = safety.no_flow_min_weight_g;
                const bool started = grind_controller_->start_grind(target_weight, target_time_ms, mode, remote_opts);
                send_ack(command, "start", started, started ? "grind started" : "grind could not start");
                break;
            }
            case CommandAction::START_MANUAL: {
                if (device_web_server.is_ota_active() || device_web_server.is_ota_preparing()) {
                    send_ack(command, "start_manual", false, "firmware update is active");
                    break;
                }
                if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                    send_ack(command, "start_manual", false, "grinder is not idle");
                    break;
                }
                const auto safety = get_safety_config();
                const bool require_cup = safety.require_cup_on_remote ||
                                         (command.has_require_cup && command.require_cup);
                if (require_cup) {
                    float gross_g = 0.0f;
                    float range_g = 0.0f;
                    const auto interlock = evaluate_live_cup_interlock(&gross_g, &range_g);
                    if (interlock != remote_safety::CupInterlockStatus::OK) {
                        LOG_BLE("[SAFETY] Remote manual start rejected: %s (gross=%.1fg, range=%.2fg)\n",
                                remote_safety::cup_interlock_reason(interlock), gross_g, range_g);
                        send_ack(command, "start_manual", false, remote_safety::cup_interlock_reason(interlock));
                        break;
                    }
                }
                RemoteGrindOptions remote_opts;
                remote_opts.is_remote = true;
                remote_opts.max_grind_time_s = safety.remote_max_grind_time_s;
                const bool started = grind_controller_->start_grind(0.0f, 0, GrindMode::MANUAL, remote_opts);
                send_ack(command, "start_manual", started,
                         started ? "manual grind started" : "grind could not start");
                break;
            }
            case CommandAction::STOP:
                if (!grind_controller_->is_active()) {
                    send_ack(command, "stop", false, "grinder is not active");
                } else {
                    grind_controller_->stop_grind();
                    send_ack(command, "stop", true, "grind stopped");
                }
                break;
            case CommandAction::DISMISS:
                if (grind_controller_->get_phase() != GrindPhase::COMPLETED &&
                    grind_controller_->get_phase() != GrindPhase::TIMEOUT) {
                    send_ack(command, "dismiss", false, "nothing to dismiss");
                } else {
                    grind_controller_->return_to_idle();
                    const bool dismissed = grind_controller_->get_phase() == GrindPhase::IDLE;
                    send_ack(command, "dismiss", dismissed,
                             dismissed ? "result dismissed" : "history completion is pending");
                }
                break;
            case CommandAction::TARE: {
                WeightSensor* sensor = hardware_->get_weight_sensor();
                if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                    send_ack(command, "tare", false, "grinder is not idle");
                } else if (!sensor || sensor->has_hardware_fault() || sensor->get_sample_count() <= 0) {
                    send_ack(command, "tare", false, "load cell is not ready");
                } else if (sensor->is_tare_in_progress()) {
                    send_ack(command, "tare", false, "tare is already in progress");
                } else {
                    sensor->tareNoDelay();
                    send_ack(command, "tare", true, "tare started");
                }
                break;
            }
            case CommandAction::SELECT_PROFILE:
                if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                    send_ack(command, "select_profile", false, "grinder is not idle");
                    break;
                }
                profile_controller_->set_current_profile(command.profile_index);
                settings_changed = true;
                LOG_BLE("[WEB] Active profile changed to %s\n",
                        profile_controller_->get_current_name());
                send_ack(command, "select_profile", true, "profile selected");
                break;
            case CommandAction::SET_MODE:
                if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                    send_ack(command, "set_mode", false, "grinder is not idle");
                    break;
                }
                profile_controller_->set_grind_mode(
                    command.grind_mode == 1 ? GrindMode::TIME : GrindMode::WEIGHT);
                settings_changed = true;
                send_ack(command, "set_mode", true, "grind mode selected");
                break;
            case CommandAction::APPLY_SETTINGS: {
                settings_operation_token_ = operation_interlock().try_acquire();
                if (!settings_operation_token_) {
                    set_settings_result(command.request_id, "busy");
                    break;
                }
                applying_settings_id_ = command.request_id;
                settings_persisted_ = apply_settings(command.settings);
                // Even a failed multi-key save can change stored values. Return
                // now so UI runtime refresh finishes before any later command.
                refresh_settings_cache();
                return true;
            }
        }
    }
    if (settings_changed) refresh_settings_cache();
    return settings_changed;
}

uint32_t DeviceApi::reserve_settings_result() {
    xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    for (const auto& result : settings_results_) {
        if (strcmp(result.status, "pending") == 0) {
            xSemaphoreGive(settings_mutex_);
            return 0;
        }
    }
    if (++next_settings_id_ == 0) ++next_settings_id_;
    const uint32_t id = next_settings_id_;
    settings_results_[next_settings_result_] = {id, "pending"};
    next_settings_result_ = (next_settings_result_ + 1) % 4;
    xSemaphoreGive(settings_mutex_);
    return id;
}

void DeviceApi::set_settings_result(uint32_t id, const char* status) {
    xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    for (auto& result : settings_results_) {
        if (result.id == id) { result.status = status; break; }
    }
    xSemaphoreGive(settings_mutex_);
}

const char* DeviceApi::settings_result(uint32_t id) {
    const char* status = "unknown";
    xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    for (const auto& result : settings_results_) {
        if (id && result.id == id) { status = result.status; break; }
    }
    xSemaphoreGive(settings_mutex_);
    return status; // All statuses are static literals, not pointers into a slot.
}

void DeviceApi::complete_settings_application(bool runtime_applied) {
    if (!applying_settings_id_) return;
    // Keep other operations excluded through persistence, cache and UI refresh.
    set_settings_result(applying_settings_id_, settings_persisted_ && runtime_applied ? "saved" : "failed");
    operation_interlock().release(settings_operation_token_);
    settings_operation_token_ = 0;
    applying_settings_id_ = 0;
}

void DeviceApi::handle_event(AsyncWebSocket*, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (!client) return;
    if (type == WS_EVT_CONNECT) {
        add_client(client);
        return;
    }
    if (type == WS_EVT_DISCONNECT || type == WS_EVT_ERROR) {
        remove_client(client->id());
        return;
    }
    if (type != WS_EVT_DATA || !arg || !data) return;
    auto* info = static_cast<AwsFrameInfo*>(arg);
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT || len > 255) {
        client->close(1009, "single text frames only");
        return;
    }
    queue_command(client->id(), data, len);
}

void DeviceApi::add_client(AsyncWebSocketClient* client) {
    for (size_t index = 0; index < MAX_CLIENTS; ++index) {
        auto& slot = client_ids_[index];
        uint32_t empty = 0;
        if (slot.compare_exchange_strong(empty, client->id())) {
            backpressure_skips_[index].store(0);
            client->keepAlivePeriod(20);
            client->text(build_state_message());
            return;
        }
    }
    client->close(1013, "too many clients");
}

void DeviceApi::remove_client(uint32_t client_id) {
    for (size_t index = 0; index < MAX_CLIENTS; ++index) {
        auto& slot = client_ids_[index];
        uint32_t expected = client_id;
        if (slot.compare_exchange_strong(expected, 0)) {
            backpressure_skips_[index].store(0);
        }
    }
}

void DeviceApi::queue_command(uint32_t client_id, const uint8_t* data, size_t len) {
    char json[256];
    memcpy(json, data, len);
    json[len] = '\0';
    uint32_t request_id = 0;
    const bool has_request_id = extract_json_uint(json, "rid", request_id);
    if (!contains_json_string(json, "type", "command")) {
        send_ack(client_id, request_id, has_request_id, "unknown", false, "invalid message type");
        return;
    }

    Command command{};
    command.client_id = client_id;
    command.request_id = request_id;
    command.has_request_id = has_request_id;
    command.action = CommandAction::STOP;
    const char* action = nullptr;
    if (contains_json_string(json, "action", "start")) {
        action = "start";
        command.action = CommandAction::START;
        if (has_json_key(json, "target_weight")) {
            float parsed_weight = 0.0f;
            if (!extract_json_float(json, "target_weight", parsed_weight)) {
                send_ack(command, action, false, "invalid_target_weight");
                return;
            }
            command.has_target_weight = true;
            command.target_weight = parsed_weight;
        }
        if (has_json_key(json, "require_cup")) {
            bool parsed_require_cup = true;
            if (!extract_json_bool(json, "require_cup", parsed_require_cup)) {
                send_ack(command, action, false, "invalid_require_cup");
                return;
            }
            command.has_require_cup = true;
            command.require_cup = parsed_require_cup;
        }
    } else if (contains_json_string(json, "action", "start_manual")) {
        action = "start_manual";
        command.action = CommandAction::START_MANUAL;
        if (has_json_key(json, "require_cup")) {
            bool parsed_require_cup = true;
            if (!extract_json_bool(json, "require_cup", parsed_require_cup)) {
                send_ack(command, action, false, "invalid_require_cup");
                return;
            }
            command.has_require_cup = true;
            command.require_cup = parsed_require_cup;
        }
    } else if (contains_json_string(json, "action", "stop")) {
        action = "stop";
        command.action = CommandAction::STOP;
    } else if (contains_json_string(json, "action", "dismiss")) {
        action = "dismiss";
        command.action = CommandAction::DISMISS;
    } else if (contains_json_string(json, "action", "tare")) {
        action = "tare";
        command.action = CommandAction::TARE;
    } else if (contains_json_string(json, "action", "select_profile")) {
        action = "select_profile";
        command.action = CommandAction::SELECT_PROFILE;
        uint32_t profile = 0;
        if (!extract_json_uint(json, "profile", profile) || profile >= USER_PROFILE_COUNT) {
            send_ack(command, action, false, "invalid profile");
            return;
        }
        command.profile_index = static_cast<int>(profile);
    } else if (contains_json_string(json, "action", "set_mode")) {
        action = "set_mode";
        command.action = CommandAction::SET_MODE;
        if (contains_json_string(json, "mode", "weight")) {
            command.grind_mode = 0;
        } else if (contains_json_string(json, "mode", "time")) {
            command.grind_mode = 1;
        } else {
            send_ack(command, action, false, "invalid grind mode");
            return;
        }
    } else {
        send_ack(client_id, request_id, has_request_id, "unknown", false, "unsupported action");
        return;
    }

    if (xQueueSend(command_queue_, &command, 0) != pdTRUE) {
        send_ack(command, action, false, "command queue busy");
    }
}

void DeviceApi::configure_settings_routes(AsyncWebServer* server) {
    server->on(AsyncURIMatcher::exact("/api/v1/safety"), HTTP_GET, [this](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", safety_json());
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    server->on(
        AsyncURIMatcher::exact("/api/v1/safety"), HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!websocket_origin_allowed(request)) {
                request->send(403, "application/json", "{\"error\":\"Request origin is not allowed\"}");
                return;
            }
            if (grind_controller_ && grind_controller_->is_active()) {
                request->send(409, "application/json", "{\"error\":\"Grinder must be idle to change safety settings\"}");
                return;
            }

            const auto* raw_body = static_cast<const SafetyRequestBody*>(request->_tempObject);
            if (raw_body && raw_body->overflow) {
                request->send(400, "application/json", "{\"error\":\"Safety payload is too large\"}");
                return;
            }

            remote_safety::SafetyPatch patch{};
            if (raw_body && raw_body->len > 0) {
                if (!remote_safety::parse_safety_json_patch(raw_body->json, patch)) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON safety payload\"}");
                    return;
                }
            } else if (request->params() == 1 && request->getParam(0)->isPost() &&
                       request->getParam(0)->value().isEmpty()) {
                // Support curl -d '{"...": ...}' even if caller omitted Content-Type: application/json
                String candidate = request->getParam(0)->name();
                candidate.trim();
                if (!candidate.startsWith("{") ||
                    !remote_safety::parse_safety_json_patch(candidate.c_str(), patch)) {
                    request->send(400, "application/json", "{\"error\":\"Invalid safety payload\"}");
                    return;
                }
            } else {
                const size_t param_count = request->params();
                for (size_t i = 0; i < param_count; ++i) {
                    const auto* param = request->getParam(i);
                    if (!param || !param->isPost()) continue;
                    const String& key = param->name();
                    const String& val = param->value();
                    if (key == "require_cup_on_remote") {
                        if (!parse_strict_bool(val, patch.require_cup_on_remote)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid require_cup_on_remote\"}");
                            return;
                        }
                        patch.has_require_cup_on_remote = true;
                    } else if (key == "cup_min_weight_g") {
                        if (!parse_strict_float(val, patch.cup_min_weight_g)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid cup_min_weight_g\"}");
                            return;
                        }
                        patch.has_cup_min_weight_g = true;
                    } else if (key == "cup_max_weight_g") {
                        if (!parse_strict_float(val, patch.cup_max_weight_g)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid cup_max_weight_g\"}");
                            return;
                        }
                        patch.has_cup_max_weight_g = true;
                    } else if (key == "remote_max_grind_time_s") {
                        uint32_t parsed = 0;
                        if (!parse_strict_uint32(val, UINT16_MAX, parsed)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid remote_max_grind_time_s\"}");
                            return;
                        }
                        patch.remote_max_grind_time_s = static_cast<uint16_t>(parsed);
                        patch.has_remote_max_grind_time_s = true;
                    } else if (key == "no_flow_timeout_ms") {
                        uint32_t parsed = 0;
                        if (!parse_strict_uint32(val, UINT16_MAX, parsed)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid no_flow_timeout_ms\"}");
                            return;
                        }
                        patch.no_flow_timeout_ms = static_cast<uint16_t>(parsed);
                        patch.has_no_flow_timeout_ms = true;
                    } else if (key == "no_flow_min_weight_g") {
                        if (!parse_strict_float(val, patch.no_flow_min_weight_g)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid no_flow_min_weight_g\"}");
                            return;
                        }
                        patch.has_no_flow_min_weight_g = true;
                    } else if (key == "wifi_tx_power_dbm") {
                        uint32_t parsed = 0;
                        if (!parse_strict_uint32(val, 255U, parsed)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid wifi_tx_power_dbm\"}");
                            return;
                        }
                        patch.wifi_tx_power_dbm = static_cast<uint8_t>(parsed);
                        patch.has_wifi_tx_power_dbm = true;
                    } else if (key == "adaptive_grind_enabled") {
                        if (!parse_strict_bool(val, patch.adaptive_grind_enabled)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid adaptive_grind_enabled\"}");
                            return;
                        }
                        patch.has_adaptive_grind_enabled = true;
                    } else if (key == "adaptive_coast_enabled") {
                        if (!parse_strict_bool(val, patch.adaptive_coast_enabled)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid adaptive_coast_enabled\"}");
                            return;
                        }
                        patch.has_adaptive_coast_enabled = true;
                    } else if (key == "reset_adaptive_coast") {
                        if (!parse_strict_bool(val, patch.reset_adaptive_coast)) {
                            request->send(400, "application/json", "{\"error\":\"Invalid reset_adaptive_coast\"}");
                            return;
                        }
                        patch.has_reset_adaptive_coast = true;
                    } else {
                        request->send(400, "application/json", "{\"error\":\"Unknown safety field\"}");
                        return;
                    }
                }
                if (!patch.any_field()) {
                    request->send(400, "application/json", "{\"error\":\"No safety fields provided\"}");
                    return;
                }
            }

            if (patch.any_safety_config_field()) {
                remote_safety::RemoteSafetyConfig updated = get_safety_config();
                patch.apply_to_config(updated);
                if (updated.require_cup_on_remote && !updated.empty_holder_calibrated) {
                    request->send(400, "application/json", "{\"error\":\"Calibrate empty holder before enabling require_cup_on_remote\"}");
                    return;
                }
                if (!remote_safety::validate_safety_config(updated)) {
                    request->send(400, "application/json", "{\"error\":\"Invalid safety parameters\"}");
                    return;
                }
                if (!save_safety_config(updated)) {
                    request->send(500, "application/json", "{\"error\":\"Failed to persist safety settings\"}");
                    return;
                }
            }

            if (patch.any_adaptive_field() && grind_controller_) {
                const bool* algo_ptr = patch.has_adaptive_grind_enabled ? &patch.adaptive_grind_enabled : nullptr;
                const bool* coast_ptr = patch.has_adaptive_coast_enabled ? &patch.adaptive_coast_enabled : nullptr;
                const bool reset_learned = patch.has_reset_adaptive_coast && patch.reset_adaptive_coast;
                if ((algo_ptr || coast_ptr || reset_learned) &&
                    !grind_controller_->configure_adaptive_grind(algo_ptr, coast_ptr, reset_learned)) {
                    request->send(500, "application/json", "{\"error\":\"Failed to persist adaptive grind settings\"}");
                    return;
                }
            }

            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", safety_json());
            response->addHeader("Cache-Control", "no-store");
            request->send(response);
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!request || !data) return;
            auto* body = static_cast<SafetyRequestBody*>(request->_tempObject);
            if (index == 0 && !body) {
                body = static_cast<SafetyRequestBody*>(calloc(1, sizeof(SafetyRequestBody)));
                request->_tempObject = body;
            }
            if (!body) return;
            if (total >= sizeof(body->json) || index + len >= sizeof(body->json)) {
                body->overflow = true;
                return;
            }
            memcpy(body->json + index, data, len);
            body->len = index + len;
            body->json[body->len] = '\0';
        });
    server->on(AsyncURIMatcher::exact("/api/v1/safety/calibrate-empty"), HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!websocket_origin_allowed(request)) {
            request->send(403, "application/json", "{\"error\":\"Request origin is not allowed\"}");
            return;
        }
        int32_t empty_raw = 0;
        {
            const auto control_lock = grind_controller_->lock_control();
            if (grind_controller_->get_phase() != GrindPhase::IDLE) {
                request->send(409, "application/json", "{\"error\":\"Grinder must be idle to calibrate empty holder\"}");
                return;
            }
            WeightSensor* sensor = hardware_->get_weight_sensor();
            if (!sensor || sensor->has_hardware_fault() || !sensor->has_recent_sample() ||
                !sensor->has_valid_tare() || sensor->is_tare_in_progress()) {
                request->send(503, "application/json", "{\"error\":\"Load cell is not ready\"}");
                return;
            }
            const float range_g = sensor->get_weight_range(500);
            if (!std::isfinite(range_g) || range_g > remote_safety::kMaxInterlockRangeG) {
                request->send(409, "application/json", "{\"error\":\"scale_unstable\"}");
                return;
            }
            empty_raw = sensor->get_raw_adc_smoothed(500);
        }
        remote_safety::RemoteSafetyConfig updated = get_safety_config();
        updated.empty_holder_raw = empty_raw;
        updated.empty_holder_calibrated = true;
        if (!save_safety_config(updated)) {
            request->send(500, "application/json", "{\"error\":\"Failed to save empty holder calibration\"}");
            return;
        }
        LOG_BLE("[SAFETY] Calibrated empty_holder_raw=%ld\n", static_cast<long>(updated.empty_holder_raw));
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", safety_json());
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    server->on(AsyncURIMatcher::exact("/api/v1/settings/result"), HTTP_GET, [this](AsyncWebServerRequest* request) {
        uint32_t id = 0;
        bool valid = request->hasParam("id");
        if (valid) {
            const String value = request->getParam("id")->value();
            valid = !value.isEmpty() && value.length() <= 10;
            uint64_t parsed = 0;
            for (size_t i = 0; valid && i < value.length(); ++i) {
                valid = value[i] >= '0' && value[i] <= '9';
                if (valid) parsed = parsed * 10 + (value[i] - '0');
            }
            valid = valid && parsed > 0 && parsed <= UINT32_MAX;
            if (valid) id = static_cast<uint32_t>(parsed);
        }
        if (!valid) {
            request->send(400, "application/json", "{\"error\":\"Invalid settings request ID\"}");
            return;
        }
        const char* status = settings_result(id);
        char body[96];
        snprintf(body, sizeof(body), "{\"request_id\":%lu,\"status\":\"%s\"}",
                 static_cast<unsigned long>(id), status);
        auto* response = request->beginResponse(strcmp(status, "unknown") == 0 ? 404 : 200,
                                                "application/json", body);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    server->on(AsyncURIMatcher::exact("/api/v1/profile"), HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!websocket_origin_allowed(request)) {
            request->send(403, "application/json", "{\"error\":\"Request origin is not allowed\"}");
            return;
        }
        queue_profile_selection(request);
    });
    server->on(AsyncURIMatcher::exact("/api/v1/settings"), HTTP_GET, [this](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", settings_json());
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    server->on(AsyncURIMatcher::exact("/api/v1/settings"), HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!websocket_origin_allowed(request)) {
            request->send(403, "application/json", "{\"error\":\"Request origin is not allowed\"}");
            return;
        }
        queue_settings_update(request);
    });
}

bool DeviceApi::queue_profile_selection(AsyncWebServerRequest* request) {
    if (!request->hasParam("profile", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing profile\"}");
        return false;
    }
    const String value = request->getParam("profile", true)->value();
    if (value != "0" && value != "1" && value != "2") {
        request->send(400, "application/json", "{\"error\":\"Profile must be 0, 1 or 2\"}");
        return false;
    }
    if (grind_controller_->get_phase() != GrindPhase::IDLE) {
        request->send(409, "application/json", "{\"error\":\"Profiles can only be changed while the grinder is idle\"}");
        return false;
    }

    Command command{};
    command.action = CommandAction::SELECT_PROFILE;
    command.profile_index = value.toInt();
    if (xQueueSend(command_queue_, &command, 0) != pdTRUE) {
        request->send(503, "application/json", "{\"error\":\"Command queue is busy; try again\"}");
        return false;
    }
    request->send(202, "application/json", "{\"accepted\":true}");
    return true;
}

bool DeviceApi::queue_settings_update(AsyncWebServerRequest* request) {
    static const char* required[] = {
        "current_profile", "grind_mode", "weight0", "weight1", "weight2",
        "time0", "time1", "time2", "auto_start", "auto_return", "purge_mode",
        "purge_amount_g", "freshness_hours", "coast_ratio", "logging_enabled",
        "swipe_enabled", "brightness_percent", "screensaver_brightness_percent",
        "screensaver_startup", "screensaver_sleep", "screensaver_idle_timeout_s",
        "screensaver_startup_timeout_s",
        "screensaver_style", "bluetooth_startup"
    };
    for (const char* field : required) {
        if (!request->hasParam(field, true)) {
            request->send(400, "application/json", String("{\"error\":\"Missing field: ") + field + "\"}");
            return false;
        }
    }

    auto value = [request](const char* name) { return request->getParam(name, true)->value(); };
    DeviceSettingsUpdate settings{};
    settings.current_profile = value("current_profile").toInt();
    settings.grind_mode = value("grind_mode").toInt();
    for (int i = 0; i < 3; ++i) {
        settings.profile_weights[i] = value((String("weight") + i).c_str()).toFloat();
        settings.profile_times[i] = value((String("time") + i).c_str()).toFloat();
    }
    settings.auto_start = form_bool(value("auto_start"));
    if (request->hasParam("auto_start_threshold_g", true)) {
        settings.auto_start_threshold_g = value("auto_start_threshold_g").toFloat();
    } else {
        Preferences auto_preferences;
        if (auto_preferences.begin("autogrind", true)) {
            settings.auto_start_threshold_g = auto_preferences.getFloat(
                "start_delta_g", USER_AUTO_GRIND_TRIGGER_DELTA_G);
            auto_preferences.end();
        }
    }
    settings.auto_return = form_bool(value("auto_return"));
    settings.purge_mode = value("purge_mode").toInt();
    settings.purge_amount_g = value("purge_amount_g").toFloat();
    settings.freshness_hours = value("freshness_hours").toFloat();
    settings.coast_ratio = value("coast_ratio").toFloat();
    settings.motor_latency_ms = request->hasParam("motor_latency_ms", true)
                                    ? value("motor_latency_ms").toFloat()
                                    : grind_controller_->get_motor_response_latency();
    settings.logging_enabled = form_bool(value("logging_enabled"));
    settings.swipe_enabled = form_bool(value("swipe_enabled"));
    settings.brightness_percent = value("brightness_percent").toInt();
    settings.screensaver_brightness_percent = value("screensaver_brightness_percent").toInt();
    settings.screensaver_startup = form_bool(value("screensaver_startup"));
    settings.screensaver_sleep = form_bool(value("screensaver_sleep"));
    const long screensaver_idle_timeout_s = value("screensaver_idle_timeout_s").toInt();
    const long screensaver_startup_timeout_s = value("screensaver_startup_timeout_s").toInt();
    // Older web pages predate panel-off settings. Preserve the stored values
    // rather than rejecting the whole form or silently resetting the option.
    settings.has_display_off_enabled = request->hasParam("display_off_enabled", true);
    settings.has_display_off_delay_s = request->hasParam("display_off_delay_s", true);
    const long display_off_delay_s = settings.has_display_off_delay_s
                                         ? value("display_off_delay_s").toInt()
                                         : settings.display_off_delay_s;
    settings.screensaver_idle_timeout_s = static_cast<uint16_t>(screensaver_idle_timeout_s);
    settings.screensaver_startup_timeout_s = static_cast<uint8_t>(screensaver_startup_timeout_s);
    settings.display_off_enabled = settings.has_display_off_enabled
                                       ? form_bool(value("display_off_enabled"))
                                       : settings.display_off_enabled;
    settings.display_off_delay_s = static_cast<uint16_t>(display_off_delay_s);
    const String screensaver_style = value("screensaver_style");
    strncpy(settings.screensaver_style, screensaver_style.c_str(), sizeof(settings.screensaver_style) - 1);
    const String gaggimate_host = request->hasParam("gaggimate_host", true)
                                      ? value("gaggimate_host")
                                      : gaggimate_status_client.configured_host();
    strncpy(settings.gaggimate_host, gaggimate_host.c_str(), sizeof(settings.gaggimate_host) - 1);
    bool grind_layout_valid = true;
    if (request->hasParam("grind_layout", true)) {
        const String layout_str = value("grind_layout");
        settings.has_grind_layout = true;
        if (layout_str == "arc" || layout_str == "0") {
            settings.grind_layout = 0;
        } else if (layout_str == "chart" || layout_str == "1") {
            settings.grind_layout = 1;
        } else if (layout_str == "portafilter" || layout_str == "2") {
            settings.grind_layout = 2;
        } else {
            grind_layout_valid = false;
        }
    }
    settings.bluetooth_startup = form_bool(value("bluetooth_startup"));

    bool valid = grind_layout_valid &&
                 settings.current_profile >= 0 && settings.current_profile < USER_PROFILE_COUNT &&
                 settings.grind_mode >= 0 && settings.grind_mode <= 1 &&
                 std::isfinite(settings.auto_start_threshold_g) &&
                 settings.auto_start_threshold_g >= USER_AUTO_GRIND_TRIGGER_MIN_G &&
                 settings.auto_start_threshold_g <= USER_AUTO_GRIND_TRIGGER_MAX_G &&
                 settings.purge_mode >= 0 && settings.purge_mode <= 1 &&
                 std::isfinite(settings.purge_amount_g) &&
                 settings.purge_amount_g >= GRIND_PURGE_AMOUNT_MIN_G &&
                 settings.purge_amount_g <= GRIND_PURGE_AMOUNT_MAX_G &&
                 std::isfinite(settings.freshness_hours) && settings.freshness_hours >= 0.5f &&
                 settings.freshness_hours <= 48.0f && std::isfinite(settings.coast_ratio) &&
                 settings.coast_ratio >= GRIND_LATENCY_TO_COAST_RATIO_MIN &&
                 settings.coast_ratio <= GRIND_LATENCY_TO_COAST_RATIO_MAX &&
                 std::isfinite(settings.motor_latency_ms) &&
                 settings.motor_latency_ms >= GRIND_AUTOTUNE_LATENCY_MIN_MS &&
                 settings.motor_latency_ms <= GRIND_AUTOTUNE_LATENCY_MAX_MS &&
                 std::fabs(settings.motor_latency_ms / GRIND_MOTOR_LATENCY_MANUAL_STEP_MS -
                           std::round(settings.motor_latency_ms /
                                      GRIND_MOTOR_LATENCY_MANUAL_STEP_MS)) < 0.001f &&
                 settings.brightness_percent >= HW_DISPLAY_MINIMAL_BRIGHTNESS_PERCENT &&
                 settings.brightness_percent <= 100 &&
                 settings.screensaver_brightness_percent >= HW_DISPLAY_MINIMAL_BRIGHTNESS_PERCENT &&
                 settings.screensaver_brightness_percent <= 100 &&
                 screensaver_idle_timeout_s >= 0 && screensaver_idle_timeout_s <= 65535 &&
                 screensaver_startup_timeout_s >= 0 && screensaver_startup_timeout_s <= 255 &&
                 display_off_delay_s >= 0 && display_off_delay_s <= 65535 &&
                 ScreensaverSettings::is_valid_idle_timeout(settings.screensaver_idle_timeout_s) &&
                 ScreensaverSettings::is_valid_startup_timeout(settings.screensaver_startup_timeout_s) &&
                 ScreensaverSettings::is_valid_display_off_delay(settings.display_off_delay_s) &&
                 (screensaver_style == "orbit" || screensaver_style == "minimal" ||
                  screensaver_style == "blank" ||
                  (screensaver_style == "custom" && LittleFS.exists(BLE_IMAGE_FILENAME)) ||
                  (screensaver_style == "gaggimate" && valid_local_host(gaggimate_host)));
    for (int i = 0; i < 3 && valid; ++i) {
        valid = std::isfinite(settings.profile_weights[i]) &&
                profile_controller_->is_weight_valid(settings.profile_weights[i]) &&
                std::isfinite(settings.profile_times[i]) &&
                profile_controller_->is_time_valid(settings.profile_times[i]);
    }
    if (!valid) {
        request->send(400, "application/json", "{\"error\":\"One or more settings are outside the supported range\"}");
        return false;
    }
    if (grind_controller_->is_active()) {
        request->send(409, "application/json", "{\"error\":\"Stop the grinder before changing settings\"}");
        return false;
    }

    Command command{};
    command.action = CommandAction::APPLY_SETTINGS;
    command.settings = settings;
    command.request_id = reserve_settings_result();
    if (!command.request_id) {
        request->send(409, "application/json", "{\"error\":\"A settings save is still pending\"}");
        return false;
    }
    if (xQueueSend(command_queue_, &command, 0) != pdTRUE) {
        set_settings_result(command.request_id, "failed");
        request->send(503, "application/json", "{\"error\":\"Settings queue is busy; try again\"}");
        return false;
    }
    request->send(202, "application/json", String("{\"accepted\":true,\"request_id\":") + command.request_id + "}");
    return true;
}

bool DeviceApi::apply_settings(const DeviceSettingsUpdate& update) {
    if (!hardware_ || !profile_controller_ || !grind_controller_ || grind_controller_->is_active()) return false;
    DeviceSettingsUpdate settings = update;
    // Resolve omissions when executing, after earlier queued saves have applied.
    if (!settings.has_display_off_enabled || !settings.has_display_off_delay_s) {
        const auto stored_timing = ScreensaverSettings::load_timing();
        if (!settings.has_display_off_enabled) settings.display_off_enabled = stored_timing.display_off_enabled;
        if (!settings.has_display_off_delay_s) settings.display_off_delay_s = stored_timing.display_off_delay_s;
    }
    Preferences* grinder = hardware_->get_preferences();
    if (!grinder) return false;
    bool saved = profile_controller_->apply_web_settings(
            settings.current_profile,
            settings.grind_mode == 0 ? GrindMode::WEIGHT : GrindMode::TIME,
            settings.profile_weights, settings.profile_times);

    saved = (grinder->putInt(GrindController::PREF_KEY_GRINDER_MODE, settings.purge_mode) == sizeof(int32_t)) && saved;
    saved = (grinder->putFloat(GrindController::PREF_KEY_GRINDER_AMOUNT_G, settings.purge_amount_g) == sizeof(float)) && saved;
    saved = (grinder->putFloat(GrindController::PREF_KEY_GRIND_FRESHNESS_HOURS, settings.freshness_hours) == sizeof(float)) && saved;
    if (settings.has_grind_layout) {
        saved = (grinder->putInt("grind_layout", settings.grind_layout) == sizeof(int32_t)) && saved;
    }
    saved = grind_controller_->save_coast_ratio(settings.coast_ratio) && saved;
    saved = grind_controller_->save_motor_latency(settings.motor_latency_ms) && saved;

    auto put_bool = [](const char* name_space, const char* key, bool value) {
        Preferences preferences;
        if (!preferences.begin(name_space, false)) return false;
        const bool stored = preferences.putBool(key, value) == sizeof(bool);
        preferences.end();
        return stored;
    };
    saved = put_bool("autogrind", "auto_start", settings.auto_start) && saved;
    saved = put_bool("autogrind", "auto_return", settings.auto_return) && saved;
    Preferences auto_preferences;
    if (auto_preferences.begin("autogrind", false)) {
        saved = (auto_preferences.putFloat("start_delta_g", settings.auto_start_threshold_g) == sizeof(float)) && saved;
        auto_preferences.end();
    } else {
        saved = false;
    }
    saved = put_bool("logging", "enabled", settings.logging_enabled) && saved;
    saved = put_bool("swipe", "enabled", settings.swipe_enabled) && saved;
    saved = put_bool("screensaver", "startup", settings.screensaver_startup) && saved;
    saved = put_bool("screensaver", "sleep", settings.screensaver_sleep) && saved;
    saved = put_bool("bluetooth", "startup", settings.bluetooth_startup) && saved;

    Preferences brightness;
    if (brightness.begin("brightness", false)) {
        const bool normal_saved = brightness.putFloat("normal", settings.brightness_percent / 100.0f) == sizeof(float);
        saved = normal_saved && saved;
        saved = (brightness.putFloat("screensaver", settings.screensaver_brightness_percent / 100.0f) == sizeof(float)) && saved;
        brightness.end();
        if (normal_saved && hardware_->get_display()) {
            hardware_->get_display()->set_brightness(settings.brightness_percent / 100.0f);
        }
    } else {
        saved = false;
    }
    if (!ScreensaverSettings::save_timing(settings.screensaver_idle_timeout_s,
                                          settings.screensaver_startup_timeout_s,
                                          settings.display_off_enabled,
                                          settings.display_off_delay_s)) {
        saved = false;
    }
    Preferences screensaver;
    if (screensaver.begin("screensaver", false)) {
        const bool style_saved = screensaver.putString("style", settings.screensaver_style) == strlen(settings.screensaver_style);
        saved = style_saved && saved;
        const String stored_style = screensaver.getString("style", "");
        screensaver.end();
        const bool style_read = stored_style == "minimal" || stored_style == "orbit" ||
                                stored_style == "blank" || stored_style == "custom" ||
                                stored_style == "gaggimate";
        if (style_read) {
            saved = gaggimate_status_client.configure(stored_style == "gaggimate",
                                                      settings.gaggimate_host) && saved;
        } else {
            // Keep the active client unchanged when stored style is unreadable.
            saved = false;
        }
    } else {
        saved = false;
    }
    LOG_BLE("[WEB] Grinder settings %s\n", saved ? "updated" : "partly saved; storage or runtime setup failed");
    return saved;
}

String DeviceApi::settings_json() {
    if (!settings_mutex_) return "{}";
    xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    const String copy = settings_json_cache_;
    xSemaphoreGive(settings_mutex_);
    return copy;
}

void DeviceApi::refresh_settings_cache() {
    if (!profile_controller_ || !hardware_ || !grind_controller_ || !settings_mutex_) return;
    const auto profile = profile_controller_->snapshot();
    auto read_bool = [](const char* name_space, const char* key, bool fallback) {
        Preferences preferences;
        if (!preferences.begin(name_space, true)) return fallback;
        const bool value = preferences.getBool(key, fallback);
        preferences.end();
        return value;
    };
    auto read_brightness = [](const char* key, float fallback) {
        Preferences preferences;
        if (!preferences.begin("brightness", true)) return fallback;
        const float value = preferences.getFloat(key, fallback);
        preferences.end();
        return value;
    };
    auto read_float = [](const char* name_space, const char* key, float fallback) {
        Preferences preferences;
        if (!preferences.begin(name_space, true)) return fallback;
        const float value = preferences.getFloat(key, fallback);
        preferences.end();
        return value;
    };
    Preferences* grinder = hardware_->get_preferences();
    const int purge_mode = grinder->getInt(GrindController::PREF_KEY_GRINDER_MODE, GRIND_PURGE_MODE_DEFAULT);
    const float purge_amount = grinder->getFloat(GrindController::PREF_KEY_GRINDER_AMOUNT_G, GRIND_PURGE_AMOUNT_DEFAULT_G);
    const float freshness = grinder->getFloat(GrindController::PREF_KEY_GRIND_FRESHNESS_HOURS, GRIND_FRESHNESS_DEFAULT_HOURS);
    const ScreensaverTimingSettings screensaver_timing = ScreensaverSettings::load_timing();
    Preferences screensaver_preferences;
    String screensaver_style = LittleFS.exists(BLE_IMAGE_FILENAME) ? "custom" : "minimal";
    if (screensaver_preferences.begin("screensaver", true)) {
        screensaver_style = screensaver_preferences.getString("style", screensaver_style);
        screensaver_preferences.end();
    }
    const String gaggimate_host = gaggimate_status_client.configured_host();
    const int saved_layout = grinder->getInt("grind_layout", 0);
    const char* grind_layout_name =
        saved_layout == 2 ? "portafilter" : saved_layout == 1 ? "chart" : "arc";

    char json[2048];
    snprintf(json, sizeof(json),
             "{\"api\":\"v1\",\"current_profile\":%d,\"grind_mode\":\"%s\","
             "\"profiles\":["
             "{\"id\":0,\"name\":\"%s\",\"weight\":%.2f,\"time\":%.2f},"
             "{\"id\":1,\"name\":\"%s\",\"weight\":%.2f,\"time\":%.2f},"
             "{\"id\":2,\"name\":\"%s\",\"weight\":%.2f,\"time\":%.2f}],"
             "\"automatic\":{\"start\":%s,\"threshold_g\":%.1f,\"return\":%s},"
             "\"purge\":{\"mode\":%d,\"amount_g\":%.2f,\"freshness_hours\":%.2f},"
             "\"coast_ratio\":%.2f,\"motor_latency_ms\":%.0f,"
             "\"logging_enabled\":%s,\"swipe_enabled\":%s,"
             "\"display\":{\"brightness\":%d,\"screensaver_brightness\":%d,"
             "\"screensaver_startup\":%s,\"screensaver_sleep\":%s,"
             "\"screensaver_idle_timeout_s\":%u,\"screensaver_startup_timeout_s\":%u,"
             "\"display_off_enabled\":%s,\"display_off_delay_s\":%u,"
             "\"screensaver_style\":\"%s\",\"has_custom_screensaver\":%s,"
             "\"gaggimate_host\":\"%s\",\"grind_layout\":\"%s\"},"
             "\"bluetooth_startup\":%s}",
             profile.current_profile,
             profile.mode == GrindMode::TIME ? "time" : "weight",
             profile.profiles[0].name, profile.profiles[0].weight, profile.profiles[0].time_seconds,
             profile.profiles[1].name, profile.profiles[1].weight, profile.profiles[1].time_seconds,
             profile.profiles[2].name, profile.profiles[2].weight, profile.profiles[2].time_seconds,
             read_bool("autogrind", "auto_start", false) ? "true" : "false",
             std::clamp(read_float("autogrind", "start_delta_g", USER_AUTO_GRIND_TRIGGER_DELTA_G),
                        USER_AUTO_GRIND_TRIGGER_MIN_G, USER_AUTO_GRIND_TRIGGER_MAX_G),
             read_bool("autogrind", "auto_return", false) ? "true" : "false",
             purge_mode, purge_amount, freshness, grind_controller_->get_coast_ratio(),
             grind_controller_->get_motor_response_latency(),
             read_bool("logging", "enabled", true) ? "true" : "false",
             read_bool("swipe", "enabled", false) ? "true" : "false",
             static_cast<int>(read_brightness("normal", USER_SCREEN_BRIGHTNESS_NORMAL) * 100.0f + 0.5f),
             static_cast<int>(read_brightness("screensaver", USER_SCREEN_BRIGHTNESS_DIMMED) * 100.0f + 0.5f),
             read_bool("screensaver", "startup", false) ? "true" : "false",
             read_bool("screensaver", "sleep", false) ? "true" : "false",
             screensaver_timing.idle_timeout_s, screensaver_timing.startup_timeout_s,
             screensaver_timing.display_off_enabled ? "true" : "false",
             screensaver_timing.display_off_delay_s,
             screensaver_style.c_str(), LittleFS.exists(BLE_IMAGE_FILENAME) ? "true" : "false",
             gaggimate_host.c_str(), grind_layout_name,
             read_bool("bluetooth", "startup", true) ? "true" : "false");
    xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    settings_json_cache_ = json;
    xSemaphoreGive(settings_mutex_);
}

void DeviceApi::send_ack(uint32_t client_id, uint32_t request_id, bool has_request_id,
                         const char* action, bool accepted, const char* reason) {
    if (!websocket_.hasClient(client_id) || !websocket_.availableForWrite(client_id)) return;
    char message[192];
    if (has_request_id) {
        snprintf(message, sizeof(message),
                 "{\"api\":\"v1\",\"type\":\"ack\",\"rid\":%lu,\"action\":\"%s\",\"accepted\":%s,\"reason\":\"%s\"}",
                 static_cast<unsigned long>(request_id), action,
                 accepted ? "true" : "false", reason);
    } else {
        snprintf(message, sizeof(message),
                 "{\"api\":\"v1\",\"type\":\"ack\",\"action\":\"%s\",\"accepted\":%s,\"reason\":\"%s\"}",
                 action, accepted ? "true" : "false", reason);
    }
    websocket_.text(client_id, message);
}

void DeviceApi::send_ack(const Command& command, const char* action, bool accepted,
                         const char* reason) {
    send_ack(command.client_id, command.request_id, command.has_request_id,
             action, accepted, reason);
}

void DeviceApi::load_safety_config() {
    remote_safety::RemoteSafetyConfig loaded{};
    Preferences prefs;
    if (prefs.begin(remote_safety::kNvsNamespace, true)) {
        loaded.require_cup_on_remote = prefs.getBool(remote_safety::kKeyRequireCup, loaded.require_cup_on_remote);
        loaded.empty_holder_calibrated = prefs.getBool(remote_safety::kKeyEmptyCal, loaded.empty_holder_calibrated);
        loaded.empty_holder_raw = prefs.getInt(remote_safety::kKeyEmptyRaw, loaded.empty_holder_raw);
        loaded.cup_min_weight_g = prefs.getFloat(remote_safety::kKeyCupMinG, loaded.cup_min_weight_g);
        loaded.cup_max_weight_g = prefs.getFloat(remote_safety::kKeyCupMaxG, loaded.cup_max_weight_g);
        loaded.remote_max_grind_time_s = prefs.getUShort(remote_safety::kKeyMaxTimeS, loaded.remote_max_grind_time_s);
        loaded.no_flow_timeout_ms = prefs.getUShort(remote_safety::kKeyNoFlowMs, loaded.no_flow_timeout_ms);
        loaded.no_flow_min_weight_g = prefs.getFloat(remote_safety::kKeyNoFlowG, loaded.no_flow_min_weight_g);
        loaded.wifi_tx_power_dbm = prefs.getUChar(remote_safety::kKeyWifiTxDbm, loaded.wifi_tx_power_dbm);
        prefs.end();
    }
    if (!remote_safety::validate_safety_config(loaded)) {
        loaded = remote_safety::RemoteSafetyConfig{};
    }
    if (settings_mutex_) xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    safety_config_ = loaded;
    if (settings_mutex_) xSemaphoreGive(settings_mutex_);
    network_manager.set_tx_power_dbm(loaded.wifi_tx_power_dbm);
}

bool DeviceApi::save_safety_config(const remote_safety::RemoteSafetyConfig& cfg) {
    if (!remote_safety::validate_safety_config(cfg)) return false;
    Preferences prefs;
    if (!prefs.begin(remote_safety::kNvsNamespace, false)) return false;
    bool ok = true;
    ok = ok && (prefs.putBool(remote_safety::kKeyRequireCup, cfg.require_cup_on_remote) > 0);
    ok = ok && (prefs.putBool(remote_safety::kKeyEmptyCal, cfg.empty_holder_calibrated) > 0);
    ok = ok && (prefs.putInt(remote_safety::kKeyEmptyRaw, cfg.empty_holder_raw) > 0);
    ok = ok && (prefs.putFloat(remote_safety::kKeyCupMinG, cfg.cup_min_weight_g) > 0);
    ok = ok && (prefs.putFloat(remote_safety::kKeyCupMaxG, cfg.cup_max_weight_g) > 0);
    ok = ok && (prefs.putUShort(remote_safety::kKeyMaxTimeS, cfg.remote_max_grind_time_s) > 0);
    ok = ok && (prefs.putUShort(remote_safety::kKeyNoFlowMs, cfg.no_flow_timeout_ms) > 0);
    ok = ok && (prefs.putFloat(remote_safety::kKeyNoFlowG, cfg.no_flow_min_weight_g) > 0);
    ok = ok && (prefs.putUChar(remote_safety::kKeyWifiTxDbm, cfg.wifi_tx_power_dbm) > 0);
    prefs.end();
    if (!ok) return false;

    if (settings_mutex_) xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    safety_config_ = cfg;
    if (settings_mutex_) xSemaphoreGive(settings_mutex_);
    network_manager.set_tx_power_dbm(cfg.wifi_tx_power_dbm);
    return true;
}

remote_safety::RemoteSafetyConfig DeviceApi::get_safety_config() const {
    if (settings_mutex_) xSemaphoreTake(settings_mutex_, portMAX_DELAY);
    const auto copy = safety_config_;
    if (settings_mutex_) xSemaphoreGive(settings_mutex_);
    return copy;
}

remote_safety::CupInterlockStatus DeviceApi::evaluate_live_cup_interlock(
    float* gross_weight_out, float* range_out) const {
    const auto cfg = get_safety_config();
    WeightSensor* sensor = hardware_ ? hardware_->get_weight_sensor() : nullptr;
    if (!sensor || sensor->has_hardware_fault() || !sensor->has_recent_sample() ||
        !sensor->has_valid_tare() || sensor->is_tare_in_progress()) {
        if (gross_weight_out) *gross_weight_out = 0.0f;
        if (range_out) *range_out = 999.0f;
        return remote_safety::CupInterlockStatus::SCALE_UNSTABLE;
    }
    const float range_g = sensor->get_weight_range(500);
    const float gross_g = cfg.empty_holder_calibrated
        ? remote_safety::gross_weight_from_raw(
              sensor->get_raw_adc_smoothed(500), cfg.empty_holder_raw, sensor->get_calibration_factor())
        : 0.0f;
    if (gross_weight_out) *gross_weight_out = gross_g;
    if (range_out) *range_out = range_g;
    return remote_safety::evaluate_calibrated_cup_interlock(gross_g, range_g, cfg);
}

String DeviceApi::safety_json() {
    const auto cfg = get_safety_config();
    float gross_g = 0.0f;
    float range_g = 0.0f;
    const auto status = evaluate_live_cup_interlock(&gross_g, &range_g);
    const auto adaptive = grind_controller_
                              ? grind_controller_->get_adaptive_coast_state()
                              : adaptive_grind::AdaptiveCoastState{};
    char json[704];
    snprintf(json, sizeof(json),
             "{\"api\":\"v1\",\"require_cup_on_remote\":%s,"
             "\"empty_holder_calibrated\":%s,\"empty_holder_raw\":%ld,"
             "\"cup_min_weight_g\":%.1f,\"cup_max_weight_g\":%.1f,"
             "\"remote_max_grind_time_s\":%u,"
             "\"no_flow_timeout_ms\":%u,\"no_flow_min_weight_g\":%.2f,"
             "\"wifi_tx_power_dbm\":%u,"
             "\"adaptive_grind_enabled\":%s,"
             "\"adaptive_coast\":{\"enabled\":%s,\"learned_coast_time_ms\":%.1f,"
             "\"reference_flow_gps\":%.2f,\"sample_count\":%lu},"
             "\"live\":{\"gross_weight_g\":%.2f,\"weight_range_g\":%.2f,\"cup_status\":\"%s\"}}",
             cfg.require_cup_on_remote ? "true" : "false",
             cfg.empty_holder_calibrated ? "true" : "false",
             static_cast<long>(cfg.empty_holder_raw),
             finite_or_zero(cfg.cup_min_weight_g), finite_or_zero(cfg.cup_max_weight_g),
             static_cast<unsigned>(cfg.remote_max_grind_time_s),
             static_cast<unsigned>(cfg.no_flow_timeout_ms),
             finite_or_zero(cfg.no_flow_min_weight_g),
             static_cast<unsigned>(cfg.wifi_tx_power_dbm),
             adaptive.algorithm_enabled ? "true" : "false",
             adaptive.enabled ? "true" : "false",
             finite_or_zero(adaptive.learned_coast_time_ms),
             finite_or_zero(adaptive.reference_flow_gps),
             static_cast<unsigned long>(adaptive.sample_count),
             finite_or_zero(gross_g), finite_or_zero(range_g),
             remote_safety::cup_interlock_reason(status));
    return String(json);
}

String DeviceApi::build_state_message() {
    // Capture profile data and cup interlock BEFORE locking control_mutex_ so
    // profile/scale scans never stall the Core 0 real-time motor loop.
    const auto profile = profile_controller_->snapshot();
    float gross_g = 0.0f;
    const auto cup_status = evaluate_live_cup_interlock(&gross_g, nullptr);

    WeightSensor* sensor = hardware_->get_weight_sensor();
    Grinder* grinder = hardware_->get_grinder();

    bool active = false;
    const char* phase_name = "idle";
    GrindMode mode = profile.mode;
    int profile_idx = profile.current_profile;
    int progress = 0;
    float target_weight = profile.profiles[profile.current_profile].weight;
    uint32_t target_time_ms = static_cast<uint32_t>(
        profile.profiles[profile.current_profile].time_seconds * 1000.0f);
    char last_error[32] = {0};

    {
        const auto control_lock = grind_controller_->lock_control();
        const bool idle = grind_controller_->get_phase() == GrindPhase::IDLE;
        active = grind_controller_->is_active();
        phase_name = api_phase_name(*grind_controller_);
        mode = idle ? profile.mode : grind_controller_->get_mode();
        profile_idx = idle ? profile.current_profile : grind_controller_->get_session_descriptor().profile_id;
        progress = grind_controller_->get_current_progress_percent();
        if (!idle) {
            target_weight = grind_controller_->get_target_weight();
            target_time_ms = grind_controller_->get_target_time_ms();
        }
        grind_controller_->get_last_error_message(last_error, sizeof(last_error));
    }

    // The web metric is for a human-readable live display, so mirror the
    // steadier filtered value used by the on-device UI. The high-frequency raw
    // control samples remain available in the saved session trace.
    const float weight = sensor ? finite_or_zero(sensor->get_display_weight()) : 0.0f;
    const float flow = sensor ? finite_or_zero(sensor->get_flow_rate()) : 0.0f;
    const bool motor_running = grinder && grinder->is_grinding();

    char message[768];
    snprintf(message, sizeof(message),
             "{\"api\":\"v1\",\"type\":\"state\",\"seq\":%lu,\"timestamp_ms\":%lu,"
             "\"grind\":{\"active\":%s,\"phase\":\"%s\",\"mode\":\"%s\",\"profile\":%d,\"progress\":%d,"
             "\"target_weight\":%.2f,\"target_time_ms\":%lu,\"last_error\":\"%s\"},"
             "\"scale\":{\"weight\":%.2f,\"flow\":%.2f,\"gross_weight_g\":%.2f,\"cup_status\":\"%s\"},"
             "\"motor\":{\"running\":%s},\"system\":{\"free_heap\":%u}}",
             static_cast<unsigned long>(sequence_.fetch_add(1) + 1), static_cast<unsigned long>(millis()),
             active ? "true" : "false",
             phase_name,
             mode == GrindMode::TIME ? "time" : mode == GrindMode::MANUAL ? "manual" : "weight",
             profile_idx,
             progress,
             finite_or_zero(target_weight),
             static_cast<unsigned long>(target_time_ms),
             last_error,
             weight, flow, finite_or_zero(gross_g), remote_safety::cup_interlock_reason(cup_status),
             motor_running ? "true" : "false",
             static_cast<unsigned int>(ESP.getFreeHeap()));
    return String(message);
}
