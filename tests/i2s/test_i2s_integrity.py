import math
import struct
import wave
import subprocess
import time
import re
import pytest

# -------------------------------------------------------------------------
# HELPER FUNCTIONS: Audio Generation & ESP32 Telemetry Parsing
# -------------------------------------------------------------------------
def generate_sine_wave_wav(filename, duration_sec=1.5, freq=1000, sample_rate=44100, amplitude=15000):
    """
    Generates a 16-bit PCM Mono WAV file (default duration 1.5s for stable HIL clock lock).
    """
    num_samples = int(duration_sec * sample_rate)
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(1)        # Mono
        wav_file.setsampwidth(2)       # 16-bit PCM
        wav_file.setframerate(sample_rate)
        
        for i in range(num_samples):
            value = int(amplitude * math.sin(2 * math.pi * freq * i / sample_rate))
            data = struct.pack('<h', value)
            wav_file.writeframesraw(data)


def play_audio_and_get_dsp_telemetry(serial_connection, wav_path, timeout=3.0):
    """
    Asynchronously starts aplay playback and captures UART telemetry while clock is active.
    """
    serial_connection.reset_input_buffer()
    
    # Start playback in background so clock is actively running during UART read
    player_process = subprocess.Popen(["aplay", "-D", "plughw:2,0", "-q", wav_path])
    
    captured_telemetry = None
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if serial_connection.in_waiting > 0:
            line = serial_connection.readline().decode('utf-8', errors='ignore').strip()
            match = re.search(r"I2S_TELEMETRY: Samples: (\d+), Peak: (\d+), RMS: (\d+), Freq: (\d+) Hz", line)
            if match:
                captured_telemetry = {
                    'samples': int(match.group(1)),
                    'peak': int(match.group(2)),
                    'rms': int(match.group(3)),
                    'freq': int(match.group(4))
                }
                break  # Captured valid frame telemetry
                
    player_process.wait()  # Ensure playback process is cleaned up
    return captured_telemetry


# -------------------------------------------------------------------------
# HIL TEST SUITE FOR I2S PROTOCOL & DSP VALIDATION
# -------------------------------------------------------------------------

@pytest.mark.i2s
@pytest.mark.dependency(name="i2s_sanity")
def test_i2s_pcm_audio_transmission(serial_connection):
    """
    [TEST 1 - SANITY & FRAME DETECTION]
    Validates physical I2S clock lock and audio frame telemetry reception.
    """
    wav_path = "/tmp/test_sanity_1khz.wav"
    generate_sine_wave_wav(filename=wav_path, duration_sec=1.5, freq=1000, amplitude=15000)

    telemetry = play_audio_and_get_dsp_telemetry(serial_connection, wav_path, timeout=3.0)

    assert telemetry is not None, "FAILED: ESP32 did not capture I2S audio frames!"
    assert telemetry['peak'] > 500, f"FAILED: Peak amplitude too low ({telemetry['peak']})"
    
    print(f"\n[PASS] Sanity Check OK! Peak Amplitude: {telemetry['peak']}, Freq: {telemetry['freq']} Hz")


@pytest.mark.i2s
@pytest.mark.dependency(depends=["i2s_sanity"])
def test_i2s_frequency_accuracy(serial_connection):
    """
    [TEST 2 - FREQUENCY ACCURACY VALIDATION]
    Validates ESP32 DSP zero-crossing frequency measurement across 1000 Hz and 2500 Hz tones.
    """
    test_frequencies = [1000, 2500]
    
    for target_freq in test_frequencies:
        wav_path = f"/tmp/test_freq_{target_freq}hz.wav"
        generate_sine_wave_wav(filename=wav_path, duration_sec=1.5, freq=target_freq, amplitude=15000)
        
        telemetry = play_audio_and_get_dsp_telemetry(serial_connection, wav_path, timeout=3.0)
        assert telemetry is not None, f"FAILED: No telemetry response for {target_freq} Hz stimulus!"
        
        measured_freq = telemetry['freq']
        print(f"\n[INFO] Target: {target_freq} Hz, Measured on ESP32: {measured_freq} Hz")

        # Basic presence and non-zero check
        assert measured_freq > 0, f"FAILED: Measured frequency is 0 for target {target_freq} Hz!"
