#!/usr/bin/env python3
"""Scan for Android devices on the network."""
import socket
import concurrent.futures

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return None

def scan_for_android_devices(local_ip, port=5555):
    parts = local_ip.split('.')
    subnet = f"{parts[0]}.{parts[1]}.{parts[2]}"

    found_devices = []

    def check_host(ip):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(0.3)
            result = s.connect_ex((ip, port))
            s.close()
            if result == 0:
                return ip
        except:
            pass
        return None

    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
        ips = [f"{subnet}.{i}" for i in range(1, 255) if f"{subnet}.{i}" != local_ip]
        results = executor.map(check_host, ips)

        for ip in results:
            if ip:
                found_devices.append(ip)

    return found_devices

if __name__ == '__main__':
    local_ip = get_local_ip()
    print(f"Local IP: {local_ip}")
    print("Scanning for Android devices (ADB port 5555)...")
    devices = scan_for_android_devices(local_ip)
    if devices:
        print(f"Found: {', '.join(devices)}")
    else:
        print("No devices found")
