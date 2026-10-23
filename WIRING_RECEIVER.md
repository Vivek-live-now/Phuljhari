# 🔌 Phuljhari Receiver Hardware & Wiring Guide

> **Tagline:** Tiny sparks. Wireless smiles. ✨  
> **Target Board:** ESP32-WROOM-32D (30-pin or 38-pin Development Board)  
> **Safety Boundary:** Exclusively for low-voltage decorative LED lighting (≤ 5V DC).  
> **Strict Rule:** DO NOT connect pyrotechnic igniters, heating coils, or explosive demonstration devices.

---

## 🛠️ Testing Configurations

### Option 1: Zero-Hardware Bench Testing (Onboard LED)
If you do not have an external LED strip on your bench, you can immediately test the receiver using the ESP32's built-in indicator LED:

* **Default Pin:** `GPIO 2` (configured via `PHULJHARI_CONFIG_ONBOARD_LED_PIN` in [`receiver/include/receiver_config.h`](file:///data/data/com.termux/files/home/phuljhari/receiver/include/receiver_config.h)).
* **Active Logic Level:** Active HIGH (`PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH = true`).
* **Visual Status Rhythms:**
  * **Safe Idle:** Soft single pulse (60 ms ON every 2,000 ms) indicates receiver is alive and awaiting commands.
  * **✨ Phuljhari Effect:** Fast festive shimmer (toggles rapidly every 50 ms).
  * **🎆 Anar Effect:** Ascending fountain cadence (100 ms pulse every 250 ms).
  * **💥 Patakha Effect:** Triple staccato pop flashes.
  * **Cooldown / Error:** LED is completely OFF (safe inactive default).

> [!NOTE]  
> If your specific dev board clone maps its onboard LED to a different pin (e.g. GPIO 4 or 16), or is active-low, update `PHULJHARI_CONFIG_ONBOARD_LED_PIN` and `PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH` in `receiver_config.h`.

---

### Option 2: Addressable RGB LED Strip (WS2812B / SK6812)

Connect a 5V WS2812B addressable LED strip (e.g. 30 LEDs) directly to the ESP32-WROOM-32D development board:

```
+-----------------------------------------------------------+
|                   ESP32-WROOM-32D                         |
|                                                           |
|     [VIN / 5V] ------------------------> Strip +5V (Red)  |
|                                                           |
|     [GND]      ------------------------> Strip GND (Black)|
|                                                           |
|     [GPIO 18]  ---[ 330Ω Resistor ]---> Strip DIN (Data)  |
+-----------------------------------------------------------+
```

### Pinout Table

| ESP32-WROOM-32D Pin | WS2812B Strip Connection | Description | Wire Color (Typical) |
| :--- | :--- | :--- | :--- |
| **VIN / 5V** | **+5V / VCC** | 5V DC power from USB | Red |
| **GND** | **GND** | Common Ground | Black / White |
| **GPIO 18** | **DIN (Data In)** | High-speed data line (via 330Ω - 470Ω resistor) | Green / Yellow |

### Best Practices & Hardware Protection
1. **Current Budget & USB Safety:**
   * 30 WS2812B LEDs at full white draw up to ~1.8A.
   * Project Phuljhari firmware enforces a **strict software brightness cap** of `180 / 255` (~70% maximum power) in [`shared/phuljhari_safety.h`](file:///data/data/com.termux/files/home/phuljhari/shared/phuljhari_safety.h).
   * Average current during Phuljhari, Anar, and Patakha animations remains below `500 mA`, safely powered from a standard USB host or 5V power adapter.
2. **Data Line Resistor:**
   * Placing a `330Ω` to `470Ω` resistor between `GPIO 18` and the strip's `DIN` prevents signal reflections and protects the ESP32 GPIO.
3. **Power Capacitor:**
   * Placing a `100 µF` to `1000 µF` (6.3V or higher) electrolytic capacitor across +5V and GND near the strip helps prevent USB brownouts during sudden brightness surges.
4. **Data Direction:**
   * Ensure you connect to the **DIN** (Data IN) end of the LED strip, indicated by the arrows printed on the strip PCB pointing away from the input connector.

---

## ⚡ Safety Enforcement Summary
* All outputs default to `LOW` / inactive on boot.
* Built-in hardware watchdog automatically cuts off LED output if communication is lost for more than 20 seconds.
* STOP command immediately clears all LEDs to black.
