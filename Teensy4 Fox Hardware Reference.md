Teensy4.1 Fox Controller Hardware Reference.  

is a concise **wiring summary** and a final **parts check** you can use as a "cheat sheet" when your Amazon orders arrive.

### 1. Final Wiring Summary

The most critical part of this build is ensuring the **Teensy** and **Baofeng** receive their specific voltages from your high-power **14.8V battery**.

| Connection Point | Source | Target | Voltage |
| --- | --- | --- | --- |
| **Main Power** | SOCOKIN 14.8V Battery | XT60 Fused Pigtail | **~14.8V - 16.8V** |
| **Radio Power** | Buck 1 (Output) | BL-5 Battery Eliminator | **Set to 8.0V** |
| **Logic Power** | Buck 2 (Output) | Teensy 4.1 (VIN Pin) | **Set to 5.0V** |
| **PTT Control** | Teensy Pin 2 | 2N2222 Transistor | **3.3V Logic** |
| **Audio Out** | Teensy Pin 12 | Radio Mic In (Pigtail) | **PWM/MQS** |
| **Battery Monitor** | Battery (+) via Divider | Teensy Pin A9 | **2.31V - 3.03V** |

### 2. Baofeng/Kenwood Pigtail Color Guide

When you snip your Kenwood 2-pin cable, you'll likely see these four colors. Always use a multimeter to verify:

* **White:** Common Ground (Connects to Teensy GND).
* **Black:** PTT (Connects to your transistor's collector).
* **Red:** Mic Audio In (Connects to your 10k potentiometer output).
* **Green:** Speaker Audio Out (Optional: Connect to Teensy for "listening" features).

### 3. Battery Voltage Monitoring Circuit

**Critical Safety Feature:** Protects your 4S LiPo from over-discharge damage

**Purpose:**
- Monitor battery voltage in real-time
- Provide early warning when battery is getting low
- Force shutdown before cell damage occurs (< 3.2V per cell)

**Circuit Design:**

**Voltage Divider:**
- **R1:** 10kΩ resistor (between Battery+ and Pin A9)
- **R2:** 2.2kΩ resistor (between Pin A9 and Ground)
- **C1:** 100nF ceramic capacitor (parallel with R2, noise filtering)

**Connection Points:**
1. Battery positive terminal → 10kΩ resistor (R1)
2. 10kΩ resistor (R1) → Teensy Pin A9
3. Pin A9 → 2.2kΩ resistor (R2)
4. 2.2kΩ resistor (R2) → Common Ground
5. 100nF capacitor (C1) → Parallel across 2.2kΩ resistor

**Voltage Scaling:**
- Divider Ratio: 2.2kΩ / (10kΩ + 2.2kΩ) = 0.1803
- Fully Charged (16.8V) → Pin A9 sees 3.03V
- Nominal (14.8V) → Pin A9 sees 2.67V
- Low Battery (13.6V) → Pin A9 sees 2.45V ⚠️ Warning
- Critical (12.8V) → Pin A9 sees 2.31V 🛑 Shutdown

**Safety Thresholds:**
- **Soft Warning:** < 13.6V (3.4V per cell) - Play "LOW BATTERY" alert
- **Hard Shutdown:** < 12.8V (3.2V per cell) - Disable PTT permanently
- **Critical Damage:** < 12.0V (3.0V per cell) - LiPo cells may be permanently damaged

**Component Selection:**
- Use ±1% tolerance resistors for accurate voltage measurement
- 1/4W resistors are sufficient (power dissipation < 0.03W)
- Ceramic capacitor for noise filtering (X7R or C0G dielectric)

**Calibration:**
1. Assemble voltage divider circuit
2. Connect fully charged battery (16.8V)
3. Measure actual voltage at Pin A9 with multimeter
4. Calculate ratio: `actual_ratio = 16.8V / measured_voltage`
5. Update `VOLTAGE_DIVIDER_RATIO` constant in code
6. Test at multiple voltage levels to verify accuracy

**Wiring Notes:**
- Keep voltage divider wiring short to minimize noise pickup
- Route away from high-current paths (radio power, buck converters)
- Consider using shielded wire if mounted near RF transmitter
- Double-check polarity - reversed polarity will damage Teensy ADC

### 4. Quick Hardware Checklist

Before you head out for the hunt, make sure these are secured inside your Ammo Can:

* [ ] **Main 5A Fuse:** Swapped into the XT60 holder.
* [ ] **XT60 Y-Splitter:** Securely connected to both LM2596 buck converters.
* [ ] **10uF Capacitor & 10k Pot:** Inline with your audio wire to prevent "clipping" and radio damage.
* [ ] **2N2222 Transistor & 1k Resistor:** Mounted to a board to keep the PTT signal clean.
* [ ] **Battery Monitor Circuit:** 10kΩ + 2.2kΩ voltage divider with 100nF filter cap installed.
* [ ] **Calibration Complete:** Battery voltage readings verified at 16.8V, 14.8V, and 13.6V.

### 5. Low Pass Audio Filter for MQS

To get **acceptable quality audio** from the Teensy 4.1's MQS (Medium Quality Sound) pins to your Baofeng, the "bare minimum" wiring works, but it can sound "tinny" or distorted because MQS is essentially a high-frequency PWM signal.

To make it sound professional and prevent the radio from "clipping," I recommend adding a simple **Low-Pass Filter**. This smooths out the digital pulses into a clean analog wave.

#### 5.1. The Necessary "Filter" Components

You likely have the capacitor and pot on your list, but adding one small resistor makes a world of difference for MQS.

* **10uF Electrolytic Capacitor**: This blocks "DC Bias" from the Teensy, ensuring you only send the audio signal to the radio.
* **10k Ohm Linear Potentiometer**: This acts as your **Master Volume**. Baofeng mic inputs are very sensitive; without this, the audio will be extremely loud and distorted.
* **1k Ohm Resistor**: Adding this between Pin 12 and the capacitor creates a basic **Low-Pass Filter** that removes the digital "hiss" characteristic of MQS.

#### 5.2. The "Acceptable Quality" Circuit

For a clean signal, wire them in this specific order:

1. **Teensy Pin 12** → **1k Resistor**.
2. **1k Resistor** → **10uF Capacitor** (Positive leg).
3. **10uF Capacitor** (Negative leg) → **Left Pin of 10k Potentiometer**.
4. **Center Pin of 10k Potentiometer** → **Radio Mic Input (Red Wire)**.
5. **Right Pin of 10k Potentiometer** → **Common Ground**.

#### 5.3. Optional: Audio Isolation (The "Pro" Fix)

If you hear a "hum" or "buzz" whenever the radio transmits (common with high-power LiPos), it’s caused by a ground loop. An isolation transformer is the magic fix for this.

* **600:600 Ohm Audio Isolation Transformer**: If you find the audio has a constant background buzz, put this small transformer between your circuit and the radio. It physically separates the Teensy's electrical ground from the Radio's electrical ground.

## Summary Recommendation

If you want to keep it simple for next weekend: **The Resistor, Capacitor, and Potentiometer** are all you need for "acceptable" audio. The **1k Resistor** is the secret ingredient that turns MQS from "harsh digital noise" into "smooth radio audio."
