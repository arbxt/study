import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))

req1 = (
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 5\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
    b"hello"
)

req2 = (
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 5\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
    b"world"
)

s.sendall(req1)
data1 = s.recv(4096)
print("response 1:")
print(data1.decode(errors="ignore"))

s.sendall(req2)
data2 = s.recv(4096)
print("response 2:")
print(data2.decode(errors="ignore"))

s.close()