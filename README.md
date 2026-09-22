# Containerized Multi-Protocol HIL Integrity Framework

[![Python](https://img.shields.io/badge/Python-3.11%2B-blue.svg)](https://www.python.org/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-red.svg)](https://www.espressif.com/)
[![Pytest](https://img.shields.io/badge/Pytest-Automated%20HIL-green.svg)](https://docs.pytest.org/)
[![License](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

A high-reliability, automated **Hardware-in-the-Loop (HIL) Test Framework** built for embedded systems validation, digital signal processing (DSP) verification, and physical layer signal integrity analysis. Designed specifically for industrial-grade hardware quality assurance, this framework establishes a real-time validation bridge between a **Hardware Test Platform (HTP)** (Raspberry Pi 3B+) and a **Device Under Test (DUT)** (ESP32-S3 microcontroller) across **UART, SPI, and I2S/PCM** communication protocols.

---

## System Architecture Overview

The system architecture decouples testing into two functional planes: a **Control Plane** for non-intrusive serial telemetry reporting and a **Data Plane** for high-speed hardware bus testing.

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

## What Was Tested & Why (Technical Deep-Dive)

### 1. Control Plane: UART Telemetry Parsing
* **Objective**: Establish non-intrusive test control, frame synchronization, and real-time metric reporting.
* **Mechanism**: The DUT firmware executes a FreeRTOS background task (`i2s_dsp_processing_task`) that converts raw DMA buffer measurements into formatted ASCII strings over UART (`/dev/ttyACM0` @ 115200 baud):
  ```text
  I2S_TELEMETRY: Samples: 1024, Peak: 15000, RMS: 10605, Freq: 1000 Hz
  ```
* **Validation**: Pytest uses regular expressions (`re.search`) to extract numerical metrics asynchronously without interrupting the DUT hardware clock.

### 2. Data Plane 1: SPI Full-Duplex Bus Integrity
* **Objective**: Verify high-speed synchronous payload transmission without bit corruption or shift register drift.
* **Mechanism**: The Raspberry Pi SPI controller (`/dev/spidev0.0`) exchanges multi-byte headers and payloads in full-duplex Mode 0 (CPOL=0, CPHA=0) with the ESP32 SPI Slave driver.
* **Validation**: Bitwise array comparison ensures 100% data fidelity between MOSI and MISO buffers.

### 3. Data Plane 2: I2S/PCM Audio DSP Pipeline
* **Objective**: Validate digital audio stream integrity, frame alignment, and real-time signal properties (frequency, peak, RMS power).
* **Linux ALSA Architecture**:
  * **Plugin Hardware Layer (`plughw:2,0`)**: Converts 16-bit mono WAV files generated dynamically in `/tmp/` into 32-bit Philips I2S slots required by the BCM2835 SoC overlay.
  * **Asynchronous Process Execution**: Pytest initiates `aplay` using non-blocking `subprocess.Popen()`. This ensures the ALSA driver actively drives the I2S clocks (BCLK/WS) while Python reads UART serial output concurrently.
* **ESP32-S3 Firmware & DSP Analysis**:
  * **ESP-IDF v5.x Channel Driver**: Configures I2S0 in `I2S_ROLE_SLAVE` mode with 32-bit slot width (`I2S_DATA_BIT_WIDTH_32BIT`) via `driver/i2s_std.h`.
  * **MSB Sample Extraction**: Extracts the true 16-bit PCM payload from the 32-bit I2S slot: `int16_t sample = (int16_t)(dma_buffer[i] >> 16)`.
  * **Hysteresis Zero-Crossing DSP Filter**: Implements a Schmitt Trigger state machine ($\text{Threshold} = \pm 1000$) to eliminate false zero-crossings caused by breadboard noise. Calculates fundamental frequency:
    $$\text{Frequency (Hz)} = \frac{\text{zero\_crossings}}{2 \cdot \text{duration\_sec}}$$
  * **RMS Signal Power**: Verifies harmonic signal purity against theoretical ideal sine wave power ratios:
    $$\frac{\text{RMS}}{\text{Peak}} \approx \frac{1}{\sqrt{2}} \approx 0.7071$$

---

## WARNING: Physical Layer (Layer 1) & Signal Integrity

> [!WARNING]
> ### PHYSICAL LAYER WIRING & SIGNAL INTEGRITY CONSTRAINTS
> **Digital Audio I2S operates at high clock frequencies (BCLK $\approx 2.8224 \text{ MHz}$). At these speeds, physical wiring quality directly dictates test repeatability.**
>
> During development and validation, loose breadboard jumper wires (Dupont cables) introduced **contact bounce, floating ground noise, and parasitic clock glitches**. This caused the ESP32 zero-crossing detector to measure erroneous frequencies (~9 kHz – 11 kHz instead of 1 kHz).
>
> **Mandatory Hardware Assembly Guidelines:**
> 1. **Common Ground (GND) Integrity**: Connect a dedicated, short, high-gauge Ground (GND) wire directly between the Raspberry Pi header and the ESP32 board. A "floating ground" creates voltage reference drift and corrupts digital logic levels.
> 2. **BCLK & DIN Separation**: Do NOT twist or tightly bundle the BCLK (GPIO 4) and DIN (GPIO 6) wires together. High-frequency switching on BCLK induces crosstalk (parasitic capacitive coupling) onto the data line.
> 3. **Header Pin Seating**: Ensure jumper cables fit tightly onto header pins. If loose, replace with fresh female-to-female jumper cables or a dedicated expansion board.
> 4. **Cable Length**: Keep all signal wires (BCLK, WS, DIN) shorter than **15 cm (6 inches)** to prevent antenna reflection effects and inductive ringing.

---

## Requirements & Test Specification Matrix

The framework maintains strict traceability between System Requirements (SRS), Test Specifications, and Pytest code modules:

| Test Case ID | Name / Description | Type | Target State / Expected Behavior | Status |
| :--- | :--- | :--- | :--- | :--- |
| **TC-F-01** | I2S PCM Audio Transmission Sanity Check | Functional | Lock BCLK/WS clock & capture PCM frames. Target returns `I2S_TELEMETRY` with Peak > 500 LSB. | `[Implemented v1.0]` |
| **TC-F-02** | Multi-Frequency DSP Accuracy Sweep | Functional | Sweep 1000 Hz & 2500 Hz audio tones. Target measures fundamental frequency within +/- 100 Hz. | `[Implemented v1.0]` |
| **TC-R-01** | Signal Quality & RMS Harmonic Ratio | Robustness | Verify pure sine wave RMS/Peak ratio ($0.55 \le R \le 0.85$). Assert no harmonic distortion. | `[Implemented v1.0]` |
| **TC-R-02** | SPI Full-Duplex Loopback Integrity | Robustness | Transmit 128-byte payload on MOSI/MISO in Mode 0. Assert 100% bitwise match without corruption. | `[Implemented v1.0]` |
| **TC-S-01** | Physical Layer Signal Integrity & Noise | Stress | Validate Schmitt Trigger hysteresis filter under contact bounce. Assert noise (< 1000 LSB) rejected. | `[Implemented v1.0]` |

---

## Future Extension Roadmap: Logic Analyzer & Oscilloscope Integration

To evolve this HIL rig from functional software testing into full physical-layer compliance testing, the framework is designed to integrate external hardware measurement instrumentation:

```text
+-----------------------------------------------------------------------------------+
|                        HARDWARE INSTRUMENTATION EXTENSION                         |
|                                                                                   |
|   +-----------------------+                    +------------------------------+   |
|   |  Pico Logic Analyzer  |                    |  Digital Storage Oscilloscope|   |
|   |  (8-Ch 24MHz / Pico)  |                    |  (Siglent SDS1000X-E Series) |   |
|   +-----------+-----------+                    +--------------+---------------+   |
|               |                                               |                   |
|               | (Protocol Decoding: SPI / I2S)                | (Analog Measurement)|
|               v                                               v                   |
|  [ Automated Protocol Timing Validation ]      [ Physical Signal Integrity ]      |
|  - Setup/Hold Time Checks ($t_{su}, t_h$)      - BCLK Rise/Fall Time ($t_r, t_f$)  |
|  - WS Duty Cycle Verification (50% ± 2%)       - Clock Jitter & Overshoot Analysis |
+-----------------------------------------------------------------------------------+
```

### Planned Hardware Integrations
1. **Pico Logic Analyzer (sigrok / PulseView CLI)**:
   * Automated digital protocol decoding via `sigrok-cli`.
   * Automated assertion of SPI setup/hold times ($t_{su}, t_h$) and I2S frame sync alignment ($WS$ edge relative to $BCLK$).
2. **Digital Storage Oscilloscope (PyVISA SCPI Integration)**:
   * Automated SCPI command control over USB/Ethernet using `PyVISA`.
   * Real-time measurement of BCLK rise time ($t_r$), fall time ($t_f$), peak-to-peak voltage ($V_{pp}$), and clock jitter under high thermal/electrical loads.

---

## How to Build & Run

### 1. Build and Flash ESP32 Firmware
```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py flash
```

### 2. Execute Automated HIL Pytest Suite
```bash
# Run full HIL test suite with verbose output
pytest -s -v tests/
```

### 3. Build Sphinx Documentation (Docs-as-Code)
```bash
cd docs
make html
# View generated HTML documentation in docs/_build/html/index.html
```

---

## Repository Directory Structure

```text
hil_integrity_framework/
├── docs/                             # Sphinx Docs-as-Code Architecture & Specifications
│   ├── architecture.md               # System & ALSA Kernel Architecture Specification
│   ├── api_specification.md          # UART Telemetry & Audio Stimulus API Spec
│   └── test_specification.md         # Requirements Traceability Matrix & Test Cases
├── firmware/                         # ESP32-S3 Firmware (ESP-IDF v5.x)
│   ├── main/
│   │   ├── main.c                    # I2S Slave, Hysteresis DSP & UART Telemetry
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt
├── tests/                            # Pytest HIL Test Suites
│   ├── conftest.py                   # Serial & ALSA Fixtures
│   ├── i2s/
│   │   └── test_i2s_integrity.py     # I2S Sanity, Frequency Sweep & Quality Tests
│   └── spi/
│       └── test_spi_integrity.py     # SPI Loopback Integrity Tests
├── pytest.ini                        # Pytest configuration & markers
└── README.md                         # Master Framework Documentation
```

---

## Author & License

* **Author**: Pero Smiljanić | Staff Software Quality Engineer
* **License**: Open Source under the MIT License.