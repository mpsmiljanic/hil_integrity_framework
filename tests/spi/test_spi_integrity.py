import pytest
import time


def test_spi_two_phase_echo_10bytes(spi_master):
    """
    TC-SPI-01: Two-Phase 10-Byte Hardware Echo Validation.
    Phase 1: Transmit 10-byte pattern (Reference) to ESP32 (DUT performs memcpy in C).
    Phase 2: Transmit dummy bytes to read back the exact echo from ESP32 MISO line.
    """
    expected_pattern = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A]
    tx_pattern = list(expected_pattern)
    
    # Phase 1: Send 10 bytes to ESP32 (xfer2 modifies tx_pattern in-place)
    spi_master.xfer2(tx_pattern)
    time.sleep(0.01)  # 10ms delay for FreeRTOS task execution on ESP32
    
    # Phase 2: Read back response from ESP32 sendbuf
    dummy_tx = [0x00] * 10
    rx_pattern = spi_master.xfer2(dummy_tx)
    
    # Verify complete data integrity byte-by-byte against expected_pattern
    assert rx_pattern == expected_pattern, (
        f"SPI Echo Integrity Failure! Expected {expected_pattern}, but received {rx_pattern}."
    )