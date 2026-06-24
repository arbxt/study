import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))

s.sendall(b"hello\n")

print("sent request, now sleep without reading...")
time.sleep(10)

print("start reading...")
total = 0
while True:
    data = s.recv(4096)
    if not data:
        break
    total += len(data)
    if total % (1024 * 1024) < 4096:
        print("received", total, "bytes")

print("done, total =", total)
s.close()