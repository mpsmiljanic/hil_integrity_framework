import pytest
import logging
from pytest_embedded.dut import Dut

logger = logging.getLogger(__name__)

def test_uart_simple_echo(dut, target_port):
    """
    Basic connectivity check. 
    Verifies that the DUT (Device Under Test) responds to a simple stimulus.
    """
    logger.info(f"Executing echo test on port: {target_port}")
    dut.write("TEST\n")
    dut.expect("TEST", timeout=5)

