#!/usr/bin/env python3
"""
Screen Stream Server for Video Launcher
Streams PC screen to Android device via MJPEG over HTTP.
Pure Python implementation - no FFmpeg required!

Requirements:
    pip install mss pillow

Usage:
    python screen_stream.py
    pythonw screen_stream.py  (hidden, no console window)
"""

import socket
import json
import threading
import os
import sys
import time
import io
from http.server import HTTPServer, BaseHTTPRequestHandler

try:
    import mss
    import mss.tools
    HAS_MSS = True
except ImportError:
    HAS_MSS = False
    print("ERROR: mss not installed. Run: pip install mss")

try:
    from PIL import Image, ImageDraw
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("ERROR: Pillow not installed. Run: pip install pillow")

# Windows cursor support and mouse control
HAS_CURSOR = False
HAS_MOUSE = False
if sys.platform == 'win32':
    try:
        import ctypes
        from ctypes import wintypes
        HAS_CURSOR = True
    except:
        pass

try:
    from pynput.mouse import Button, Controller as MouseController
    mouse = MouseController()
    HAS_MOUSE = True
except ImportError:
    try:
        import pyautogui
        pyautogui.FAILSAFE = False
        mouse = None
        HAS_MOUSE = True
    except ImportError:
        mouse = None
        pass

# Configuration
CONTROL_PORT = 5558
STREAM_PORT = 8080
STREAM_WIDTH = 1024
STREAM_HEIGHT = 600
STREAM_FPS = 25
STREAM_QUALITY = 92  # JPEG quality (1-100)
MONITOR_INDEX = 0  # 0 = auto/primary, 1+ = specific monitor
ZOOM_LEVEL = 1.3  # 1.0 = no zoom, 1.5 = 150% zoom (capture 66% of screen)

# Global state
streaming = False
http_server = None
stream_lock = threading.Lock()
sct = None  # mss instance
current_monitor = 1  # Which monitor to stream (1 = primary)


def get_cursor_pos():
    """Get current cursor position on Windows."""
    if not HAS_CURSOR:
        return None
    try:
        class POINT(ctypes.Structure):
            _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]
        pt = POINT()
        ctypes.windll.user32.GetCursorPos(ctypes.byref(pt))
        return (pt.x, pt.y)
    except:
        return None


def click_at(x, y):
    """Move mouse and click using absolute coordinates with SendInput."""
    x, y = int(x), int(y)

    try:
        import ctypes

        user32 = ctypes.windll.user32

        # Get screen metrics for absolute coordinate conversion
        SM_CXSCREEN = 0
        SM_CYSCREEN = 1
        SM_XVIRTUALSCREEN = 76
        SM_YVIRTUALSCREEN = 77
        SM_CXVIRTUALSCREEN = 78
        SM_CYVIRTUALSCREEN = 79

        # Get virtual screen dimensions (all monitors combined)
        virt_left = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
        virt_top = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
        virt_width = user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
        virt_height = user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)

        # Convert to absolute coordinates (0-65535 range for SendInput ABSOLUTE)
        abs_x = int(((x - virt_left) * 65536) / virt_width)
        abs_y = int(((y - virt_top) * 65536) / virt_height)

        # Clamp values
        abs_x = max(0, min(65535, abs_x))
        abs_y = max(0, min(65535, abs_y))

        # Click using SendInput with ABSOLUTE coordinates
        INPUT_MOUSE = 0
        MOUSEEVENTF_MOVE = 0x0001
        MOUSEEVENTF_LEFTDOWN = 0x0002
        MOUSEEVENTF_LEFTUP = 0x0004
        MOUSEEVENTF_ABSOLUTE = 0x8000
        MOUSEEVENTF_VIRTUALDESK = 0x4000  # Map to entire virtual desktop

        class MOUSEINPUT(ctypes.Structure):
            _fields_ = [
                ("dx", ctypes.c_long),
                ("dy", ctypes.c_long),
                ("mouseData", ctypes.c_ulong),
                ("dwFlags", ctypes.c_ulong),
                ("time", ctypes.c_ulong),
                ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))
            ]

        class INPUT_UNION(ctypes.Union):
            _fields_ = [("mi", MOUSEINPUT)]

        class INPUT(ctypes.Structure):
            _fields_ = [
                ("type", ctypes.c_ulong),
                ("union", INPUT_UNION)
            ]

        # Move to position with ABSOLUTE + VIRTUALDESK
        inp = INPUT()
        inp.type = INPUT_MOUSE
        inp.union.mi.dx = abs_x
        inp.union.mi.dy = abs_y
        inp.union.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK
        user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))

        time.sleep(0.01)

        # Mouse down at same position
        inp.union.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK
        user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))

        time.sleep(0.01)

        # Mouse up
        inp.union.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK
        user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))

        return True
    except Exception as e:
        print(f"Click error: {e}", flush=True)
        return False


def draw_cursor(img, cursor_pos, monitor, scale_x, scale_y):
    """Draw cursor on the image."""
    if cursor_pos is None:
        return img

    # Calculate cursor position relative to monitor and scaled
    cx = int((cursor_pos[0] - monitor['left']) * scale_x)
    cy = int((cursor_pos[1] - monitor['top']) * scale_y)

    # Check if cursor is within bounds
    if cx < 0 or cy < 0 or cx >= img.width or cy >= img.height:
        return img

    # Draw a simple arrow cursor
    draw = ImageDraw.Draw(img)
    cursor_size = 16

    # White arrow with black outline
    arrow_points = [
        (cx, cy),
        (cx, cy + cursor_size),
        (cx + cursor_size // 3, cy + cursor_size * 2 // 3),
        (cx + cursor_size * 2 // 3, cy + cursor_size * 2 // 3),
        (cx + cursor_size // 3, cy + cursor_size // 2),
        (cx + cursor_size, cy + cursor_size // 2),
        (cx, cy)
    ]

    # Draw outline
    draw.polygon(arrow_points, fill='white', outline='black')

    return img


class MJPEGHandler(BaseHTTPRequestHandler):
    """HTTP handler for MJPEG streaming."""

    def log_message(self, format, *args):
        """Suppress HTTP logs."""
        pass

    def do_GET(self):
        """Handle GET request - stream MJPEG."""
        global current_monitor

        if self.path != '/' and self.path != '/stream':
            self.send_error(404)
            return

        print(f"Client connected: {self.client_address}")

        self.send_response(200)
        self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        self.send_header('Connection', 'close')
        self.end_headers()

        try:
            with mss.mss() as sct:
                # Get selected monitor (current_monitor: 1=primary, 2=secondary, etc.)
                mon_idx = min(current_monitor, len(sct.monitors) - 1)
                monitor = sct.monitors[mon_idx]
                print(f"Streaming monitor {mon_idx}: {monitor['width']}x{monitor['height']}")

                # Calculate capture region and scale factors
                mon_width = monitor['width']
                mon_height = monitor['height']
                scale_x = STREAM_WIDTH / mon_width
                scale_y = STREAM_HEIGHT / mon_height

                # Capture full screen and resize - stream until client disconnects
                client_connected = True
                while client_connected:
                    start_time = time.time()

                    # Get cursor position BEFORE capture
                    cursor_pos = get_cursor_pos()

                    # Capture screen
                    img = sct.grab(monitor)

                    # Convert to PIL Image (mss returns BGRA format)
                    pil_img = Image.frombytes('RGB', img.size, img.bgra, 'raw', 'BGRX')

                    # Resize to target resolution (BILINEAR is faster and cleaner)
                    pil_img = pil_img.resize((STREAM_WIDTH, STREAM_HEIGHT), Image.BILINEAR)

                    # Draw cursor
                    pil_img = draw_cursor(pil_img, cursor_pos, monitor, scale_x, scale_y)

                    # Convert to JPEG
                    jpeg_buffer = io.BytesIO()
                    pil_img.save(jpeg_buffer, format='JPEG', quality=STREAM_QUALITY)
                    jpeg_data = jpeg_buffer.getvalue()

                    # Send frame
                    try:
                        self.wfile.write(b'--frame\r\n')
                        self.wfile.write(b'Content-Type: image/jpeg\r\n')
                        self.wfile.write(f'Content-Length: {len(jpeg_data)}\r\n'.encode())
                        self.wfile.write(b'\r\n')
                        self.wfile.write(jpeg_data)
                        self.wfile.write(b'\r\n')
                    except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
                        print(f"Client disconnected: {self.client_address}")
                        client_connected = False
                        break

                    # Frame rate control
                    elapsed = time.time() - start_time
                    sleep_time = (1.0 / STREAM_FPS) - elapsed
                    if sleep_time > 0:
                        time.sleep(sleep_time)

        except Exception as e:
            print(f"Stream error: {e}")


def start_stream(target_ip=None):
    """Start HTTP streaming server."""
    global streaming, http_server

    if not HAS_MSS or not HAS_PIL:
        print("Error: Required libraries not installed!")
        return False

    with stream_lock:
        if streaming:
            return True

        try:
            http_server = HTTPServer(('0.0.0.0', STREAM_PORT), MJPEGHandler)
            http_server.timeout = None  # No timeout - wait forever

            streaming = True

            # Start server in background thread - serve forever
            def serve():
                print(f"HTTP server listening on port {STREAM_PORT}")
                try:
                    http_server.serve_forever()
                except Exception as e:
                    print(f"Server error: {e}")

            thread = threading.Thread(target=serve, daemon=True)
            thread.start()

            print(f"Stream started on port {STREAM_PORT}")
            print(f"URL: http://0.0.0.0:{STREAM_PORT}")
            return True

        except Exception as e:
            print(f"Error starting stream: {e}")
            return False


def stop_stream():
    """Stop HTTP streaming server."""
    global streaming, http_server

    with stream_lock:
        streaming = False

        if http_server:
            try:
                http_server.shutdown()
            except:
                pass
            http_server = None

        print("Stream stopped")


def get_monitors_info():
    """Get list of available monitors."""
    monitors = []
    try:
        with mss.mss() as sct:
            for i, mon in enumerate(sct.monitors):
                if i == 0:  # Skip "all monitors" virtual monitor
                    continue
                monitors.append({
                    'index': i,
                    'width': mon['width'],
                    'height': mon['height'],
                    'left': mon['left'],
                    'top': mon['top']
                })
    except:
        pass
    return monitors


def handle_command(data, addr):
    """Handle incoming control command."""
    global current_monitor

    try:
        cmd = json.loads(data)
        action = cmd.get('action', '')

        print(f"Command from {addr}: {action}")

        if action == 'start_stream':
            target = cmd.get('target')
            monitor = cmd.get('monitor', current_monitor)
            current_monitor = monitor
            success = start_stream(target)
            return {'status': 'ok' if success else 'error', 'streaming': streaming, 'monitor': current_monitor}

        elif action == 'stop_stream':
            stop_stream()
            return {'status': 'ok', 'streaming': False}

        elif action == 'set_monitor':
            monitor = cmd.get('monitor', 1)
            current_monitor = monitor
            print(f"Monitor set to: {current_monitor}")
            return {'status': 'ok', 'monitor': current_monitor}

        elif action == 'list_monitors':
            monitors = get_monitors_info()
            return {'status': 'ok', 'monitors': monitors, 'current': current_monitor}

        elif action == 'status':
            return {'status': 'ok', 'streaming': streaming, 'port': STREAM_PORT, 'monitor': current_monitor}

        elif action == 'click':
            # Get relative position (0.0 - 1.0)
            rel_x = cmd.get('x', 0.5)
            rel_y = cmd.get('y', 0.5)
            # Use monitor from command, or fall back to current_monitor
            click_monitor = cmd.get('monitor', current_monitor)

            # Get monitor info
            try:
                with mss.mss() as sct:
                    mon_idx = min(click_monitor, len(sct.monitors) - 1)
                    monitor = sct.monitors[mon_idx]

                    # Calculate absolute screen position
                    abs_x = monitor['left'] + int(rel_x * monitor['width'])
                    abs_y = monitor['top'] + int(rel_y * monitor['height'])

                    print(f"Click at: ({abs_x}, {abs_y}) - relative: ({rel_x:.2f}, {rel_y:.2f})")

                    success = click_at(abs_x, abs_y)
                    return {'status': 'ok' if success else 'error', 'x': abs_x, 'y': abs_y}
            except Exception as e:
                return {'status': 'error', 'message': str(e)}

        else:
            return {'status': 'error', 'message': 'Unknown action'}

    except json.JSONDecodeError:
        return {'status': 'error', 'message': 'Invalid JSON'}
    except Exception as e:
        return {'status': 'error', 'message': str(e)}


def control_server():
    """Run UDP control server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', CONTROL_PORT))

    print(f"Control server listening on port {CONTROL_PORT}")

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            response = handle_command(data.decode('utf-8'), addr)

            # Send response back
            sock.sendto(json.dumps(response).encode(), addr)

        except Exception as e:
            print(f"Control server error: {e}")


def get_local_ip():
    """Get local IP address."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except:
        return "127.0.0.1"


def main():
    """Main entry point."""
    # Handle hidden mode
    hidden = False
    if sys.executable.endswith('pythonw.exe'):
        hidden = True
        sys.stdout = open(os.devnull, 'w')
        sys.stderr = open(os.devnull, 'w')

    if not HAS_MSS or not HAS_PIL:
        if not hidden:
            print("ERROR: Required libraries not installed!")
            print("Run: pip install mss pillow")
        return

    local_ip = get_local_ip()

    if not hidden:
        print(f"Screen Stream Server (Pure Python)")
        print(f"Local IP: {local_ip}")
        print(f"Control port: {CONTROL_PORT}")
        print(f"Stream port: {STREAM_PORT}")
        print(f"Stream resolution: {STREAM_WIDTH}x{STREAM_HEIGHT} @ {STREAM_FPS}fps")
        print("-" * 40)
        print("Waiting for commands...")
        print("Or press Ctrl+C to start stream manually...")

    # Start control server in background
    control_thread = threading.Thread(target=control_server, daemon=True)
    control_thread.start()

    # Main loop - keep alive
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        if not streaming:
            print("\nStarting stream manually...")
            start_stream()
            try:
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                pass
        stop_stream()


if __name__ == '__main__':
    main()
