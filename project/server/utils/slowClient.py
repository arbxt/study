import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))

s.sendall(b"big response please\n")
s.shutdown(socket.SHUT_WR)

total = 0

while True:
    chunk = s.recv(4096)
    if not chunk:
        break
    total += len(chunk)

print("total received:", total)

s.close()