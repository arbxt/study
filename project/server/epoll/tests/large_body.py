import socket

body_size = 2 * 1024 * 1024

s = socket.socket()
s.connect(("127.0.0.1", 8080))

req = (
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: " + str(body_size).encode() + b"\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
)

s.sendall(req)

data = s.recv(4096)
print(data.decode(errors="ignore"))

s.close()