import argparse
import pytest
import spidev
import serial


def pytest_addoption(parser):
    """
    Register custom CLI options for HIL testing.
    Safely handles option conflicts if plugins (like pytest-embedded) registered them first.
    """
    options = [
        ("--port", "/dev/ttyACM0", "UART serial port for DUT connection (default: /dev/ttyACM0)"),
        ("--baud", "115200", "UART baud rate (default: 115200)"),
        ("--spi-bus", "0", "Raspberry Pi SPI bus index (default: 0)"),
        ("--spi-device", "0", "Raspberry Pi SPI Chip Select / Device index (default: 0 for CE0)"),
    ]

    for opt, default_val, help_text in options:
        try:
            parser.addoption(opt, action="store", default=default_val, help=help_text)
        except (ValueError, argparse.ArgumentError):
            # Option already registered by pytest-embedded or another plugin
            pass


# --- UART Fixtures ---

@pytest.fixture(scope="session")
def uart_port(request):
    """
    Retrieve configured UART serial port from CLI options.
    Falls back to /dev/ttyACM0 if another plugin registered --port with default=None.
    """
    val = request.config.getoption("--port", default=None)
    return val if val is not None else "/dev/ttyACM0"


@pytest.fixture(scope="session")
def uart_baud(request):
    """
    Retrieve configured UART baud rate from CLI options.
    Falls back to 115200 if another plugin registered --baud with default=None.
    """
    val = request.config.getoption("--baud", default=None)
    return int(val) if val is not None else 115200


@pytest.fixture(scope="function")
def serial_connection(uart_port, uart_baud):
    """
    Provide an active serial connection to the Device Under Test (DUT).
    Automatically closes the port after test execution.
    """
    ser = serial.Serial(uart_port, uart_baud, timeout=2.0)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    yield ser
    
    ser.close()


# --- SPI Fixtures ---

@pytest.fixture(scope="session")
def spi_bus(request):
    """Retrieve SPI bus index from CLI options."""
    return int(request.config.getoption("--spi-bus"))


@pytest.fixture(scope="session")
def spi_device(request):
    """Retrieve SPI device index from CLI options."""
    return int(request.config.getoption("--spi-device"))


@pytest.fixture(scope="module")
def spi_master(spi_bus, spi_device):
    """
    Initialize and yield the hardware SPI Master interface (spidev) on RPi.
    Ensures graceful bus teardown after module execution.
    """
    spi = spidev.SpiDev()
    spi.open(spi_bus, spi_device)
    spi.max_speed_hz = 1000000  # Default 1 MHz baseline speed
    spi.mode = 0b00             # SPI Mode 0 (CPOL=0, CPHA=0)
    
    yield spi
    
    spi.close()