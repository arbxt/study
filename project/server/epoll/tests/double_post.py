import socket

def recv_http_response(s):
    data = b""

    while b"\r\n\r\n" not in data:
        chunk = s.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before response header complete")
        data += chunk

    head, rest = data.split(b"\r\n\r\n", 1)

    content_length = None
    for line in head.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            content_length = int(line.split(b":", 1)[1].strip())

    if content_length is None:
        raise RuntimeError("no Content-Length in response")

    body = rest
    while len(body) < content_length:
        chunk = s.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before response body complete")
        body += chunk

    current_body = body[:content_length]
    extra = body[content_length:]

    return head, current_body, extra


s = socket.socket()
s.connect(("127.0.0.1", 8080))

req = (
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 5\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
    b"hello"
    b"POST /echo HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 5\r\n"
    b"Content-Type: text/plain\r\n"
    b"\r\n"
    b"world"
)

s.sendall(req)

# 简化：第一次读响应
data = b""
while data.count(b"HTTP/1.1") < 2:
    chunk = s.recv(4096)
    if not chunk:
        break
    data += chunk

print(data.decode(errors="ignore"))

s.close()