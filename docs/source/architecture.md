# hil-integrity-framework - System Architecture (v1.0.0)

## 1. Overview

The HIL Integrity Framework is a localized, automated Hardware-in-the-Loop (HIL) test execution and protocol validation suite. It is engineered to validate multi-protocol communication stability, digital signal processing (DSP) algorithms, and physical layer signal integrity across embedded systems.

By establishing a real-time validation bridge between a Linux host controller and a microcontroller target, this framework bridges the gap between high-level automated test software and bare-metal hardware execution.

---

## 2. Hardware Topology

The HIL system consists of two primary physical nodes:

* **Device Under Test (DUT):** An ESP32-S3 microcontroller running custom C firmware compiled under ESP-IDF v5.x. It acts as an I2S Slave Receiver and SPI Slave, processing high-speed digital signals in real-time.
* **Hardware Test Platform (HTP):** A Raspberry Pi 3B+ (or PC) executing automated test scripts under the pytest framework. It acts as the signal generator, ALSA audio driver controller, and test runner.

### Frequencies and Connections

* **Control/Debug Plane (UART):** Serial connection via USB-CDC (`/dev/ttyACM0` @ 115200 baud) linking the RPi directly to the ESP32-S3 for non-intrusive log extraction, DSP telemetry collection, and hardware state monitoring.
* **Data Plane 1 (I2S/PCM Audio Bus):** 3-wire digital audio bus linking RPi (Master) and ESP32-S3 (Slave Receiver):
  * **BCLK (Bit Clock):** GPIO 4 operating at ~2.8224 MHz (32-bit slot width @ 44.1 kHz sample rate).
  * **WS (Word Select / Frame Sync):** GPIO 5 operating at 44.1 kHz frame clock.
  * **DIN (Data In):** GPIO 6 carrying 16-bit MSB-aligned PCM audio payload.
* **Data Plane 2 (SPI Bus):** Full-duplex synchronous bus (`/dev/spidev0.0`) operating up to 10 MHz for register verification and 128-byte payload loopback integrity.

```text
       +-------------------------------------------------+
       |         Hardware Test Platform (HTP)            |
       |             Raspberry Pi 3B+                    |
       |  +-------------------------------------------+  |
       |  |  Pytest Automation Engine                 |  |
       |  |  ALSA Audio Generator (`plughw:2,0`)      |  |
       |  +-------------------------------------------+  |
       +-----------------------+-------------------------+
                               |
            Data Plane         | Control/Debug Plane
            I2S / SPI Buses    | UART via USB (115200)
            (BCLK, WS, DIN)    | (Logs & Telemetry)
                               |
       +-----------------------v-------------------------+
       |             Device Under Test (DUT)             |
       |                 ESP32-S3 MCU                    |
       |  +-------------------------------------------+  |
       |  |  ESP-IDF v5.x I2S/SPI Slave Drivers       |  |
       |  |  FreeRTOS DSP Schmitt-Trigger Task        |  |
       |  +-------------------------------------------+  |
       +-------------------------------------------------+
```

---

## 3. Protocol Architecture & DSP Pipeline

The framework implements an end-to-end signal processing pipeline across the Linux kernel and FreeRTOS firmware:

### Linux ALSA Subsystem Integration
* Utilizes ALSA `plughw:2,0` plugin layer to convert 16-bit mono WAV files dynamically into 32-bit Philips I2S frame slots required by the BCM2835 SoC hardware overlay.
* Employs non-blocking `subprocess.Popen()` execution to keep hardware clock lines active while Pytest reads UART telemetry concurrently.

### ESP32-S3 Firmware DSP Engine
* **DMA Ring Buffer:** 8 descriptors × 1023 frames in 32-bit slot mode via `driver/i2s_std.h`.
* **16-Bit MSB Sample Extraction:**
  $$\text{sample} = (\text{int16\_t})(\text{dma\_buffer}[i] \gg 16)$$
* **Schmitt Trigger Hysteresis Filter:** Implements dual-threshold state machine ($\text{Threshold} = \pm 1000$) to eliminate contact bounce and jumper noise.
* **Real-time Metrics:** Calculates Peak Amplitude, Root Mean Square (RMS) power, and Zero-Crossing Frequency:
  $$f = \frac{\text{zero\_crossings}}{2 \cdot \text{duration}}$$

---

## 4. Software Design Patterns

* **Asynchronous Process Execution:** Separates stimulus generation (`aplay`) from response validation (serial read) to prevent I2S clock teardown during test assertions.
* **Hysteresis Noise Suppression:** Hardware-agnostic DSP filtering inside firmware ensuring test stability despite physical breadboard noise.
* **Traceability-First Test Organization:** Direct mapping between System Requirements (SRS), Test Specifications (`TC-*`), and Pytest test cases.