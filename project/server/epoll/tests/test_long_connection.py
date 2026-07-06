import socket

s = socket.socket()
s.connect(("127.0.0.1", 8080))

req1 = (
    "GET /ping HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "\r\n"
).encode()

req2 = (
    "GET /status HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "\r\n"
).encode()

s.sendall(req1)
data1 = s.recv(4096)
print(data1.decode(errors="ignore"))

s.sendall(req2)
data2 = s.recv(4096)
print(data2.decode(errors="ignore"))

s.close()