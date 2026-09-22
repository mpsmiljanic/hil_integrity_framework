import pytest
import logging

logger = logging.getLogger(__name__)

# Data-driven testing matrix: Testing stability across multiple baud rates
@pytest.mark.parametrize("baud",[115200, 460800, 921600] , indirect=True)
@pytest.mark.parametrize("msg_len", [3-5])
def test_uart_stress_matrix(dut, baud, msg_len):
    """
    Hardware stress test to identify UART buffer overflows or signal degradation.
    """
    logger.info(f"Running stress test: Baud={baud}, Payload Length={msg_len} bytes")
    
    # Generate unique payload
    test_payload = ("A" * (msg_len - 1)) + "\n"
    
    # Stimulus and Verification
    dut.write(test_payload)
    dut.expect(test_payload, timeout=10)
    
    logger.info(f"Successfully verified {msg_len} bytes at {baud} baud.")