import pytest
import logging
import time
from pytest_embedded.dut import Dut

logger = logging.getLogger(__name__)

class UARTIntegrityAnalyzer:
    """
    Advanced integrity analyzer to detect signal noise and physical layer instability.
    """
    def __init__(self, dut: Dut):
        self.dut = dut

    def verify_signal_purity(self, message: str, timeout: int = 5):
        # Step 1: Transmit stimulus
        logger.info(f"Sending test packet: '{message.strip()}'")
        self.dut.write(message) 

        # Step 2: Check for Pre-Data noise (corruption before the actual message)
        try:
            # Index 0: Correct message, Index 1: Any other character (Noise)
            index = self.dut.expect([message, r'.+'], timeout=timeout)
            
            if index == 1:
                received_garbage = self.dut.pexpect_proc.before
                pytest.fail(f"SIGNAL CORRUPTION (PRE-DATA): Detected noise: {received_garbage}")
            
            logger.info("Message received clean (no pre-data noise).")

        except Exception:
            pytest.fail("HARDWARE ERROR: Port is silent. Check physical connections/GND!")

        # Step 3: Check for Trailing Garbage (instability after transmission)
        time.sleep(0.05) 
        try:
            self.dut.expect(r'.+', timeout=0.2)
            noise = self.dut.pexpect_proc.before + self.dut.pexpect_proc.after
            pytest.fail(f"SIGNAL INSTABILITY: Trailing noise detected ({noise})")
        except Exception: 
            logger.info("Signal line stable and silent.")

def test_uart_total_integrity(dut: Dut):
    analyzer = UARTIntegrityAnalyzer(dut)
    test_payload = "ADI_STRESS_V1\n"
    
    analyzer.verify_signal_purity(test_payload)
    logger.info(">>> TOTAL INTEGRITY PASS <<<")