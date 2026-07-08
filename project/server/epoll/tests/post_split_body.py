import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))

s.sendall(
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 11\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
)

print("sent headers only")
time.sleep(1)

# 此时服务器不应该响应
s.settimeout(0.5)
try:
    data = s.recv(4096)
    print("unexpected response:", data)
except socket.timeout:
    print("ok: no response before body complete")

s.settimeout(None)

s.sendall(b"hello ")
time.sleep(1)

s.settimeout(0.5)
try:
    data = s.recv(4096)
    print("unexpected response:", data)
except socket.timeout:
    print("ok: still no response before full body")

s.settimeout(None)

s.sendall(b"world")

data = s.recv(4096)
print(data.decode(errors="ignore"))

s.close()