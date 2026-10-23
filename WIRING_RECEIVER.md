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

### Best Practices & Realistic LED Power Budget
1. **Realistic Power Requirements & Limits:**
   * A single WS2812B / SK6812 RGB LED draws up to **60 mA** at full white (20 mA per color channel: Red, Green, Blue at 100% brightness).
   * **30 LEDs on full white draw up to 1.8 A (9 Watts)!**
   * **60 LEDs on full white draw up to 3.6 A (18 Watts)!**
   * Even with the firmware's software brightness cap of `180 / 255` (~70%), 30 fully illuminated white LEDs draw **1.26 A**, which exceeds standard PC USB 2.0 ports (500 mA) and can trigger USB overcurrent brownout resets.
   * **Why the default animations are safe on USB:**
     * *Phuljhari:* Mostly warm amber/gold (red + green channels, blue off, ~20-30% active LEDs at any given moment). Average draw: ~250–350 mA.
     * *Anar:* Sparse fountain stream (maximum 8 active particle sparks at any instant). Average draw: ~200–300 mA.
     * *Patakha:* Brief expanding wave with rapid decay. Peak burst ~450 mA for <100 ms.
   * **Hardware Recommendation:** If you expand the strip beyond 30 LEDs, or test custom full-white effects, **use an external 5V 2A–3A DC regulated power supply** connected directly to the strip's +5V and GND, ensuring the ESP32 ground and power supply ground are securely tied together.

2. **Strapping Pin Advisory for Onboard LED (GPIO 2):**
   * GPIO 2 is an internal boot strapping pin (`MTDO`) on the ESP32.
   * If held HIGH during power-up or reset by an external circuit or pull-up resistor, the ESP32 may fail to boot from flash memory and enter download mode.
   * The firmware configures GPIO 2 strictly as an output after bootloader initialization.
   * Do not attach pull-up resistors or high-voltage external lines to GPIO 2.

3. **Data Line Resistor:**
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
