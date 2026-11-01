#!/usr/bin/env python3
"""
PC Stats Sender for Video Launcher
Sends CPU, GPU, RAM, Disk, and Temperature stats to Android device via UDP broadcast.
Runs as a hidden background process.

Requirements:
    pip install psutil wmi pywin32

Usage:
    python pc_stats.py
    pythonw pc_stats.py  (hidden, no console window)
"""

import socket
import time
import json
import sys
import os

try:
    import psutil
except ImportError:
    import subprocess
    # Flaga dla Windows zapobiegająca tworzeniu okna konsoli przez proces potomny
    startupinfo = None
    if os.name == 'nt':
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    
    subprocess.check_call(
        [sys.executable, "-m", "pip", "install", "psutil"],
        startupinfo=startupinfo
    )
    import psutil

# Windows WMI for temperatures (try LibreHardwareMonitor first, then OpenHardwareMonitor)
HAS_WMI = False
wmi_obj = None
wmi_source = None
try:
    import wmi
    # Try LibreHardwareMonitor first (actively maintained, supports newer CPUs)
    try:
        wmi_obj = wmi.WMI(namespace="root\\LibreHardwareMonitor")
        HAS_WMI = True
        wmi_source = "LHM"
    except:
        # Fall back to OpenHardwareMonitor
        wmi_obj = wmi.WMI(namespace="root\\OpenHardwareMonitor")
        HAS_WMI = True
        wmi_source = "OHM"
except:
    pass

# GPU support (NVIDIA via pynvml - direct C-types, no console window)
HAS_GPU = False
try:
    import pynvml
    pynvml.nvmlInit()
    HAS_GPU = True
except Exception:
    HAS_GPU = False


def get_cpu_temp_ohm():
    """Get CPU temp from OpenHardwareMonitor or LibreHardwareMonitor."""
    if not HAS_WMI or wmi_obj is None:
        return None
    try:
        sensors = wmi_obj.Sensor()
        for sensor in sensors:
            if sensor.SensorType == 'Temperature':
                # Check if it's a CPU sensor by name or parent path
                name = sensor.Name.lower()
                parent = getattr(sensor, 'Parent', '').lower()
                if 'cpu' in name or 'core' in name or '/cpu/' in parent:
                    # Skip GPU Core
                    if 'gpu' in name or '/gpu' in parent:
                        continue
                    return sensor.Value
    except:
        pass
    return None


def get_cpu_temp_psutil():
    """Get CPU temp using psutil (Linux/Mac)."""
    try:
        temps = psutil.sensors_temperatures()
        if temps:
            for name in ['coretemp', 'k10temp', 'cpu_thermal', 'acpitz', 'cpu-thermal']:
                if name in temps and temps[name]:
                    return temps[name][0].current
            for name, entries in temps.items():
                if entries:
                    return entries[0].current
    except:
        pass
    return None


def get_cpu_temp():
    """Get CPU temperature from any available source."""
    # Try OpenHardwareMonitor first (Windows)
    temp = get_cpu_temp_ohm()
    if temp is not None:
        return temp

    # Try psutil (Linux/Mac)
    temp = get_cpu_temp_psutil()
    if temp is not None:
        return temp

    return None


def get_gpu_stats():
    """Get GPU stats safely without launching console subprocesses.
    Takes multiple samples and returns the maximum to better reflect actual load."""
    if not HAS_GPU:
        return None, None, None
    try:
        handle = pynvml.nvmlDeviceGetHandleByIndex(0)

        # Pobierz kilka próbek i weź maksymalną wartość (lepiej oddaje rzeczywiste obciążenie)
        samples = []
        for _ in range(5):
            utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)
            samples.append(utilization.gpu)
            time.sleep(0.02)  # 20ms między próbkami

        gpu_load = max(samples)  # Maksymalna wartość z próbek

        # Użycie pamięci VRAM (%)
        memory_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
        gpu_mem = (memory_info.used / memory_info.total) * 100

        # Temperatura (°C)
        gpu_temp = pynvml.nvmlDeviceGetTemperature(handle, pynvml.NVML_TEMPERATURE_GPU)

        return gpu_load, gpu_mem, gpu_temp
    except Exception:
        return None, None, None


# Network tracking
last_net_io = None
last_net_time = None


def get_net_speed():
    """Get network speed in MB/s."""
    global last_net_io, last_net_time

    try:
        net_io = psutil.net_io_counters()
        current_time = time.time()

        if last_net_io is None:
            last_net_io = net_io
            last_net_time = current_time
            return 0.0

        time_delta = current_time - last_net_time
        if time_delta < 0.1:
            return 0.0

        bytes_delta = (net_io.bytes_sent - last_net_io.bytes_sent) + \
                      (net_io.bytes_recv - last_net_io.bytes_recv)

        last_net_io = net_io
        last_net_time = current_time

        # Return MB/s
        return bytes_delta / time_delta / (1024 * 1024)
    except:
        return 0.0


def get_stats():
    """Collect all system stats."""
    # CPU
    cpu_percent = psutil.cpu_percent(interval=0.1)

    # RAM
    mem = psutil.virtual_memory()
    ram_percent = mem.percent

    # GPU
    gpu_load, gpu_mem, gpu_temp = get_gpu_stats()

    # Network speed
    net_speed = get_net_speed()

    # Build stats with important fields first (in case of truncation)
    stats = {
        'cpu': round(cpu_percent, 1),
        'gpu': round(gpu_load, 1) if gpu_load is not None else 0,
        'ram': round(ram_percent, 1),
        'gpu_temp': round(gpu_temp, 0) if gpu_temp is not None else 0,
        'net': round(net_speed, 1),
    }

    return stats


def get_local_ip():
    """Get local IP address."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return None


def scan_for_android_devices(local_ip, port=5555):
    """Scan subnet for Android devices (ADB port open)."""
    import concurrent.futures

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

    # Scan all IPs in subnet
    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
        ips = [f"{subnet}.{i}" for i in range(1, 255) if f"{subnet}.{i}" != local_ip]
        results = executor.map(check_host, ips)

        for ip in results:
            if ip:
                found_devices.append(ip)

    return found_devices


def main():
   # Zapobiega alokowaniu okna konsoli przez pythonw w przypadku błędów
    if sys.executable.endswith('pythonw.exe') or not sys.stdout:
        sys.stdout = open(os.devnull, 'w')
        sys.stderr = open(os.devnull, 'w')
    # UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    port = 5556

    # Check if running hidden (no console)
    hidden = not sys.stdout or sys.executable.endswith('pythonw.exe')

    # Get local IP
    local_ip = get_local_ip()
    if not local_ip:
        if not hidden:
            print("Error: Cannot detect local IP")
        return

    # Scan for Android devices
    if not hidden:
        print(f"PC Stats Sender started")
        print(f"Local IP: {local_ip}")
        print(f"Scanning for Android devices...")

    target_devices = scan_for_android_devices(local_ip)

    if not target_devices:
        if not hidden:
            print("No Android devices found! Falling back to broadcast.")
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        parts = local_ip.split('.')
        target_devices = [f"{parts[0]}.{parts[1]}.{parts[2]}.255"]

    if not hidden:
        print(f"Target devices: {', '.join(target_devices)}")
        print(f"GPU support: {'Yes' if HAS_GPU else 'No'}")
        print(f"Temp support: {'Yes (' + wmi_source + ')' if HAS_WMI else 'No'}")
        print("-" * 40)

    # Rescan every 60 seconds
    last_scan = time.time()
    scan_interval = 60

    while True:
        try:
            # Rescan for devices periodically
            if time.time() - last_scan > scan_interval:
                new_devices = scan_for_android_devices(local_ip)
                if new_devices:
                    target_devices = new_devices
                    if not hidden:
                        print(f"\nRescan: {', '.join(target_devices)}")
                last_scan = time.time()

            stats = get_stats()
            message = json.dumps(stats)

            # Send to all target devices
            for target in target_devices:
                sock.sendto(message.encode(), (target, port))

            if not hidden:
                print(f"\nJSON: {message}")
                line = f"CPU: {stats['cpu']:5.1f}%  RAM: {stats['ram']:5.1f}%"
                if 'gpu' in stats:
                    line += f"  GPU: {stats['gpu']:5.1f}%"
                if 'cpu_temp' in stats:
                    line += f"  CPU-T: {stats['cpu_temp']:.0f}C"
                if 'gpu_temp' in stats:
                    line += f"  GPU-T: {stats['gpu_temp']:.0f}C"
                print(f"{line}")

            time.sleep(2)

        except KeyboardInterrupt:
            break
        except Exception as e:
            if not hidden:
                print(f"\nError: {e}")
            time.sleep(2)

    sock.close()


if __name__ == '__main__':
    main()
