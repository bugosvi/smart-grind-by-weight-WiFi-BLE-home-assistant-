#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "wifi_join_failure.h"

enum class NetworkState : uint8_t {
    WIFI_DISABLED,
    WIFI_NO_CREDENTIALS,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_RETRY_WAIT,
    WIFI_SETUP_REQUIRED,
    WIFI_SETUP_AP
};

class SmartGrindNetworkManager {
public:
    void init(Preferences* preferences);
    void update();
    void set_grind_active(bool active) { grind_active_.store(active); }

    bool set_enabled(bool enabled);
    bool set_credentials(const String& ssid, const String& password);
    bool connect_saved_credentials();
    void clear_credentials();
    bool start_setup_access_point(const String& ssid, const String& password);

    bool is_enabled() const { return enabled_.load(); }
    bool is_connected() const { return state() == NetworkState::WIFI_CONNECTED; }
    bool has_credentials() const;
    NetworkState state() const { return state_.load(); }
    String hostname() const;
    String device_id() const;
    String network_name() const;
    String ip_address() const;

    // Why the last attempt to join the saved network failed; NONE once joined.
    WifiJoinFailure last_join_failure() const { return last_join_failure_.load(); }
    // "Could not join <network>: <reason>." in plain ASCII, or empty.
    String join_failure_summary() const;
    // The summary followed by what the user can do about it, or empty.
    String join_failure_explanation() const;
    // Optional Wi-Fi TX power cap in dBm (0 = unrestricted stock default; 8..20 = cap)
    void set_tx_power_dbm(uint8_t dbm);
    uint8_t tx_power_dbm() const { return tx_power_dbm_.load(); }

private:
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t RETRY_DELAY_MS = 30000;

    Preferences* preferences_ = nullptr;
    std::atomic<NetworkState> state_{NetworkState::WIFI_DISABLED};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> grind_active_{false};
    std::atomic<uint8_t> tx_power_dbm_{0};
    // Written by the Wi-Fi event task, read when a join attempt times out.
    std::atomic<uint16_t> last_disconnect_reason_{wifi_disconnect_reason::NONE};
    std::atomic<WifiJoinFailure> last_join_failure_{WifiJoinFailure::NONE};
    bool ever_connected_ = false;
    bool mdns_started_ = false;
    uint32_t state_changed_at_ms_ = 0;
    String ssid_;
    String password_;
    String hostname_;
    mutable SemaphoreHandle_t settings_mutex_ = nullptr;

    void load_settings();
    void begin_connection();
    void handle_connected();
    void stop_network();
    void set_state(NetworkState state);
    void apply_tx_power() const;
    static String default_hostname();
    static String sanitize_hostname(const String& hostname);
};

extern SmartGrindNetworkManager network_manager;
