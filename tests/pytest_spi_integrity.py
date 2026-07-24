import pytest
import spidev
import time

@pytest.fixture(scope="module")
def spi_bus():
    """Initialize SPI bus on Raspberry Pi (Bus 0, Device 0)."""
    spi = spidev.SpiDev()
    try:
        spi.open(0, 0)
        spi.mode = 0b00  
        spi.loop = False    
        yield spi
    finally:
        try:
            spi.close()
        except OSError:
            pass

# List of frequencies to test (Audio standards)
@pytest.mark.parametrize("speed_hz", [44100, 48000, 88200, 96000, 176400, 192000])
@pytest.mark.parametrize("test_pattern", [
    [0xAA, 0x55, 0xAA, 0x55],
    [0xFF, 0x00, 0xFF, 0x00],
    list(range(0, 10)),
])
def test_spi_loopback_integrity(spi_bus, speed_hz, test_pattern):
    """Validate data integrity using Buffered Echo mechanism."""
    spi_bus.max_speed_hz = speed_hz
    time.sleep(0.05) 

    # Step 1: Send pattern
    spi_bus.xfer2(list(test_pattern))
    time.sleep(0.01)

    # Step 2: Retrieve echo
    dummy_data = [0x00] * len(test_pattern)
    received = spi_bus.xfer2(dummy_data) 

    assert received == test_pattern, \
        f"Failure at {speed_hz}Hz! Got {received}, expected {test_pattern}"

def test_spi_debug_output(spi_bus):
    """Sanity check with fixed 1MHz speed."""
    spi_bus.max_speed_hz = 1000000
    expected = [0xDE, 0xAD, 0xBE, 0xEF]
    
    spi_bus.xfer2(list(expected))
    time.sleep(0.01)
    received = spi_bus.xfer2([0x00, 0x00, 0x00, 0x00])
    
    print(f"\nDEBUG - Received: {received}")
    assert received == expected