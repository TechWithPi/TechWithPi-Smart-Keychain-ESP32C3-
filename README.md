# TechWithPi-Smart-Keychain-ESP32C3-
A smart ESP32-C3 keychain featuring an OLED display, customizable images via smartphone, and deep sleep mode for extended battery life. Built by TechWithPi.
# 🔑 ESP32-C3 Robot Eye Keychain

A tiny smart keychain built on the **ESP32-C3 Super Mini** and a **0.96" SSD1306 OLED**. On power-up it shows **animated robot eyes**. Connect your phone to its WiFi, open a web page, take a photo or pick one from the gallery, and it appears on the OLED. No app needed. The photo is converted to 128x64 right inside your browser.

Made by [TechWithPi](https://www.youtube.com/@TechWithPi1)

<p align="center">
  <img src="images/01-hero.jpg" alt="Robot Eye Keychain" width="600">
</p>

---

## ✨ Features

### 👀 Robot eyes
- Natural blinking, double blinks and eyes that look around
- 5 moods: **Angry**, **Funny** (happy and wacky), **Love**, **Sleepy**, **Sad**
- **Auto** mode switches moods randomly, or lock one mood from the web page

### 📸 Photo to OLED from your phone
- Take a photo with the camera or choose one from the gallery
- Converted to a **128x64 1-bit bitmap in the browser** (no software, no app)
- Adjustable contrast, brightness, dithering, invert, and fit (cover / contain / stretch)
- Live preview before sending

### 💾 Photo saved in flash
The photo stays after a power cut or battery removal. Robot eyes come back only when you trigger a reset (see Button below) or press **Robot Eyes** on the web page.

### 🎬 Photo animations (picked from the web page)
| Option | What it does |
|---|---|
| Static | No animation |
| Blink | Eyelids close and open over the photo |
| Slide | Photo leaves on one side and comes back from the other |
| Zoom | Photo grows from small to full size |
| Auto | Cycles through Blink, Slide and Zoom |

An animation plays every **5 seconds**. All choices (photo animation, robot face) are saved in flash.

### 🔘 One button, three actions (GPIO3)
| Action | Result |
|---|---|
| Press once | Wakes the OLED and restarts the 2 minute timer |
| Press 3 times quickly (within 700 ms) | **Hard reset**, comes back with Robot Eyes (saved photo is cleared) |
| Long press (1.5 s) | Wakes from Standby (deep sleep). A short tap in Standby is ignored, so it will not wake by accident in a pocket |

### 🔋 Battery saving
- OLED **turns off by itself after 2 minutes** of no activity
- **Screen OFF / ON** from the web page (WiFi stays on)
- **Standby (deep sleep)** from the web page: OLED and WiFi off, lowest power use
- CPU runs at **80 MHz** to reduce heat and current
- While charging with a TP4056, use Standby so the charger sees the correct load

### 🛡 Built for long runs
- Timing is rollover-safe (keeps working past 49 days of uptime)
- Free-memory watchdog triggers a clean hard reset if memory ever gets too low
- Triple press does a real chip reboot (deep sleep cycle) that clears WiFi and I2C state, not just a software restart

---

## 📸 Photos

| | |
|---|---|
| <img src="images/01-hero.jpg" alt="Finished keychain" width="320"><br>**Finished keychain** | <img src="images/02-wiring.jpg" alt="Wiring" width="320"><br>**Wiring** |
| <img src="images/03-web-ui.png" alt="Web page" width="320"><br>**Web page on phone** | <img src="images/04-photo-on-oled.jpg" alt="Photo on OLED" width="320"><br>**Photo on the OLED** |

> Add your 4 photos in an `images/` folder with these exact names: `01-hero.jpg`, `02-wiring.jpg`, `03-web-ui.png`, `04-photo-on-oled.jpg`.

---

## 🧰 Components

| Part | Notes |
|---|---|
| ESP32-C3 Super Mini | Main board |
| 0.96" SSD1306 OLED, 128x64, I2C | Address `0x3C` |
| Push button | Any tactile button |
| TP4056 charging module | For Li-Po charging |
| 3.7 V Li-Po battery | Example: 250 mAh |

## 🔌 Wiring

| Part | Pin | ESP32-C3 |
|---|---|---|
| OLED | SDA | GPIO8 |
| OLED | SCL | GPIO9 |
| OLED | VCC | 3V3 |
| OLED | GND | GND |
| Button | one leg | GPIO3 |
| Button | other leg | GND |

No resistor is needed for the button, the code uses the internal pull-up. Do not connect GPIO3 straight to GND without a button.

> Tip: if Standby does not wake reliably with your board, add a 10 kΩ resistor between GPIO3 and 3V3.

**TP4056:** the default charge current is 1 A, which is too high for a small battery. Change the Rprog resistor (about 10 kΩ gives roughly 130 mA) to suit your battery.

---

## 🚀 Setup

### 1. Install libraries (Arduino Library Manager)
- **Adafruit GFX Library**
- **Adafruit SSD1306**

`WiFi`, `WebServer` and `Preferences` come with the ESP32 Arduino core.

### 2. Arduino IDE settings
- Board: **ESP32C3 Dev Module**
- USB CDC On Boot: **Enabled**

### 3. Upload
Open `keychain_eye.ino`, select your port and upload.

---

## 📱 How to use

1. Power on the keychain. Robot eyes start.
2. On your phone, connect to WiFi **`KeyChain-Eye`** (password: `12345678`).
3. Open **http://192.168.4.1** in your browser. If the page does not open, turn off mobile data and try again.
4. Choose **Camera** or **Gallery**, adjust the settings, and press **Send to OLED**.
5. Pick a **Photo animation** and a **Robot face animation** on the same page.
6. Use the **Power** section for Screen OFF / ON and Standby.

> You can change the WiFi name and password at the top of the code (`AP_SSID`, `AP_PASS`). The password must be at least 8 characters.

---

## ⚙️ Settings you can change in the code

| Setting | Default | Meaning |
|---|---|---|
| `AUTO_OFF_MS` | 120000 | OLED auto-off time (2 minutes) |
| `ANIM_GAP` | 5000 | Gap between photo animations |
| `LONG_PRESS_MS` | 800 | Press longer than this is not counted as a click |
| `WAKE_HOLD_MS` | 1500 | Hold time to wake from Standby |
| `MULTI_GAP_MS` | 700 | Maximum time window for the triple press |
| `I2C_HZ` | 800000 | Use 400000 if the OLED glitches |
| `CPU_MHZ` | 80 | Use 160 if animations feel slow |

---

## 🌐 Web endpoints

| Endpoint | Use |
|---|---|
| `GET /` | Web page |
| `POST /upload` | Send a 1024-byte bitmap (as 2048 hex characters) |
| `GET /eyes` | Back to Robot Eyes |
| `GET /anim?m=0..4` | Photo animation (static, blink, slide, zoom, auto) |
| `GET /face?m=0..7` | Robot face (auto, normal, angry, funny-happy, funny-wacky, love, sleepy, sad) |
| `GET /power?m=off\|on\|standby` | Screen OFF, ON, or Standby |

---

## 🛠 Troubleshooting

| Problem | Fix |
|---|---|
| OLED not found | Check SDA = GPIO8, SCL = GPIO9 and address `0x3C` |
| OLED shows noise or glitches | Set `I2C_HZ` to 400000 |
| Cannot connect to WiFi | Uncomment `WiFi.setTxPower(WIFI_POWER_8_5dBm);` in `setup()` |
| Page will not open | Turn off mobile data, forget and reconnect to `KeyChain-Eye` |
| Camera button opens the gallery | Browser dependent. Take the photo with your camera app and use Gallery |
| Standby does not wake | Long press for 1.5 s, or add a 10 kΩ resistor between GPIO3 and 3V3 |
| Charging is slow while the screen is on | Put the device in Standby while charging |

---

## 📝 Notes

- A live camera preview inside the page is not possible, because browsers need HTTPS for it and the ESP32 serves plain HTTP. The Camera button opens your phone's own camera, then the photo loads into the page.
- Standby turns WiFi off, so it cannot be turned back on from the web page. Use the button.
- Battery life depends on your board and battery. Measure your own with a USB power meter.

---

## 📺 Video

Watch the full build on YouTube: **[add link]**

## 🙌 Credits

Made by **TechWithPi**. Libraries: Adafruit GFX and Adafruit SSD1306.

If this helped, give the repo a ⭐ and subscribe to the channel.

## 📄 License

MIT License. Add a `LICENSE` file to your repo.
