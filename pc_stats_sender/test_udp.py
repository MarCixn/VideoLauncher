import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
target = '192.168.5.19'
port = 5556

for i in range(10):
    data = '{"cpu":50,"gpu":30,"ram":40,"gpu_temp":55}'
    s.sendto(data.encode(), (target, port))
    print(f"Sent to {target}:{port}")
    time.sleep(1)

s.close()
print("Done")
