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
| **Audio Out** | Teensy Pin 12 | Radio Mic In (Pigtail | **PWM/MQS** |

### 2. Baofeng/Kenwood Pigtail Color Guide

When you snip your Kenwood 2-pin cable, you'll likely see these four colors. Always use a multimeter to verify:

* **White:** Common Ground (Connects to Teensy GND).
* **Black:** PTT (Connects to your transistor's collector).
* **Red:** Mic Audio In (Connects to your 10k potentiometer output).
* **Green:** Speaker Audio Out (Optional: Connect to Teensy for "listening" features).

### 3. Quick Hardware Checklist

Before you head out for the hunt, make sure these are secured inside your Ammo Can:

* [ ] **Main 5A Fuse:** Swapped into the XT60 holder.
* [ ] **XT60 Y-Splitter:** Securely connected to both LM2596 buck converters.
* [ ] **10uF Capacitor & 10k Pot:** Inline with your audio wire to prevent "clipping" and radio damage.
* [ ] **2N2222 Transistor & 1k Resistor:** Mounted to a board to keep the PTT signal clean.

Good luck with your build and the fox hunt! If you're looking for more ways to customize the setup, I can help you find **external antenna mounts** or **bulkhead connectors** to run your antenna through the side of the ammo can. Would you like me to do that?