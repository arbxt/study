import socket

s = socket.socket()
s.connect(("127.0.0.1", 8080))

big_header = b"A" * (20 * 1024)

req = (
    b"GET /ping HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"X-Big: " + big_header + b"\r\n"
    b"\r\n"
)

s.sendall(req)

data = s.recv(4096)
print(data.decode(errors="ignore"))

s.close()