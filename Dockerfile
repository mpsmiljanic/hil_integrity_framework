# Use a slim Python image to reduce attack surface and build time
FROM python:3.13-slim

WORKDIR /app

# Install system dependencies, C toolchain, and ALSA audio tools for I2S
RUN apt-get update && apt-get install -y \
    alsa-utils \
    libasound2-dev \
    libusb-1.0-0 \
    gcc \
    python3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy only requirements first to leverage Docker layer caching
COPY requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt

# Create output directory for test reports
RUN mkdir -p /app/reports

# Copy full repository content
COPY . .

# Run automated HIL tests by default with HTML & JUnit XML reporting
CMD ["pytest", "-s", "-v", "tests/", "--port", "/dev/ttyS0", "--html=reports/report.html", "--self-contained-html", "--junitxml=reports/junit.xml"]
