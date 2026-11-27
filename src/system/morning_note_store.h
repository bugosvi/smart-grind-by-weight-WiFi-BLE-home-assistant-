#pragma once

#include <Arduino.h>
#include <FS.h>
#include <atomic>
#include <cstdint>
#include <mutex>

#include "morning_note_policy.h"
#include "operation_interlock.h"

/**
 * MorningNoteStore - Thread-safe LittleFS persistence and lifecycle manager
 * for scheduled AMOLED Morning Notes (/note.rgb565 + /note.json).
 *
 * Accessed concurrently by AsyncTCP web handlers and the LVGL UI task.
 * Never invokes LVGL APIs directly.
 */
class MorningNoteStore {
public:
    void init();

    // Image upload (protected by mutex + OperationInterlock, atomic .tmp/.bak swap)
    bool begin_image_upload(size_t expected_size);
    bool write_image_chunk(const uint8_t* data, size_t len);
    bool finish_image_upload();
    void abort_image_upload();
    bool is_upload_active() const { return upload_active_.load(); }

    // Config & schedule operations
    morning_note::MorningNoteConfig get_config() const;
    bool update_config(const morning_note::MorningNoteConfig& cfg,
                       uint64_t client_epoch,
                       bool has_client_epoch);
    bool clear_note();

    // Clock, evaluation, and lifecycle hooks (UI task)
    bool has_image() const;
    bool load_image_buffer(uint8_t* dst, size_t dst_size) const;
    uint64_t current_epoch(bool* clock_synced = nullptr) const;
    bool is_due_now();
    void on_grind_completed();

    // Live preview signaling (Web -> UI task)
    bool request_preview(uint16_t duration_s = morning_note::kDefaultPreviewDurationS);
    uint16_t consume_preview_request();

    // Increments on any committed image/config change so UI can refresh active note
    uint32_t revision() const { return revision_.load(); }

    // Serializes current note status and metadata for GET/POST /api/v1/note
    String status_json() const;

private:
    bool has_image_locked() const;
    uint64_t current_epoch_locked(bool* clock_synced = nullptr) const;
    bool persist_config_locked(const morning_note::MorningNoteConfig& cfg);
    void load_config_locked();
    void cleanup_temp_files_locked();

    mutable std::recursive_mutex mutex_;
    morning_note::MorningNoteConfig config_{};
    bool initialized_ = false;
    bool has_image_cached_ = false;
    uint64_t fallback_anchor_epoch_ = 0;
    uint64_t fallback_anchor_boot_s_ = 0;

    File upload_file_;
    size_t upload_expected_size_ = 0;
    size_t upload_received_size_ = 0;
    OperationInterlock::Token upload_token_ = 0;
    std::atomic<bool> upload_active_{false};
    std::atomic<uint16_t> preview_request_s_{0};
    std::atomic<uint32_t> revision_{1};
};

extern MorningNoteStore morning_note_store;
