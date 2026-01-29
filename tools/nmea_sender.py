#!/usr/bin/env python3
"""
NMEA Data Sender for Saildrop-OS testing.
Connects to SAILDROP_AP WiFi and sends NMEA data from a file line by line.
"""

import argparse
import atexit
import signal
import socket
import subprocess
import sys
import time

WIFI_SSID = "SAILDROP_AP"
WIFI_PASSWORD = "12345678"
DEFAULT_HOST = "192.168.4.1"
DEFAULT_PORT = 2001

previous_wifi = None


def get_current_wifi() -> str | None:
    """Get the currently connected WiFi SSID."""
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "active,ssid", "dev", "wifi"],
            capture_output=True,
            text=True
        )
        for line in result.stdout.strip().split('\n'):
            if line.startswith("yes:"):
                return line.split(":", 1)[1]
    except Exception:
        pass
    return None


def restore_wifi():
    """Reconnect to the previous WiFi network."""
    global previous_wifi
    if previous_wifi:
        print(f"\nReconnecting to previous WiFi '{previous_wifi}'...")
        try:
            subprocess.run(
                ["nmcli", "connection", "up", previous_wifi],
                capture_output=True,
                text=True
            )
            print(f"Reconnected to '{previous_wifi}'")
        except Exception as e:
            print(f"Failed to reconnect: {e}")


def signal_handler(_signum, _frame):
    """Handle Ctrl+C and other signals."""
    restore_wifi()
    sys.exit(0)


def connect_wifi(ssid: str, password: str) -> bool:
    """Connect to WiFi using nmcli (Linux)."""
    print(f"Connecting to WiFi '{ssid}'...")
    try:
        # First, try to connect to existing connection
        result = subprocess.run(
            ["nmcli", "connection", "up", ssid],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print(f"Connected to '{ssid}'")
            return True

        # If that fails, create a new connection
        result = subprocess.run(
            ["nmcli", "device", "wifi", "connect", ssid, "password", password],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print(f"Connected to '{ssid}'")
            return True
        else:
            print(f"Failed to connect: {result.stderr}")
            return False
    except FileNotFoundError:
        print("nmcli not found. Please connect to WiFi manually.")
        return False


def send_nmea_data(filepath: str, host: str, port: int, delay: float, start_line: int = 1) -> None:
    """Send NMEA data from file over TCP, one line per second."""
    print(f"Connecting to {host}:{port}...")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((host, port))
            print(f"Connected to {host}:{port}")

            with open(filepath, 'r') as f:
                lines = f.readlines()

            # Skip to start line (1-indexed)
            if start_line > 1:
                lines = lines[start_line - 1:]
                print(f"Starting from line {start_line}, sending {len(lines)} lines from '{filepath}'...")
            else:
                print(f"Sending {len(lines)} lines from '{filepath}'...")

            total_lines = len(lines)
            for i, line in enumerate(lines, 1):
                line = line.strip()
                if not line:
                    continue

                # Ensure line ends with CRLF (NMEA standard)
                if not line.endswith('\r\n'):
                    line = line + '\r\n'

                sock.sendall(line.encode('ascii'))
                print(f"[{i}/{total_lines}] (line {start_line + i - 1}) {line.strip()}")
                time.sleep(delay)

            print("Done sending all lines.")

    except ConnectionRefusedError:
        print(f"Connection refused. Is the device listening on {host}:{port}?")
        sys.exit(1)
    except FileNotFoundError:
        print(f"File not found: {filepath}")
        sys.exit(1)
    except KeyboardInterrupt:
        pass  # Handled by signal_handler


def main():
    parser = argparse.ArgumentParser(
        description="Send NMEA data to Saildrop-OS device"
    )
    parser.add_argument("file", help="NMEA data file to send")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"Device IP (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"TCP port (default: {DEFAULT_PORT})")
    parser.add_argument("--delay", type=float, default=0.1, help="Delay between lines in seconds (default: 0.1)")
    parser.add_argument("--start-line", type=int, default=1, help="Line number to start from (default: 1)")
    parser.add_argument("--no-wifi", action="store_true", help="Skip WiFi connection (already connected)")
    parser.add_argument("--loop", action="store_true", help="Loop the file continuously")

    args = parser.parse_args()

    if not args.no_wifi:
        global previous_wifi
        previous_wifi = get_current_wifi()
        if previous_wifi:
            print(f"Current WiFi: '{previous_wifi}' (will reconnect on exit)")

        # Register cleanup handlers
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
        atexit.register(restore_wifi)

        if not connect_wifi(WIFI_SSID, WIFI_PASSWORD):
            print("WiFi connection failed. Use --no-wifi if already connected.")
            sys.exit(1)
        time.sleep(2)  # Wait for connection to stabilize

    if args.loop:
        while True:
            send_nmea_data(args.file, args.host, args.port, args.delay, args.start_line)
    else:
        send_nmea_data(args.file, args.host, args.port, args.delay, args.start_line)


if __name__ == "__main__":
    main()
