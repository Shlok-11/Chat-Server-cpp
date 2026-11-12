"""Wire-protocol helpers for the C++ TCP chat server.

Protocol: 4-byte unsigned big-endian payload length, then UTF-8 bytes.
Only standard-library modules are required.
"""
import socket
import struct
import time

MAX_FRAME_BYTES = 1024 * 1024


def send_frame(sock: socket.socket, message: str) -> None:
    data = message.encode("utf-8")
    if len(data) > MAX_FRAME_BYTES:
        raise ValueError("test message exceeds maximum test frame size")
    sock.sendall(struct.pack("!I", len(data)) + data)


def read_exact(sock: socket.socket, n: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < n:
        part = sock.recv(n - len(chunks))
        if not part:
            raise ConnectionError("peer closed before complete frame")
        chunks.extend(part)
    return bytes(chunks)


def recv_frame(sock: socket.socket) -> str:
    length = struct.unpack("!I", read_exact(sock, 4))[0]
    if length > MAX_FRAME_BYTES:
        raise ValueError(f"unexpected frame length: {length}")
    return read_exact(sock, length).decode("utf-8", errors="strict")


def recv_until(sock: socket.socket, predicate, timeout: float = 8.0) -> str:
    """Ignore asynchronous join/leave notifications until expected response arrives."""
    end = time.monotonic() + timeout
    old_timeout = sock.gettimeout()
    try:
        while True:
            remaining = end - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("expected reply did not arrive within timeout")
            sock.settimeout(remaining)
            message = recv_frame(sock)
            if predicate(message):
                return message
    finally:
        sock.settimeout(old_timeout)


def connect_user(host: str, port: int, username: str, timeout: float = 8.0) -> socket.socket:
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    try:
        send_frame(sock, f"CONNECT:{username}")
        answer = recv_until(sock, lambda s: s.startswith(("OK:", "ERR:")), timeout)
        if not answer.startswith("OK:"):
            raise RuntimeError(f"username {username!r} rejected: {answer}")
        return sock
    except Exception:
        sock.close()
        raise


def close_user(sock: socket.socket) -> None:
    try:
        send_frame(sock, "QUIT")
    except (OSError, ValueError):
        pass
    finally:
        sock.close()


def online_users(sock: socket.socket, timeout: float = 8.0) -> set[str]:
    send_frame(sock, "LIST")
    reply = recv_until(sock, lambda s: s.startswith(("USERLIST:", "ERR:")), timeout)
    if not reply.startswith("USERLIST:"):
        raise AssertionError(f"LIST returned {reply!r}")
    text = reply[len("USERLIST:"):]
    return set(filter(None, text.split(",")))
