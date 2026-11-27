# אפיון פרויקט מלא: Smart Grind-by-Weight (Eureka Mignon Specialità) + הרחבת Wi-Fi ו-Home Assistant

---

## 0. סקירת הפרויקט המקורי ונקודת ההתחלה

* **הריפו המקורי (בסיס לפיתוח / Fork):** [jaapp/smart-grind-by-weight](https://github.com/jaapp/smart-grind-by-weight) (`main`)
* **סרטון הרכבה רשמי של היוצר (6:39 דק׳):** [YouTube – smart-grind-by-weight Eureka Mignon Specialita assembly](http://www.youtube.com/watch?v=-kfKjiwJsGM)
* **רפרנס ארכיטקטוני מקביל:** [jb-xyz/openGBW](https://github.com/jb-xyz/openGBW)

### מה הפרויקט המקורי כולל?
הפרויקט הופך מטחנת **Eureka Mignon Specialità** מבוססת-זמן למטחנת **Grind-by-Weight (GBW)** מדויקת, באמצעות החלפת המסך המקורי ומזלג הפורטפילטר בחיישן משקל ובקר ESP32-S3.

#### רכיבי החומרה (BOM)
* **בקר ומסך:** Waveshare ESP32-S3 1.64" AMOLED (מעבד דו-ליבתי 240MHz, 16MB Flash, 8MB PSRAM, Wi-Fi 2.4GHz + BLE 5, בקר מסך `CO5300`, מגע `FT3168`).
* **חיישן משקל (Load Cell):** מסוג **T70** (1 ק"ג — מתאים הן לכוסית והן לידית פורטפילטר; גרסת 0.3 ק"ג מוגבלת לכוסית בלבד).
* **מגבר משקל:** מודול **HX711**.
* **שליטה במנוע:** ריליי (SSR / ממסר) המחובר לקו הפעלת המנוע של המטחנה.
* **ברגים וחומרה:** 4× ברגי M3 (2 לצד הקבוע של ה-T70 ו-2 לצד הצף) + ברגי המטחנה המקוריים.

#### חלקי התלת-ממד המקוריים (5 חלקים)
1. **Screen adapter** — מחליף את מסגרת המסך המקורית בחזית העליונה ומחזיק את מסך ה-Waveshare.
2. **Back plate** — מתיישב בשקע התחתון של החזית (במקום המזלג המקורי), ועליו מוברג הצד הקבוע של ה-Load Cell (ויושב ה-HX711).
3. **Cover plate** — מכסה הגימור שנסגר מעל ה-Back plate וננעל עם בורג המזלג המקורי.
4. **Cup holder (54mm / 58mm)** — מחזיק כוסית שקילה המוברג לצד הצף של ה-Load Cell (ניתן להחלפה במתאם פורטפילטר — ראו סעיף 8).
5. **Screw hole covers** — כיסויי ברגים המסתירים את ראשי ה-M3 ומונעים הצטברות קפה.

#### תוכנה וממשקים בגרסה המקורית
* **ממשק מסך AMOLED (LVGL):** בחירה בין תצוגה נקייה לתצוגת גרפים בזמן אמת (משקל, קצב זרימה, עצירה חזויה ופולסים), ניהול פרופילים וכיול.
* **מכונת מצבים לטחינה:** `IDLE` ← `TARING` ← `PREDICTIVE` ← `SETTLING` ← `PULSE` ← `COMPLETED`.
* **קישוריות מקורית (BLE בלבד):**
  * צריבה ראשונית ב-USB (דרך Web Flasher ב-Chrome/Edge במחשב או Android).
  * עדכוני קושחה אלחוטיים (**BLE OTA**) ללא צורך בכבל או פירוק.
  * שליפת לוגים ודשבורד אנליטיקה במחשב ב-Streamlit (`python3 tools/grinder.py analyze`).

#### תקציר פירוק והרכבה מכנית (12 שלבים)
* **שלב א׳ — פירוק המטחנה מהמפעל:** ניתוק מהחשמל ← הסרת מיכל הפולים ← שליפת תגית ה-Eureka האחורית והברגת בורג המכסה העליון ← הרמת המכסה העליון ← פירוק מזלג הפורטפילטר המקורי (לשמור את הבורג!) ← החלקת חיפוי החזית והוצאת המסך המקורי.
* **שלב ב׳ — הרכבת חלקי התלת-ממד:** הכנסת ה-`Screen adapter` לחיפוי החזית ← הושבת ה-`Back plate` ← הברגת חיישן ה-**T70** (לוודא שהחץ מצביע **כלפי מטה ↓**) עם 2 ברגי M3 ← סגירת `Cover plate` עם בורג המזלג המקורי ← חיבור ה-`Cup holder` (או מתאם הידית) עם 2 ברגי M3 ← כיסויי ברגים וסגירת המכסה והמיכל.

---

## 1. מטרות ואילוצים להרחבת Wi-Fi ו-Home Assistant

1. **אפס הפרעה לטחינה (Real-Time Isolation):** תעבורת Wi-Fi/MQTT או ניתוקי רשת לעולם לא יחסמו (Block) את קריאת ה-HX711 או את כיבוי המנוע.
2. **שמירה על Internal SRAM:** לא מריצים BLE ו-Wi-Fi במקביל בשגרה. BLE משמש אך ורק להגדרה ראשונית (Provisioning) או לעדכון חירום, ונכבה לחלוטין לאחר מכן.
3. **הגדרת רשת קלה מהנייד:** הזנת SSID, סיסמת Wi-Fi ופרטי MQTT דרך Bluetooth (תקן **Improv Wi-Fi**) בלי להקליד על מסך ה-1.64 אינץ'.
4. **Plug & Play ב-Home Assistant:** שימוש ב-**MQTT Auto-Discovery** כך שהמטחנה תופיע אוטומטית כהתקן (Device) עם כל הישויות שלה (ללא צורך במתאם חומרה נוסף ב-HA).
5. **שליטה מרחוק בטוחה:** תמיכה בהפעלת טחינה באוטומציה (למשל 07:00 בבוקר בשילוב מתג ה-Shelly) עם מנגנון אימות כוסית/ידית והגנת תקלות.
6. **עדכוני קושחה אלחוטיים:** מעבר ל-**Wi-Fi OTA** (ושמירת כפתור בתפריט המסך להדלקת BLE זמנית ל-5 דקות).

---

## 2. ארכיטקטורת חומרה, ליבות ורדיו (ESP32-S3)

ה-Waveshare ESP32-S3 (דו-ליבתי, 16MB Flash, 8MB PSRAM) מאפשר הפרדה קשיחה בין זמן-אמת לרשת:

| ליבה | משימות (FreeRTOS Tasks) | עדיפות | הערות |
| :--- | :--- | :--- | :--- |
| **Core 1** | דגימת HX711, אלגוריתם חיזוי, שליטה בריליי המנוע, LVGL UI | גבוהה (High) | ללא שום קריאות רשת חוסמות |
| **Core 0** | ESP-IDF Wi-Fi Stack, MQTT Client, Wi-Fi OTA, Reconnect | נמוכה (Low) | מתקשר עם Core 1 דרך FreeRTOS Queues בלבד |

### מחזור חיי הרדיו (Radio Lifecycle)
1. **בעלייה (Boot):** בדיקת NVS (`Preferences`) לקיום פרטי Wi-Fi.
2. **מצב Provisioning (אין פרטים ב-NVS או לחיצה על "Wi-Fi Setup" בתפריט המסך):**
   * הפעלת BLE עם שירות **Improv Wi-Fi**.
   * התחברות מהנייד (דרך אפליקציית Home Assistant או דפדפן) ← הזנת SSID וסיסמה ← שמירה ב-NVS ← אתחול (Reboot).
3. **מצב עבודה רגיל (יש פרטים ב-NVS):**
   * כיבוי מלא של בקר ה-BLE (`esp_bt_controller_disable()` + שחרור זיכרון) כדי לפנות את ה-Internal SRAM.
   * הפעלת Wi-Fi (STA mode) והתחברות אסינכרונית ל-Broker של ה-MQTT.
   * *Fallback:* אם ה-Wi-Fi לא זמין, המטחנה עובדת **אופליין לחלוטין** ומנסה להתחבר מחדש ברקע על Core 0 עם Exponential Backoff (כל 5 ← 15 ← 60 שניות).

---

## 3. שילוב במכונת המצבים (State Gating)

| מצב טחינה | פעילות רשת מותרת (Core 0) | פעולת טלמטריה |
| :--- | :--- | :--- |
| `IDLE` | חיבור MQTT פעיל, האזנה לפקודות, Wi-Fi OTA | פרסום `state: idle` |
| `TARING` | **השהיית פרסומים ו-Reconnect** | ללא שידור |
| `PREDICTIVE` | **השהיית פרסומים ו-Reconnect** | ללא שידור |
| `SETTLING` | **השהיית פרסומים ו-Reconnect** | ללא שידור |
| `PULSE` | **השהיית פרסומים ו-Reconnect** | ללא שידור |
| `COMPLETED` | חידוש פעילות רשת | דחיפת Struct של סיכום השוט ל-`telemetryQueue` ושידור ב-MQTT |

*העברת המידע מ-Core 1 ל-Core 0 מתבצעת באמצעות `xQueueSend(..., 0)` (Non-blocking).*

---

## 4. פרוטוקול MQTT ו-Home Assistant Auto-Discovery

### מבנה ה-Topics
* **Base Topic:** `smart_gbw/<chip_id>`
* **Availability (LWT):** `smart_gbw/<chip_id>/status` (`online` / `offline` עם `retain: true`)
* **Shot Telemetry:** `smart_gbw/<chip_id>/shot` (JSON payload עם `retain: true`)
* **Discovery Prefix:** `homeassistant/sensor/smart_gbw_<chip_id>/<entity>/config`

### ישויות (Entities) שנחשפות ל-Home Assistant
* `sensor.grinder_last_weight` — משקל אחרון שנטחן (`g`, `state_class: measurement`)
* `sensor.grinder_target_weight` — משקל יעד (`g`)
* `sensor.grinder_weight_error` — סטייה מהיעד (`g`)
* `sensor.grinder_grind_duration` — משך הטחינה (`s`)
* `sensor.grinder_flow_rate` — קצב זרימה ממוצע (`g/s`)
* `sensor.grinder_pulses` — מספר פולסי תיקון שבוצעו
* `sensor.grinder_profile` — פרופיל פעיל (`Single` / `Double` / `Custom`)
* `sensor.grinder_total_grams` — מונה גרמים מצטבר (נשמר ב-NVS, `state_class: total_increasing`)

---

## 5. ממשק פקודות מרחוק (MQTT Command Interface)

### Topics
* **פקודות נכנסות (Subscribe):** `smart_gbw/{chip_id}/cmd` (לעולם ללא `retain`)
* **אירועים ושגיאות (Publish):** `smart_gbw/{chip_id}/event` (ללא `retain`)

### פורמט הפקודות (JSON)

**הפעלת טחינה למשקל יעד:**
```json
{
  "action": "grind",
  "target_weight": 10.0,
  "require_cup": true
}
```

**עצירת חירום מיידית (Emergency Stop):**
```json
{
  "action": "stop"
}
```

### כללי ולידציה והעברה בין ליבות (Core 0 ← Core 1)
1. **תור פקודות (`cmdQueue`):** ה-MQTT Task על Core 0 מפענח את ה-JSON ודוחף Struct לתור עבור Core 1.
2. **עצירת חירום (`stop`):** מטופלת **בכל מצב** ומכבה מיד את ריליי המנוע.
3. **סינון פקודת `grind`:**
   * מתקבלת **רק** אם מכונת המצבים ב-`IDLE` (אחרת מחזירה `{"event": "rejected", "reason": "busy"}`).
   * **גבולות משקל (Sanity Bounds):** דחייה אוטומטית אם `target_weight < 4.0g` או `target_weight > 30.0g`.

---

## 6. מנגנון בטיחות לזיהוי כוסית/ידית וטחינה ללא השגחה (Cup/Portafilter Interlock & Watchdogs)

### פרמטרים שנשמרים ב-NVS (כיול חד-פעמי)
* `empty_holder_raw`: קריאת האפס המכוילת של המחזיק כשהוא ריק.
* `cup_min_weight_g`: משקל מינימלי לזיהוי כוסית/ידית ריקה (למשל `80.0g` לכוסית שקילה, או `~350.0g–450.0g` לידית פורטפילטר).
* `cup_max_weight_g`: משקל מקסימלי מותר לפני טחינה (למשל `empty_weight + 15g` — מונע טחינה כפולה לתוך כוסית/ידית שכבר מלאה בקפה).
* `max_grind_time_s`: זמן מקסימלי לטחינה (ברירת מחדל `25s`).

### לוגיקת הביצוע לפני `TARING` (כאשר `require_cup == true`)
1. **דגימה מיוצבת (Settled Raw Weight):** ממוצע של 5 דגימות רצופות מה-HX711 יחסית ל-`empty_holder_raw` (לפני איפוס ה-Tare).
2. **בדיקת יציבות:** אם הסטייה בין הדגימות גדולה מ-`1.0g`, ממתינים עד 2 שניות ואז מבטלים עם `error: scale_unstable`.
3. **בדיקת נוכחות כוסית/ידית:**
   * משקל נמוך מ-`cup_min_weight_g` ← ביטול מיידי ופרסום `{"event": "error", "reason": "no_cup_detected"}`.
   * משקל גבוה מ-`cup_max_weight_g` ← ביטול מיידי ופרסום `{"event": "error", "reason": "cup_already_full"}`.
4. **הגנת "אין פולים" / תקיעה בזמן טחינה (No-Flow Watchdog):**
   * במצב `PREDICTIVE`, אם עברו 3 שניות מתחילת הטחינה והמשקל עלה בפחות מ-`0.5g` ← כיבוי מנוע מיידי ופרסום `{"event": "error", "reason": "no_beans_or_jam"}`.
   * בכל מצב, חריגה מ-`max_grind_time_s` מכבה את המנוע מיד ומדווחת `error: timeout`.

---

## 7. דוגמת אוטומציה ב-Home Assistant (07:00 בבוקר — 10 גרם קפה)

```yaml
alias: "Morning Coffee - Auto Grind 10g"
description: "Turns on grinder via Shelly at 07:00 and grinds 10g with cup/portafilter interlock"
mode: single

triggers:
  - trigger: time
    at: "07:00:00"

actions:
  # 1. הדלקת המטחנה דרך ה-Shelly
  - action: switch.turn_on
    target:
      entity_id: switch.grinder_shelly

  # 2. המתנה שה-ESP32 יתחבר ל-Wi-Fi וידווח online ב-MQTT (עד 15 שניות)
  - wait_for_trigger:
      - trigger: mqtt
        topic: "smart_gbw/grinder_01/status"
        payload: "online"
    timeout:
      seconds: 15
    continue_on_timeout: false

  # 3. השהיה קצרה להתייצבות חיישן המשקל אחרי עליית מתח
  - delay:
      seconds: 2

  # 4. שליחת פקודת טחינה של 10 גרם עם אימות כוסית/ידית
  - action: mqtt.publish
    data:
      topic: "smart_gbw/grinder_01/cmd"
      payload: '{"action": "grind", "target_weight": 10.0, "require_cup": true}'
      qos: 1
      retain: false

  # 5. המתנה לתוצאה (סיום טחינה או שגיאה) והתראה במקרה של תקלה
  - wait_for_trigger:
      - trigger: mqtt
        topic: "smart_gbw/grinder_01/event"
        id: "grinder_event"
      - trigger: mqtt
        topic: "smart_gbw/grinder_01/shot"
        id: "grinder_done"
    timeout:
      seconds: 40

  - choose:
      - conditions:
          - condition: template
            value_template: "{{ wait.trigger is not none and wait.trigger.id == 'grinder_event' }}"
        sequence:
          - action: notify.mobile_app_iphone
            data:
              title: "הטחינה האוטומטית בוטלה"
              message: >
                סיבה: {{ (wait.trigger.payload | from_json).reason }}
```

---

## 8. תמיכה בטחינה ישירה לידית (Portafilter Mod & 3D Hardware)

כברירת מחדל הריפו כולל רק מחזיק לכוסית שקילה (`Cup holder`). לטחינה ישירה לידית פורטפילטר (58 מ"מ / 54 מ"מ), מחליפים **רק את חלק ה-Cup holder** במתאם פורטפילטר מהקהילה (שאר החלקים — `Screen adapter`, `Back plate`, `Cover plate` — נשארים ללא שינוי).

### חומרת חיישן המשקל
* **חיישן T70 1kg:** זהו החיישן המומלץ רשמית בריפו עבור טחינה לידית פורטפילטר.

### מודלים מוכנים להורדה (Community 3D Designs – Printables)
1. **מומלץ (שימוש חוזר במזלג המתכת המקורי של ה-Eureka):**
   * [Eureka Mignon GBW mod Portafilter Holder (by Giovanni Oppizzi / goppizzi)](https://www.printables.com/model/1514218-eureka-mignon-gbw-mod-portafilter-holder)
   * **הרכבה:** המתאם מוברג לצד הצף של ה-Load Cell, ואליו מחברים את מזלג המתכת המקורי שפורק מה-Specialità.
   * **חומרה נדרשת:** 1× אינסרט הברגה (`M3x3` Heat-set insert) להברגת המזלג המקורי.
   * **רפרנס בקהילה:** [GitHub Discussion #101](https://github.com/jaapp/smart-grind-by-weight/discussions/101).
2. **חלופות מודפסות מלאות (כולל קבצי `.f3d` להתאמה אישית):**
   * **עם תמיכה לצוואר הידית (לידית נייקד):** [Portafilter Holder – Handle Rest](https://www.printables.com/model/1866834-eureka-mignon-specialita-portafilter-holder-for-sm)
   * **משולב לידית + נעילה לכוסית שקילה:** [Portafilter Holder – Cup Latch](https://www.printables.com/model/1866835-eureka-mignon-specialita-portafilter-holder-for-sm)

### דגשי הדפסה וכיול לידית כבדה ([Discussion #72](https://github.com/jaapp/smart-grind-by-weight/discussions/72))
* **חומר וקשיחות:** להדפיס את מתאם הידית ואת ה-`Back plate` ב-**PETG או ABS/ASA** עם לפחות 4–5 דפנות (Walls) ומילוי גבוה כדי למנוע גמישות ורעידות מהמשקל והמומנט של הידית.
* **טבעת (Dosing Funnel):** לוודא מרווח גובה מספק בין המזלג ליציאת הקפה אם משתמשים ב-Funnel מגנטי.
* **כיול Interlock:** לעדכן ב-NVS את `cup_min_weight_g` בהתאם למשקל הידית הריקה (לרוב ~350–450 גרם).

---

## 9. דרישות קומפילציה ומחיצות (`platformio.ini` ו-`partitions.csv`)

1. **מחיצות OTA:** לוודא שלפחות `3MB` מוקצים לכל מחיצת OTA (`app0` ו-`app1`) מתוך ה-16MB Flash.
2. **ספריות מומלצות:**
   * **MQTT:** `esp-mqtt` (מובנה ב-ESP-IDF, אסינכרוני) או `AsyncMqttClient` (לא `PubSubClient` החוסם).
   * **Provisioning:** `improv-wifi/Improv` (BLE).
   * **JSON:** `ArduinoJson`.

---

## 10. תוכנית עבודה מדורגת (Implementation Phases)

1. **Phase 1 — Baseline & Partitions:** Fork ל-`jaapp/smart-grind-by-weight`, קומפילציה ב-PlatformIO, ובדיקת `partitions.csv`.
2. **Phase 2 — Radio Manager & Improv BLE:** הוספת `network_manager.cpp`, שמירה ב-NVS, וכפתור "Wi-Fi Setup" במסך.
3. **Phase 3 — Async MQTT & HA Discovery:** Task על Core 0 ל-Mosquitto, Auto-Discovery, ו-`telemetryQueue`.
4. **Phase 4 — Remote Commands & Interlock:** הוספת `cmdQueue`, בדיקת משקל גולמי (כוסית/ידית), No-Flow Watchdog ובדיקת מאמץ (ניתוק ראוטר באמצע טחינה).
