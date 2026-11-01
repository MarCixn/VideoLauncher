#!/usr/bin/env python3
"""Listen for PC Stats UDP packets to verify what's being sent."""
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('0.0.0.0', 5556))

print("Listening on port 5556...")
print("Waiting for PC Stats packets...\n")

try:
    while True:
        data, addr = sock.recvfrom(1024)
        msg = data.decode('utf-8')
        print(f"From {addr[0]}: {msg}")
        print(f"Length: {len(msg)} bytes")
        print("-" * 50)
except KeyboardInterrupt:
    print("\nStopped")
finally:
    sock.close()
