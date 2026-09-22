# hil-integrity-framework - Test Specification (v1.0.0)

## 1. Overview

This document outlines the risk-based test specification for validating digital signal processing, serial communications, and physical layer signal integrity. The tests cover functional verification, robustness/error handling, signal purity, and physical layer stress scenarios.

---

## 2. Test Case Matrix

| Test Case ID | Name / Description | Type | Target State / Expected Behavior | Status |
| :--- | :--- | :--- | :--- | :--- |
| **TC-F-01** | I2S PCM Audio Transmission Sanity Check | Functional | Lock BCLK/WS clock & capture PCM frames. Target returns `I2S_TELEMETRY` with Peak > 500 LSB. | `[Implemented v1.0]` |
| **TC-F-02** | Multi-Frequency DSP Accuracy Sweep | Functional | Sweep 1000 Hz & 2500 Hz audio tones. Target measures fundamental frequency within +/- 100 Hz tolerance. | `[Implemented v1.0]` |
| **TC-R-01** | Signal Quality & RMS Harmonic Ratio | Robustness | Verify pure sine wave RMS/Peak ratio ($0.55 \le R \le 0.85$). Assert no harmonic distortion or clock bit drops. | `[Implemented v1.0]` |
| **TC-R-02** | SPI Full-Duplex Loopback Integrity | Robustness | Transmit 128-byte payload on MOSI/MISO in Mode 0. Assert 100% bitwise match without corruption. | `[Implemented v1.0]` |
| **TC-S-01** | Physical Layer Signal Integrity & Noise | Stress | Validate Schmitt Trigger hysteresis filter under contact bounce. Assert noise (< 1000 LSB) is rejected. | `[Implemented v1.0]` |

---

## 3. Detailed Test Design (v1.0.0 MVP Implementation)

### TC-F-01: I2S PCM Audio Transmission Sanity Check
* **Objective**: Ensure the ESP32-S3 I2S slave driver locks onto master BCLK/WS clocks and receives PCM audio frames without frame drops.
* **Pre-conditions**: ESP32-S3 is powered, UART connection active on `/dev/ttyACM0` @ 115200 baud, I2S bus wired (GPIO 4, 5, 6, GND).
* **Stimulus (Python Test-Runner)**:
  * Generate synthetic 16-bit PCM WAV file: $f = 1000\text{ Hz}$, $A = 15000\text{ LSB}$, duration = $1.5\text{ s}$.
  * Launch asynchronous audio playback via ALSA: `aplay -D plughw:2,0 -q /tmp/test_sanity_1khz.wav` via `subprocess.Popen()`.
  * Sample UART serial buffer concurrently for up to $3.0\text{ s}$.
* **Verifications**:
  * Read UART stream: Assert receipt of structured log matching `I2S_TELEMETRY: Samples: (\d+), Peak: (\d+)`.
  * Assert measured $\text{Peak} > 500\text{ LSB}$.

### TC-F-02: Multi-Frequency DSP Accuracy Sweep
* **Objective**: Validate ESP32-S3 DSP zero-crossing frequency measurement algorithm across multiple input frequencies.
* **Pre-conditions**: TC-F-01 passed.
* **Stimulus (Python Test-Runner)**:
  * Generate and play $1.5\text{ s}$ WAV file at $1000\text{ Hz}$ ($A = 15000\text{ LSB}$). Parse `I2S_TELEMETRY` log.
  * Generate and play $1.5\text{ s}$ WAV file at $2500\text{ Hz}$ ($A = 15000\text{ LSB}$). Parse `I2S_TELEMETRY` log.
* **Verifications**:
  * Read UART stream: Assert $|f_{measured} - 1000| \le 100\text{ Hz}$ for $1000\text{ Hz}$ tone.
  * Read UART stream: Assert $|f_{measured} - 2500| \le 100\text{ Hz}$ for $2500\text{ Hz}$ tone.

### TC-R-01: Signal Quality & RMS Harmonic Ratio
* **Objective**: Verify signal purity and RMS power calculation for pure sine waves.
* **Pre-conditions**: TC-F-01 passed.
* **Stimulus (Python Test-Runner)**:
  * Generate $1.5\text{ s}$ WAV file at $1000\text{ Hz}$ ($A = 20000\text{ LSB}$).
  * Play audio stimulus and capture `I2S_TELEMETRY` frame containing Peak and RMS metrics.
* **Verifications**:
  * Calculate experimental ratio:
    $$R = \frac{\text{RMS}}{\text{Peak}}$$
  * Assert $0.55 \le R \le 0.85$ (Theoretical ideal pure sine ratio $\approx 0.7071$).

### TC-R-02: SPI Full-Duplex Loopback Integrity
* **Objective**: Verify full-duplex SPI synchronous data transmission without bit shift or payload corruption.
* **Pre-conditions**: RPi `/dev/spidev0.0` connected to ESP32 SPI Slave driver.
* **Stimulus (Python Test-Runner)**:
  * Construct 128-byte pseudo-random test payload.
  * Execute full-duplex SPI transfer (`xfer2`) in SPI Mode 0 ($10\text{ MHz}$).
* **Verifications**:
  * Assert returned buffer matches transmitted buffer bit-for-bit.

### TC-S-01: Physical Layer Signal Integrity & Noise
* **Objective**: Validate firmware robustness against contact bounce and floating ground noise on lab jumper wires.
* **Pre-conditions**: ESP32 firmware running Schmitt Trigger Hysteresis DSP task ($\text{Threshold} = \pm 1000$).
* **Stimulus (Python Test-Runner)**:
  * Observe UART output during silence (no ALSA clock active) and during contact disturbance.
* **Verifications**:
  * Assert noise spikes ($< 1000\text{ LSB}$) do not trigger false zero-crossings or erroneous frequency calculations.