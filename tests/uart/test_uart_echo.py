import logging
import time
import pytest

logger = logging.getLogger(__name__)

def test_uart_simple_echo(serial_connection):
    """
    Hardware Challenge-Response Test over physical UART (/dev/ttyS0).
    Validates physical wiring and firmware ping-pong logic.
    """
    logger.info("Executing UART hardware challenge-response check...")
    
    serial_connection.reset_input_buffer()
    serial_connection.reset_output_buffer()
    
    serial_connection.write(b"UART_PING\n")
    serial_connection.flush()
    time.sleep(0.05)
    
    response = serial_connection.readline().decode("utf-8", errors="ignore").strip()
    logger.info(f"Received response from DUT: '{response}'")
    
    assert "UART_PONG" in response, f"Expected 'UART_PONG', received: '{response}'"
    logger.info("UART physical line verification PASSED!")