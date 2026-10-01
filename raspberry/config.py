import os
from pathlib import Path

# ============================================================================
# COMMUNICATION MODE SELECTOR
# ============================================================================
COMM_MODE_BLE = "BLE"
COMM_MODE_WIFI = "WIFI"
COMM_MODE_LORA = "LORA"

# Select active mode: COMM_MODE_BLE, COMM_MODE_WIFI, or COMM_MODE_LORA
ACTIVE_COMM_MODE = COMM_MODE_BLE

# ============================================================================
# GENERAL GATEWAY CONFIGURATION
# ============================================================================
BASE_DIR = Path(__file__).resolve().parent
CSV_FILE = BASE_DIR / "gateway.csv"
CSV_FIELDS = ["node_id", "timestamp", "temperature", "ph", "turbidity"]

# General execution loop delay (seconds)
LOOP_DELAY_S = 5

# ============================================================================
# BLUETOOTH LOW ENERGY (BLE) CONFIGURATION
# ============================================================================
# Adapter name (check with `bluetoothctl list`)
BLUETOOTH_INTERFACE = "hci0"

# GATT UUIDs (must match ESP32 config.h)
SERVICE_UUID = "7d8f0001-3160-4b12-9a10-000000000001"
MEASUREMENT_UUID = "7d8f0002-3160-4b12-9a10-000000000002"
ACK_UUID = "7d8f0003-3160-4b12-9a10-000000000003"

# Scanning & Connection Settings
DEVICE_NAME_PREFIX = "ICC316-G"
SCAN_TIMEOUT_S = 10
MEASUREMENT_TIMEOUT_S = 30

# Distance Estimation Settings (Log-Distance Path Loss)
MEASURED_POWER_1M = -59    # Signal strength (dBm) at 1 meter
PATH_LOSS_EXPONENT = 2.0   # 2.0 = open field, 2.5–3.0 = indoor/obstacles

# ============================================================================
# WI-FI CONFIGURATION (Local Flask / HTTP API Server)
# ============================================================================
HTTP_HOST = "0.0.0.0"       # Listen on all local interfaces
HTTP_PORT = 5000
HTTP_ENDPOINT = "/api/data"

# ============================================================================
# LORA CONFIGURATION (External USB-Serial Module)
# ============================================================================
# Verify connected serial device using: ls /dev/ttyUSB* or ls /dev/ttyACM*
LORA_SERIAL_PORT = "/dev/ttyUSB0"  # Or "/dev/ttyACM0" depending on dongle type
LORA_BAUDRATE = 115200
LORA_SERIAL_TIMEOUT_S = 2

# LoRa Protocol Sync (Matches ESP32 payload structure)
LORA_EXPECT_ACK = True
LORA_ACK_SUCCESS_PAYLOAD = b"OK\n"
LORA_ACK_ERROR_PAYLOAD = b"ERROR\n"