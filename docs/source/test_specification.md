# Test Specification & Traceability Matrix

## 1. System Requirements Specification (SRS) & Traceability Overview

This document defines the formal test specification and bidirectional traceability links between System Requirements, Test Specifications, and Pytest implementation modules in the `hil_integrity_framework`.

The framework validates an embedded system consisting of a Linux HIL Master (Raspberry Pi 3B+) and an Embedded Target (ESP32-S3 running ESP-IDF) across three hardware protocols: I2S PCM Audio, SPI Full-Duplex Slave, and Hardware UART (`/dev/ttyS0`).

```text
+------------------------------------+         +---------------------------------------+         +----------------------------------------+
|    System Requirement Specification|  <--->  |          Test Specification           |  <--->  |     Pytest Test Implementation Module  |
| (e.g., REQ-SYS-UART, REQ-SYS-I2S)  |         | (e.g., TC-UART-01, TC-I2S-SANITY-01)  |         |        (tests/*/test_*.py)             |
+------------------------------------+         +---------------------------------------+         +----------------------------------------+
```

---

## 2. Requirements & Test Traceability Matrix (9/9 Verified Tests)

| Requirement ID | Requirement Description | Test Case ID | Pytest Function / Module | Target Bus / Port | Verification Method |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-SYS-I2S-01** | The DUT shall lock onto I2S BCLK/WS clocks and capture PCM audio frames without DMA buffer drops. | **TC-I2S-01** | `test_i2s_pcm_audio_transmission` | I2S PCM Bus (GPIO 4, 5, 6) | ALSA Audio Stream & Peak Telemetry Capture |
| **REQ-SYS-I2S-02** | The DUT DSP task shall measure fundamental audio frequency within +/- 100 Hz accuracy across sweeps. | **TC-I2S-02** | `test_i2s_frequency_accuracy` | I2S PCM Bus (GPIO 4, 5, 6) | Zero-Crossing Telemetry Match (1000 Hz & 2500 Hz) |
| **REQ-SYS-SPI-01** | The SPI slave driver shall execute full-duplex DMA transactions and mirror 10-byte payloads cleanly. | **TC-SPI-01** | `test_spi_two_phase_echo_10bytes` | `/dev/spidev0.0` (SPI0) | Full-Duplex MOSI/MISO Byte Comparison |
| **REQ-SYS-UART-01** | The DUT shall respond to `UART_PING\n` with an explicit `UART_PONG\r\n` challenge response. | **TC-UART-01** | `test_uart_simple_echo` | `/dev/ttyS0` @ 115200 | Hardware Challenge-Response String Match |
| **REQ-SYS-UART-02** | The physical UART line shall be free of pre-transmission noise, payload corruption, and trailing garbage. | **TC-UART-02** | `test_uart_total_integrity` | `/dev/ttyS0` @ 115200 | `UARTIntegrityAnalyzer` Purity Inspection |
| **REQ-SYS-UART-03** | The UART line buffer (1024B) shall handle 16-byte frame stress payloads without buffer overflow. | **TC-UART-03A** | `test_uart_stress_matrix[16]` | `/dev/ttyS0` @ 115200 | Exact Payload Length Mirror Verification |
| **REQ-SYS-UART-03** | The UART line buffer (1024B) shall handle 64-byte frame stress payloads without buffer overflow. | **TC-UART-03B** | `test_uart_stress_matrix[64]` | `/dev/ttyS0` @ 115200 | Exact Payload Length Mirror Verification |
| **REQ-SYS-UART-03** | The UART line buffer (1024B) shall handle 128-byte frame stress payloads without buffer overflow. | **TC-UART-03C** | `test_uart_stress_matrix[128]` | `/dev/ttyS0` @ 115200 | Exact Payload Length Mirror Verification |
| **REQ-SYS-UART-03** | The UART line buffer (1024B) shall handle 256-byte frame stress payloads without buffer overflow. | **TC-UART-03D** | `test_uart_stress_matrix[256]` | `/dev/ttyS0` @ 115200 | Exact Payload Length Mirror Verification |

---

## 3. Detailed Test Specifications

### 3.1 TC-I2S-01: I2S PCM Audio Transmission & Frame Capture
* **Requirement Link**: `REQ-SYS-I2S-01`
* **Target Pytest Marker**: `@pytest.mark.i2s`
* **Pre-conditions**:
  * ESP32-S3 flashed with firmware containing active `i2s_dsp_processing_task`.
  * Raspberry Pi ALSA audio driver configured (`plughw` audio device available).
  * Physical I2S lines connected (BCLK: GPIO 18 -> GPIO 4, WS: GPIO 19 -> GPIO 5, DOUT: GPIO 21 -> GPIO 6).
* **Execution Steps**:
  1. Generate synthetic 16-bit PCM WAV file ($f = 1000\text{ Hz}$, $A = 15000\text{ LSB}$, duration = $1.5\text{ s}$).
  2. Initiate non-blocking ALSA playback via `subprocess.Popen("aplay -D default /tmp/test.wav")`.
  3. Sample UART `/dev/ttyS0` concurrently for `I2S_TELEMETRY` frames.
* **Pass/Fail Criteria**:
  * **PASS**: Valid `I2S_TELEMETRY` frame received with Peak Amplitude $> 2000\text{ LSB}$.
  * **FAIL**: Timeout (no telemetry received) OR measured Peak $\le 2000\text{ LSB}$.

### 3.2 TC-I2S-02: Multi-Frequency DSP Accuracy Sweep
* **Requirement Link**: `REQ-SYS-I2S-02`
* **Target Pytest Marker**: `@pytest.mark.i2s`
* **Pre-conditions**: `TC-I2S-01` passed.
* **Execution Steps**:
  1. Iterate through target frequencies: $F_{sweep} = \{1000\text{ Hz}, 2500\text{ Hz}\}$.
  2. Play synthetic WAV file for each frequency via ALSA.
  3. Parse telemetry output on `/dev/ttyS0` and extract `Freq` parameter.
* **Pass/Fail Criteria**:
  * **PASS**: Measured frequency is within tolerance band ($\pm 100\text{ Hz}$) of target frequency.
  * **FAIL**: Telemetry timeout OR frequency calculation error exceeds tolerance.

### 3.3 TC-SPI-01: SPI Full-Duplex DMA Loopback (10-Byte Payload)
* **Requirement Link**: `REQ-SYS-SPI-01`
* **Target Pytest Marker**: `@pytest.mark.spi`
* **Pre-conditions**:
  * ESP32-S3 `spi_slave_task` active with DMA buffer allocated.
  * Physical SPI lines connected (MOSI: GPIO 10 -> GPIO 11, MISO: GPIO 9 -> GPIO 13, SCLK: GPIO 11 -> GPIO 12, CS: GPIO 8 -> GPIO 10).
* **Execution Steps**:
  1. Open `/dev/spidev0.0` at 1 MHz, Mode 0.
  2. Transmit a 10-byte test vector over MOSI while simultaneously reading MISO.
  3. Verify mirrored payload response.
* **Pass/Fail Criteria**:
  * **PASS**: Returned MISO byte array matches transmitted MOSI payload exactly.
  * **FAIL**: Byte mismatch or SPI bus timeout.

### 3.4 TC-UART-01: UART Challenge-Response Ping-Pong Verification
* **Requirement Link**: `REQ-SYS-UART-01`
* **Target Pytest Marker**: `@pytest.mark.uart`
* **Pre-conditions**: Physical UART cross-connected (RPi TX GPIO 14 -> ESP32 RX GPIO 18, RPi RX GPIO 15 -> ESP32 TX GPIO 17, Common GND).
* **Execution Steps**:
  1. Reset input and output buffers on `/dev/ttyS0`.
  2. Write `UART_PING\n` to `/dev/ttyS0` and flush.
  3. Read response line using `readline()`.
* **Pass/Fail Criteria**:
  * **PASS**: Response line contains string `"UART_PONG"`.
  * **FAIL**: Response timeout or unexpected string received.

### 3.5 TC-UART-02: Physical Signal Purity & Noise Analysis
* **Requirement Link**: `REQ-SYS-UART-02`
* **Target Pytest Marker**: `@pytest.mark.uart`
* **Pre-conditions**: `TC-UART-01` passed.
* **Execution Steps**:
  1. Execute `UARTIntegrityAnalyzer.verify_signal_purity(b"ADI_STRESS_V1\n")`.
  2. Verify initial line is clean (`in_waiting == 0`).
  3. Transmit payload and read response.
  4. Wait 50ms and check for trailing garbage.
* **Pass/Fail Criteria**:
  * **PASS**: Clean initial state, exact payload match (`ADI_STRESS_V1`), zero trailing garbage.
  * **FAIL**: Pre-data noise detected, payload corruption, or post-transmission trailing garbage.

### 3.6 TC-UART-03 [A-D]: UART Buffer Stress Matrix
* **Requirement Link**: `REQ-SYS-UART-03`
* **Target Pytest Marker**: `@pytest.mark.uart`, `@pytest.mark.parametrize`
* **Pre-conditions**: Line buffer in ESP32 firmware configured to 1024 bytes.
* **Execution Steps**:
  1. Parametrize test over message lengths $L \in \{16, 64, 128, 256\}$.
  2. Transmit $L-1$ ASCII characters plus `\n`.
  3. Read line response and assert length equals $L-1$.
* **Pass/Fail Criteria**:
  * **PASS**: Echo length matches $L-1$ exactly for all matrix sizes.
  * **FAIL**: Length truncation or missing newline response.

---

## 4. Risk-Based Testing & Boundary Analysis (Analog Devices SQA Compliance)

In accordance with Analog Devices Software Quality Assurance (SQA) guidelines (`DOC_DOC_AD`), the framework incorporates:

* **Boundary Value Analysis (BVA):** Stressing the 1024-byte ESP-IDF static line buffer across multi-byte boundary steps ($16\text{B} \rightarrow 256\text{B}$).
* **Buffer Overflow Mitigation:** Firmware contains explicit bounds checking (`if (line_pos >= sizeof(line_buf) - 1) line_pos = 0;`) to prevent heap corruption or task deadlocks.
* **Environmental Noise Isolation:** Active signal purity checking catches floating pins, jumper degradation, and power rail fluctuations.