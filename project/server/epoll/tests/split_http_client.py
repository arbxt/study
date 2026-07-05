import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))

s.sendall(b"GET /ping HTTP/1.1\r\n")
time.sleep(2)

# 这时服务器不应该响应，因为还没有 \r\n\r\n

s.sendall(b"Host: localhost\r\n")
time.sleep(2)

# 仍然不应该响应

s.sendall(b"\r\n")

data = b""
while True:
    chunk = s.recv(4096)
    if not chunk:
        break
    data += chunk

print(data.decode(errors="ignore"))
s.close()