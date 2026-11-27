# Pre-Grinder Bench Testing Checklist

Run this bench checklist on the ESP32-S3 board **before connecting mains power (220V/110V) or the grinder motor**. It verifies the display, touch input, Wi-Fi, Smart Grind Studio, Morning Notes, Portafilter screen, load cell, low-voltage relay safety, and Web OTA in roughly 20 minutes on the desk.

For full on-grinder validation after installing the controller into the grinder, continue with [HARDWARE_ACCEPTANCE.md](HARDWARE_ACCEPTANCE.md).

---

## Stage 0 — Hardware Revision Verification & Initial USB Flash

- [ ] **Confirm your Waveshare 1.64" AMOLED hardware revision (critical — pins differ in `src/config/hardware.h`):**
  - **V1 (`waveshare-esp32s3-touch-amoled-164`):** `HX711 DOUT = GPIO3`, `SCK = GPIO2`, `Motor Relay = GPIO18`
  - **V2 (`waveshare-esp32s3-touch-amoled-164-v2`):** `HX711 DOUT = GPIO3`, `SCK = GPIO1`, `Motor Relay = GPIO16` (`GPIO18` is shared with `TP_INT` and must not drive the relay)
- [ ] **Flash `feat/web-studio-and-notes` over USB** using the matching target (`v1` or `v2`):
  ```bash
  tools/venv/bin/python3 tools/grinder.py usb --hardware v1
  # or
  tools/venv/bin/python3 tools/grinder.py usb --hardware v2
  ```
- [ ] **Open Serial Monitor (`115200` baud)** and confirm clean boot:
  - No boot-loop, watchdog reset, PSRAM init error, LittleFS mount error, or NVS failure.
  - *Note:* Without an HX711 connected yet, weight-mode grinds are intentionally blocked (`scale_fault` / `stale`), while display, Wi-Fi, Web Studio, Morning Notes, and time mode can all be tested normally. For V1 desk testing with a simulated load cell, you can optionally build the `waveshare-esp32s3-touch-amoled-164-mock` environment in `platformio.ini`.

---

## Stage 1 — AMOLED Display, Touch & Screen Transitions

- [ ] Display renders sharply in the right orientation with no flicker or LVGL artifacts.
- [ ] Touch responds accurately in all four corners (switching profiles, Grind button, scrolling settings).
- [ ] In **Settings -> Display**, toggle **Portafilter Screen** on/off and adjust brightness; reboot the board and confirm both persist.
- [ ] Let the board sit until **Screen Timeout / Idle** dims or sleeps the panel, then tap to verify clean wake-up.

---

## Stage 2 — Wi-Fi, Smart Grind Studio, WebSocket & PWA

- [ ] Connect to your 2.4 GHz home Wi-Fi (via the setup AP or Improv Serial) and confirm a stable IP address.
- [ ] Open Smart Grind Studio from both phone and desktop (`http://<IP>/` or `http://smart-grinder.local/`):
  - [ ] WebSocket status indicator turns green and live telemetry updates continuously.
  - [ ] Edit a profile's target weight or time in the browser -> confirm the on-device touchscreen updates immediately and retains the value across a power cycle.
  - [ ] Install as a **PWA** ("Add to Home Screen" on mobile) and confirm it launches full-screen.

---

## Stage 3 — Morning Notes (PSRAM, LittleFS & Clock Sync)

- [ ] **Show now:** Draw or type a note in the Morning Notes tab and send it:
  - Confirm it renders immediately on the AMOLED with no PSRAM allocation failure (~255 KB) or crash.
  - Confirm tapping the screen dismisses it cleanly.
- [ ] **Preview status reporting:**
  - While the grinder screen is on **READY**, click Preview -> confirm the note previews on screen and the browser reports `shown`.
  - Open **Settings** on the touchscreen and click Preview -> confirm the preview is safely blocked and the browser reports the blocked status instead of silently succeeding.
- [ ] **Scheduled delivery (One-shot / Daily):**
  - Confirm `/api/v1/note/clock` syncs your local timezone/DST accurately.
  - Schedule a one-shot note 2 minutes ahead, reboot the board, and verify the note appears on time after restart.

---

## Stage 4 — Low-Voltage Motor Relay Check (No Mains / No 220V!)

Connect an LED + resistor or a relay module **completely disconnected from mains AC** (listen for the click / watch the indicator LED) to the motor output pin (`GPIO18` on V1, `GPIO16` on V2):

- [ ] **Power-on / Reset safety test (most important check):** Unplug/replug USB and press Reset several times — the relay output must stay **completely OFF** throughout the entire boot sequence (not even a brief glitch/click).
- [ ] **Time Mode (2 s)** from both touchscreen and Web Studio:
  - Relay turns on for 2 seconds and turns off cleanly.
- [ ] **Emergency STOP:** Start a 10 s time grind and press **STOP** on the screen, then repeat from Web Studio — relay turns off immediately.
- [ ] **Portafilter Screen animation:** With Portafilter Screen enabled, run a grind and verify the large weight readout, animated coffee mound, and completion pulse/deviation render smoothly.

---

## Stage 5 — HX711 Load Cell, Remote Safety & Top-up Pulse

*(Run once the HX711 + load cell are wired on the bench.)*

- [ ] Serial logs / Diagnostics show healthy sampling (~10 SPS) with no `HX711 sample rate` warning and no `near ADC rail` saturation.
- [ ] **Calibration & Tare:** Calibrate with a known weight (e.g., 100 g), tare, and confirm resting noise stays within `±0.05 g`.
- [ ] **Cup Interlock (`require_cup_on_remote`):**
  1. With the scale empty, click **Calibrate Empty Holder** in Smart Grind Studio.
  2. Enable `require_cup_on_remote`.
  3. Trigger a remote Start **without a cup** -> must be refused with a clear plain-language reason on the dashboard.
  4. Place a cup/weight inside `[cup_min_weight_g, cup_max_weight_g]` -> remote Start is accepted.
- [ ] **No-Flow Watchdog:** Trigger a remote Start with the cup in place and do **not** apply extra weight -> relay must stop automatically after the configured no-flow timeout (~4 s default) and the dashboard must explain why it stopped.
- [ ] **Finger-press weight grind + Top-up Pulse (`+`):**
  - Start an 18 g weight grind and gradually press down on the load cell up to 18 g.
  - Confirm the dashed target line on the live dashboard chart, the Portafilter mound easing, and the completion screen showing final deviation and the **`+` Top-up Pulse** button.
  - Lift the cup/weight while on `GRIND_COMPLETE` -> confirm the `+` button immediately disappears/updates without leaving the screen.

---

## Stage 6 — Web OTA Verification

- [ ] Upload a compiled `.bin` image through the Web OTA UI before sealing the controller in the enclosure:
  - Confirm update reaches 100%, reboots cleanly into the new image, and preserves Wi-Fi credentials, profiles, safety settings, and scheduled Morning Notes.

---

## Deferred Until Grinder Installation

Once all bench stages pass, only two hardware-coupled behaviors remain for the assembled grinder:

1. **Mains motor EMI & vibration immunity:** Confirm switching the real grinder motor on/off does not brown-out/reset the ESP32-S3 or inject spikes into the HX711 (add an RC snubber across relay/SSR contacts and keep load-cell wires routed away from AC wiring if needed).
2. **Motor Auto-Tune & Adaptive Coast:** Run `motor_response_latency_ms` auto-tune and 3–4 real coffee shots so `adaptive_coast` learns your burr/chute behavior.
