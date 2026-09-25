# Use a slim version to reduce attack surface and build time
FROM python:3.13-slim

WORKDIR /app

# Install system dependencies and build toolchain for C-extensions
RUN apt-get update && apt-get install -y \
    libusb-1.0-0 \
    gcc \
    python3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy only requirements first to leverage Docker layer caching
COPY requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt

# Copy source code
COPY . .

# Run all automated HIL tests by default
CMD ["pytest", "-s", "-v", "tests/"]