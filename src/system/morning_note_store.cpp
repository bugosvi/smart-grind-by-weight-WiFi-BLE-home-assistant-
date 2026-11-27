#include "morning_note_store.h"

#include <LittleFS.h>
#include <cstdio>
#include <ctime>
#include <esp_timer.h>

#include "../config/constants.h"
#include "../config/logging.h"

MorningNoteStore morning_note_store;

namespace {
constexpr const char* kConfigTempPath = "/note.json.tmp";
constexpr const char* kConfigBackupPath = "/note.json.bak";
constexpr size_t kMaxConfigFileBytes = 1536;

uint64_t boot_seconds() {
    const int64_t us = esp_timer_get_time();
    return us > 0 ? static_cast<uint64_t>(us / 1000000LL) : 0ULL;
}

void append_json_escaped(String& out, const char* src) {
    if (!src) return;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(src); *p != '\0'; ++p) {
        const unsigned char ch = *p;
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 0x20U) out += static_cast<char>(ch);
                break;
        }
    }
}
}

void MorningNoteStore::init() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (initialized_) return;
    cleanup_temp_files_locked();
    has_image_cached_ = has_image_locked();
    load_config_locked();
    initialized_ = true;
    LOG_BLE("[NOTE] Initialized (enabled=%s, mode=%s, has_image=%s)\n",
            config_.enabled ? "true" : "false",
            morning_note::mode_to_string(config_.mode),
            has_image_cached_ ? "true" : "false");
}

void MorningNoteStore::cleanup_temp_files_locked() {
    if (LittleFS.exists(MORNING_NOTE_TEMP_FILENAME)) {
        LittleFS.remove(MORNING_NOTE_TEMP_FILENAME);
    }
    if (LittleFS.exists(kConfigTempPath)) {
        LittleFS.remove(kConfigTempPath);
    }
}

bool MorningNoteStore::has_image_locked() const {
    File image = LittleFS.open(MORNING_NOTE_IMAGE_FILENAME, "r");
    const bool valid = image && image.size() == BLE_IMAGE_EXPECTED_SIZE;
    if (image) {
        image.close();
    }
    return valid;
}

bool MorningNoteStore::has_image() const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return has_image_cached_;
}

bool MorningNoteStore::load_image_buffer(uint8_t* dst, size_t dst_size) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!dst || dst_size != BLE_IMAGE_EXPECTED_SIZE || !has_image_cached_ || upload_active_.load()) {
        return false;
    }
    File f = LittleFS.open(MORNING_NOTE_IMAGE_FILENAME, "r");
    if (!f) {
        return false;
    }
    size_t total_read = 0;
    constexpr size_t kChunkSize = 4096;
    while (total_read < dst_size) {
        const size_t to_read = (dst_size - total_read < kChunkSize) ? (dst_size - total_read) : kChunkSize;
        const size_t bytes_read = f.read(dst + total_read, to_read);
        if (bytes_read == 0) break;
        total_read += bytes_read;
    }
    f.close();
    return total_read == dst_size;
}

uint64_t MorningNoteStore::current_epoch_locked(bool* clock_synced) const {
    const time_t raw_now = time(nullptr);
    if (raw_now >= static_cast<time_t>(morning_note::kMinValidEpoch)) {
        if (clock_synced) *clock_synced = true;
        return static_cast<uint64_t>(raw_now);
    }
    if (fallback_anchor_epoch_ >= morning_note::kMinValidEpoch) {
        const uint64_t now_boot = boot_seconds();
        const uint64_t elapsed_s =
            now_boot >= fallback_anchor_boot_s_ ? (now_boot - fallback_anchor_boot_s_) : 0ULL;
        if (clock_synced) *clock_synced = true;
        return fallback_anchor_epoch_ + elapsed_s;
    }
    if (clock_synced) *clock_synced = false;
    return 0;
}

uint64_t MorningNoteStore::current_epoch(bool* clock_synced) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return current_epoch_locked(clock_synced);
}

bool MorningNoteStore::begin_image_upload(size_t expected_size) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_) init();
    if (upload_active_.load() || expected_size != BLE_IMAGE_EXPECTED_SIZE) {
        return false;
    }
    const auto token = operation_interlock().try_acquire();
    if (!token) {
        return false;
    }
    if (LittleFS.exists(MORNING_NOTE_TEMP_FILENAME)) {
        LittleFS.remove(MORNING_NOTE_TEMP_FILENAME);
    }
    upload_file_ = LittleFS.open(MORNING_NOTE_TEMP_FILENAME, "w");
    if (!upload_file_) {
        operation_interlock().release(token);
        LOG_BLE("[NOTE] Failed to open temp note image file\n");
        return false;
    }
    upload_token_ = token;
    upload_expected_size_ = expected_size;
    upload_received_size_ = 0;
    upload_active_.store(true);
    return true;
}

bool MorningNoteStore::write_image_chunk(const uint8_t* data, size_t len) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!upload_active_.load() || !upload_file_) {
        return false;
    }
    if (len == 0) return true;
    if (!data || upload_received_size_ + len > upload_expected_size_) {
        abort_image_upload();
        return false;
    }
    const size_t written = upload_file_.write(data, len);
    if (written != len) {
        LOG_BLE("[NOTE] Note image chunk write failed (%u/%u)\n",
                static_cast<unsigned>(written), static_cast<unsigned>(len));
        abort_image_upload();
        return false;
    }
    upload_received_size_ += len;
    return true;
}

bool MorningNoteStore::finish_image_upload() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!upload_active_.load()) {
        return false;
    }
    if (upload_file_) {
        upload_file_.close();
    }
    auto cleanup_release = [&]() {
        if (LittleFS.exists(MORNING_NOTE_TEMP_FILENAME)) {
            LittleFS.remove(MORNING_NOTE_TEMP_FILENAME);
        }
        upload_expected_size_ = 0;
        upload_received_size_ = 0;
        upload_active_.store(false);
        if (upload_token_) {
            operation_interlock().release(upload_token_);
            upload_token_ = 0;
        }
    };

    if (upload_received_size_ != upload_expected_size_) {
        LOG_BLE("[NOTE] Note image size mismatch (%u != %u)\n",
                static_cast<unsigned>(upload_received_size_),
                static_cast<unsigned>(upload_expected_size_));
        cleanup_release();
        return false;
    }

    LittleFS.remove(MORNING_NOTE_BACKUP_FILENAME);
    const bool had_existing = LittleFS.exists(MORNING_NOTE_IMAGE_FILENAME);
    if (had_existing &&
        !LittleFS.rename(MORNING_NOTE_IMAGE_FILENAME, MORNING_NOTE_BACKUP_FILENAME)) {
        LOG_BLE("[NOTE] Failed to back up existing note image\n");
        cleanup_release();
        return false;
    }

    if (!LittleFS.rename(MORNING_NOTE_TEMP_FILENAME, MORNING_NOTE_IMAGE_FILENAME)) {
        LOG_BLE("[NOTE] Failed to commit new note image\n");
        if (had_existing) {
            LittleFS.rename(MORNING_NOTE_BACKUP_FILENAME, MORNING_NOTE_IMAGE_FILENAME);
        }
        cleanup_release();
        return false;
    }

    const bool committed_ok = has_image_locked();
    if (!committed_ok) {
        LittleFS.remove(MORNING_NOTE_IMAGE_FILENAME);
        if (had_existing) {
            LittleFS.rename(MORNING_NOTE_BACKUP_FILENAME, MORNING_NOTE_IMAGE_FILENAME);
        }
        has_image_cached_ = has_image_locked();
        cleanup_release();
        return false;
    }

    LittleFS.remove(MORNING_NOTE_BACKUP_FILENAME);
    has_image_cached_ = true;
    revision_.fetch_add(1);
    cleanup_release();
    LOG_BLE("[NOTE] Stored note image (%d bytes)\n", BLE_IMAGE_EXPECTED_SIZE);
    return true;
}

void MorningNoteStore::abort_image_upload() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!upload_active_.load()) {
        return;
    }
    if (upload_file_) {
        upload_file_.close();
    }
    if (LittleFS.exists(MORNING_NOTE_TEMP_FILENAME)) {
        LittleFS.remove(MORNING_NOTE_TEMP_FILENAME);
    }
    upload_expected_size_ = 0;
    upload_received_size_ = 0;
    upload_active_.store(false);
    if (upload_token_) {
        operation_interlock().release(upload_token_);
        upload_token_ = 0;
    }
}

morning_note::MorningNoteConfig MorningNoteStore::get_config() const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return config_;
}

bool MorningNoteStore::persist_config_locked(const morning_note::MorningNoteConfig& cfg) {
    String json;
    json.reserve(640);
    char prefix[256];
    const int prefix_len = snprintf(
        prefix, sizeof(prefix),
        "{\"enabled\":%s,\"mode\":\"%s\",\"start_epoch\":%llu,\"end_epoch\":%llu,"
        "\"start_min\":%u,\"end_min\":%u,\"tz_offset_min\":%d,\"clear_on_grind\":%s,"
        "\"dismissed_day\":%lu,\"headline\":\"",
        cfg.enabled ? "true" : "false",
        morning_note::mode_to_string(cfg.mode),
        static_cast<unsigned long long>(cfg.start_epoch),
        static_cast<unsigned long long>(cfg.end_epoch),
        static_cast<unsigned>(cfg.start_min),
        static_cast<unsigned>(cfg.end_min),
        static_cast<int>(cfg.tz_offset_min),
        cfg.clear_on_grind ? "true" : "false",
        static_cast<unsigned long>(cfg.dismissed_day));
    if (prefix_len <= 0 || static_cast<size_t>(prefix_len) >= sizeof(prefix)) {
        return false;
    }
    json += prefix;
    append_json_escaped(json, cfg.headline);
    json += "\",\"body\":\"";
    append_json_escaped(json, cfg.body);
    json += "\",\"signature\":\"";
    append_json_escaped(json, cfg.signature);
    json += "\",\"theme\":\"";
    json += cfg.theme;
    json += "\"}";

    if (json.length() >= kMaxConfigFileBytes) {
        return false;
    }

    if (LittleFS.exists(kConfigTempPath)) {
        LittleFS.remove(kConfigTempPath);
    }
    File f = LittleFS.open(kConfigTempPath, "w");
    if (!f) return false;
    const size_t n = f.write(reinterpret_cast<const uint8_t*>(json.c_str()), json.length());
    f.close();
    if (n != json.length()) {
        LittleFS.remove(kConfigTempPath);
        return false;
    }

    LittleFS.remove(kConfigBackupPath);
    const bool had_existing = LittleFS.exists(MORNING_NOTE_CONFIG_FILENAME);
    if (had_existing && !LittleFS.rename(MORNING_NOTE_CONFIG_FILENAME, kConfigBackupPath)) {
        LittleFS.remove(kConfigTempPath);
        return false;
    }
    if (!LittleFS.rename(kConfigTempPath, MORNING_NOTE_CONFIG_FILENAME)) {
        if (had_existing) {
            LittleFS.rename(kConfigBackupPath, MORNING_NOTE_CONFIG_FILENAME);
        }
        LittleFS.remove(kConfigTempPath);
        return false;
    }
    LittleFS.remove(kConfigBackupPath);
    return true;
}

void MorningNoteStore::load_config_locked() {
    config_ = morning_note::MorningNoteConfig{};
    File f = LittleFS.open(MORNING_NOTE_CONFIG_FILENAME, "r");
    if (!f) return;
    const size_t size = f.size();
    if (size == 0 || size >= kMaxConfigFileBytes) {
        f.close();
        return;
    }
    String raw;
    if (!raw.reserve(size + 1)) {
        f.close();
        return;
    }
    char chunk[128];
    size_t remaining = size;
    while (remaining > 0) {
        const size_t to_read = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        const size_t got = f.read(reinterpret_cast<uint8_t*>(chunk), to_read);
        if (got == 0) break;
        raw.concat(chunk, got);
        remaining -= got;
    }
    f.close();

    morning_note::MorningNoteConfig parsed{};
    if (morning_note::parse_morning_note_json(raw.c_str(), morning_note::MorningNoteConfig{}, parsed,
                                              nullptr, nullptr, true) &&
        morning_note::validate_morning_note_config(parsed)) {
        config_ = parsed;
    }
}

bool MorningNoteStore::update_config(const morning_note::MorningNoteConfig& cfg,
                                     uint64_t client_epoch,
                                     bool has_client_epoch) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_) init();
    if (!morning_note::validate_morning_note_config(cfg)) {
        return false;
    }
    morning_note::MorningNoteConfig next = cfg;
    // Saving or re-arming a note resets any prior daily dismissal.
    next.dismissed_day = 0;

    if (has_client_epoch && client_epoch >= morning_note::kMinValidEpoch) {
        fallback_anchor_epoch_ = client_epoch;
        fallback_anchor_boot_s_ = boot_seconds();
    }
    if (!persist_config_locked(next)) {
        return false;
    }
    config_ = next;
    revision_.fetch_add(1);
    LOG_BLE("[NOTE] Saved config (enabled=%s, mode=%s, clear_on_grind=%s)\n",
            config_.enabled ? "true" : "false",
            morning_note::mode_to_string(config_.mode),
            config_.clear_on_grind ? "true" : "false");
    return true;
}

bool MorningNoteStore::clear_note() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_) init();
    if (upload_active_.load()) {
        return false;
    }
    if (LittleFS.exists(MORNING_NOTE_IMAGE_FILENAME)) {
        if (!LittleFS.remove(MORNING_NOTE_IMAGE_FILENAME)) {
            return false;
        }
        has_image_cached_ = false;
    }
    if (LittleFS.exists(MORNING_NOTE_CONFIG_FILENAME) &&
        !LittleFS.remove(MORNING_NOTE_CONFIG_FILENAME)) {
        revision_.fetch_add(1);
        return false;
    }
    config_ = morning_note::MorningNoteConfig{};
    has_image_cached_ = false;
    preview_request_s_.store(0);
    revision_.fetch_add(1);
    LOG_BLE("[NOTE] Cleared morning note and frame\n");
    return true;
}

bool MorningNoteStore::is_due_now() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_ || upload_active_.load()) {
        return false;
    }
    bool clock_synced = false;
    const uint64_t now_epoch = current_epoch_locked(&clock_synced);
    const auto eval = morning_note::evaluate_morning_note_due(
        config_, now_epoch, clock_synced, has_image_cached_);
    if (eval.expired_one_shot && config_.enabled) {
        config_.enabled = false;
        persist_config_locked(config_);
        revision_.fetch_add(1);
        LOG_BLE("[NOTE] One-shot morning note window ended; disarmed\n");
    }
    return eval.is_due;
}

void MorningNoteStore::on_grind_completed() {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_) return;
    bool clock_synced = false;
    const uint64_t now_epoch = current_epoch_locked(&clock_synced);
    if (morning_note::apply_grind_completion(config_, now_epoch, clock_synced, has_image_cached_)) {
        persist_config_locked(config_);
        revision_.fetch_add(1);
        LOG_BLE("[NOTE] Auto-cleared morning note after completed grind\n");
    }
}

bool MorningNoteStore::request_preview(uint16_t duration_s) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!initialized_ || upload_active_.load() || !has_image_cached_) {
        return false;
    }
    if (duration_s == 0) duration_s = morning_note::kDefaultPreviewDurationS;
    if (duration_s > morning_note::kMaxPreviewDurationS) {
        duration_s = morning_note::kMaxPreviewDurationS;
    }
    preview_request_s_.store(duration_s);
    return true;
}

uint16_t MorningNoteStore::consume_preview_request() {
    return preview_request_s_.exchange(0);
}

String MorningNoteStore::status_json() const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    bool clock_synced = false;
    const uint64_t now_epoch = current_epoch_locked(&clock_synced);
    const auto eval = morning_note::evaluate_morning_note_due(
        config_, now_epoch, clock_synced, has_image_cached_);

    String json;
    json.reserve(768);
    char prefix[320];
    snprintf(
        prefix, sizeof(prefix),
        "{\"api\":\"v1\",\"enabled\":%s,\"due_now\":%s,\"has_image\":%s,"
        "\"clock_synced\":%s,\"device_epoch\":%llu,"
        "\"mode\":\"%s\",\"start_epoch\":%llu,\"end_epoch\":%llu,"
        "\"start_min\":%u,\"end_min\":%u,\"tz_offset_min\":%d,"
        "\"clear_on_grind\":%s,\"headline\":\"",
        config_.enabled ? "true" : "false",
        eval.is_due ? "true" : "false",
        has_image_cached_ ? "true" : "false",
        clock_synced ? "true" : "false",
        static_cast<unsigned long long>(now_epoch),
        morning_note::mode_to_string(config_.mode),
        static_cast<unsigned long long>(config_.start_epoch),
        static_cast<unsigned long long>(config_.end_epoch),
        static_cast<unsigned>(config_.start_min),
        static_cast<unsigned>(config_.end_min),
        static_cast<int>(config_.tz_offset_min),
        config_.clear_on_grind ? "true" : "false");
    json += prefix;
    append_json_escaped(json, config_.headline);
    json += "\",\"body\":\"";
    append_json_escaped(json, config_.body);
    json += "\",\"signature\":\"";
    append_json_escaped(json, config_.signature);
    json += "\",\"theme\":\"";
    json += config_.theme;
    json += "\"}";
    return json;
}
