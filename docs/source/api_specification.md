API & Protocol Interface Specification
1. Control Plane: Hardware UART Protocol Interface Specification

The Control Plane provides bidirectional communication, telemetry reporting, and stress matrix verification between the HIL Test Runner (Raspberry Pi) and the DUT (ESP32-S3) over /dev/ttyS0.
1.1 Physical & Bus Configuration

    Port Path: /dev/ttyS0 (Raspberry Pi Physical Mini-UART)
    Baud Rate: 115200 bps
    Data Bits: 8
    Parity: None
    Stop Bits: 1
    Flow Control: None
    Line Termination: \r\n (ASCII 0x0D 0x0A / CR-LF)

1.2 Ping-Pong Command API (TC-UART-01)

Used for physical line verification and automated challenge-response checks.

    Request Syntax: UART_PING\n
    Response Syntax: UART_PONG\r\n
    Latency Guarantee: Response transmitted within 50ms of newline receipt.

1.3 Stress Payload API (TC-UART-03)

Used for line buffer boundary testing and stress validation up to 256-byte payload frames.

    Request Syntax: <PAYLOAD_BYTES>\n (where length is 16, 64, 128, or 256 bytes)
    Response Syntax: <PAYLOAD_BYTES>\r\n
    Buffer Safety: Handled by 1024-byte static buffer with overflow reset in firmware.

1.4 DSP Telemetry Output API (I2S_TELEMETRY)

Output by the DUT over UART1 when I2S signal power exceeds noise threshold ($\text{Peak} > 2000\text{ LSB}$).
Message Syntax:

I2S_TELEMETRY: Samples: <SAMPLES>, Peak: <PEAK>, RMS: <RMS>, Freq: <FREQ> Hz\r\n

Field Specifications:
Field Name	Type	Data Unit	Expected Range	Description
<SAMPLES>	uint16_t	Count	1023 - 1024	Number of 32-bit I2S frame slots processed in current DMA buffer.
<PEAK>	uint16_t	LSB	2000 - 32767	Absolute peak amplitude detected in DMA buffer.
<RMS>	uint32_t	LSB	0 - 32767	Root Mean Square (RMS) energy calculation.
<FREQ>	uint32_t	Hz	20 - 5000	Estimated fundamental frequency from hysteresis zero-crossing detector.
Pytest Telemetry Extraction Regex:

I2S_TELEMETRY_REGEX = r"I2S_TELEMETRY: Samples: (\d+), Peak: (\d+), RMS: (\d+), Freq: (\d+) Hz"

2. Data Plane 1: I2S PCM Audio Stimulus Specification
2.1 Waveform Generation Parameters

Synthesized on the fly by the test suite into /tmp/*.wav prior to ALSA execution.

    Sample Rate ($f_s$): 44,100 Hz
    Sample Encoding: 16-bit Signed Integer PCM (Little-Endian)
    Channels: Mono (1 Channel)
    Duration: 1.5 seconds
    Mathematical Synthesis Formula: $$x[i] = \text{int}\left(A \cdot \sin\left(\frac{2\pi \cdot f \cdot i}{f_s}\right)\right)$$

2.2 Target Stimulus Test Matrix
Profile Name	Target Frequency ($f$)	Peak Amplitude ($A$)	Target Peak Threshold	Target Frequency Band
Sanity Check	1000 Hz	15000 LSB	$> 2000\text{ LSB}$	$900 - 1100\text{ Hz}$
Mid Sweep	1000 Hz	15000 LSB	$> 2000\text{ LSB}$	$900 - 1100\text{ Hz}$
High Sweep	2500 Hz	15000 LSB	$> 2000\text{ LSB}$	$2300 - 2700\text{ Hz}$
3. Data Plane 2: SPI Full-Duplex Bus Specification

    Port Identifier: /dev/spidev0.0
    Clock Mode: SPI Mode 0 (CPOL = 0, CPHA = 0)
    Clock Frequency: 1.0 MHz
    Transaction Size: 10 Bytes
    Protocol Execution: Simultaneous full-duplex MOSI write / MISO read transaction via SpiDev.

4. Pytest Test Helper & Fixture API Reference
4.1 serial_connection Fixture Lifecycle (conftest.py)

@pytest.fixture(scope="module")
def serial_connection(port_option):
    """
    Manages PySerial connection to /dev/ttyS0 @ 115200 baud.
    Flushes input and output buffers prior to returning connection handle.
    """
    ser = serial.Serial(port_option, 115200, timeout=2.0)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    yield ser
    ser.close()

4.2 UARTIntegrityAnalyzer Class Reference (tests/uart/test_uart_modular.py)
verify_signal_purity(message: bytes, timeout: float = 2.0)

Executes physical line purity inspection across 4 strict verification gates:

    Pre-Data Noise Gate: Checks in_waiting == 0 prior to transmission. Fails on floating line noise.
    Payload Transmission: Writes message and flushes output buffer.
    Response Frame Validation: Reads response line and validates string match against expected payload.
    Post-Transmission Stability Gate: Waits 50ms post-response and asserts zero trailing garbage.
