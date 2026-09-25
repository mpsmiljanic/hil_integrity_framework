import logging
import time
import pytest

logger = logging.getLogger(__name__)

class UARTIntegrityAnalyzer:
    """
    Advanced physical layer integrity analyzer.
    Detects line noise, signal corruption, and trailing garbage on serial lines.
    """
    def __init__(self, serial_conn):
        self.ser = serial_conn

    def verify_signal_purity(self, message: bytes, timeout: float = 2.0):
        # Step 1: Clear stale data and verify clean initial line state
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        
        time.sleep(0.05)
        if self.ser.in_waiting > 0:
            pre_noise = self.ser.read_all()
            pytest.fail(f"SIGNAL CORRUPTION (PRE-DATA): Detected initial noise on line: {pre_noise}")

        # Step 2: Transmit stimulus
        logger.info(f"Transmitting test packet: '{message.strip().decode('utf-8')}'")
        self.ser.write(message)
        self.ser.flush()

        # Step 3: Read complete response line
        start_time = time.time()
        rx_data = b""
        
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting > 0:
                rx_data += self.ser.read(self.ser.in_waiting)
                if b"\n" in rx_data:
                    break
            time.sleep(0.02)

        if not rx_data:
            pytest.fail("HARDWARE ERROR: Serial port is silent. Check physical TX/RX wires and GND!")

        clean_rx = rx_data.strip().decode("utf-8", errors="ignore")
        expected_clean = message.strip().decode("utf-8", errors="ignore")

        if clean_rx != expected_clean:
            pytest.fail(f"PAYLOAD CORRUPTION: Expected '{expected_clean}', received '{clean_rx}'")

        logger.info("Primary payload received cleanly without corruption.")

        # Step 4: Check for trailing noise after transmission
        time.sleep(0.05)
        if self.ser.in_waiting > 0:
            trailing_garbage = self.ser.read_all()
            pytest.fail(f"SIGNAL INSTABILITY: Trailing noise detected after payload: {trailing_garbage}")

        logger.info("Signal line is completely quiet and stable post-transmission.")


def test_uart_total_integrity(serial_connection):
    """
    Executes full signal purity and physical integrity analysis on UART bus.
    """
    analyzer = UARTIntegrityAnalyzer(serial_connection)
    test_payload = b"ADI_STRESS_V1\n"
    
    analyzer.verify_signal_purity(test_payload)
    logger.info(">>> TOTAL INTEGRITY PASS <<<")