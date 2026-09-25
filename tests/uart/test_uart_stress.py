import logging
import time
import pytest

logger = logging.getLogger(__name__)

@pytest.mark.parametrize("msg_len", [16, 64, 128, 256])
def test_uart_stress_matrix(serial_connection, msg_len):
    """
    Executes buffer stress test with variable payload sizes over UART.
    """
    logger.info(f"Running UART stress test with payload size: {msg_len} bytes")
    
    serial_connection.reset_input_buffer()
    serial_connection.reset_output_buffer()
    
    # Generate test payload of exact specified length
    raw_payload_str = "A" * (msg_len - 1)
    payload = (raw_payload_str + "\n").encode("utf-8")
    
    serial_connection.write(payload)
    serial_connection.flush()
    
    # Allow hardware buffer processing time based on frame size
    time.sleep(0.1)
    
    response = serial_connection.readline().decode("utf-8", errors="ignore").strip()
    
    assert len(response) == (msg_len - 1), f"Stress test failed! Expected length {msg_len - 1}, received {len(response)}"
    logger.info(f"Successfully verified {msg_len} bytes payload over physical UART!")
