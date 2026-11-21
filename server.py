import base64
import hashlib
import socket
import socketserver
import struct
import threading
import time

PORT = 8765
players = {}
clients = {}
next_id = 1
free_ids = []
last_hit = {}
lock = threading.Lock()


def send_raw(conn, text):
    conn.sendall((text + "\n").encode())


def ws_frame(text):
    b = text.encode()
    head = bytearray([0x81])
    if len(b) < 126:
        head.append(len(b))
    elif len(b) < 65536:
        head.append(126)
        head += struct.pack("!H", len(b))
    else:
        head.append(127)
        head += struct.pack("!Q", len(b))
    return bytes(head) + b


def read_ws(conn, first):
    buf = first
    while True:
        if len(buf) >= 2:
            n = buf[1] & 127
            off = 2
            if n == 126:
                n = struct.unpack("!H", buf[2:4])[0] if len(buf) >= 4 else None
                off = 4
            elif n == 127:
                n = struct.unpack("!Q", buf[2:10])[0] if len(buf) >= 10 else None
                off = 10
            if n is not None:
                masked = buf[1] & 128
                start = off + (4 if masked else 0)
                need = start + n
                if len(buf) >= need:
                    payload = buf[start:need]
                    rest = buf[need:]
                    if masked:
                        key = buf[off:off + 4]
                        payload = bytes(c ^ key[i % 4] for i, c in enumerate(payload))
                    opcode = buf[0] & 15
                    if opcode == 8:
                        return None, rest
                    if opcode == 1:
                        return payload.decode(errors="ignore"), rest
                    buf = rest
                    continue
        chunk = conn.recv(4096)
        if not chunk:
            return None, b""
        buf += chunk


def broadcast(text, skip=None):
    with lock:
        targets = list(clients.items())
    for cid, send in targets:
        if cid != skip:
            try:
                send(text)
            except OSError:
                pass


def handle(line, cid):
    parts = line.split()
    if not parts:
        return
    if parts[0] == "S" and len(parts) >= 8:
        pid, name = int(parts[1]), parts[2]
        with lock:
            if pid in players:
                players[pid].update(name=name, x=float(parts[3]), y=float(parts[4]),
                                    z=float(parts[5]), yaw=float(parts[6]), hp=int(float(parts[7])))
        broadcast("P " + " ".join(parts[1:8]), pid)
    elif parts[0] == "F" and len(parts) == 8:
        broadcast(line, int(parts[1]))
    elif parts[0] == "H" and len(parts) == 3:
        src, dst = int(parts[1]), int(parts[2])
        if src == dst:
            return
        key = tuple(sorted((src, dst)))
        now = time.monotonic()
        snap = ""
        with lock:
            if now - last_hit.get(key, 0) < 0.12:
                return
            last_hit[key] = now
            if dst in players:
                p = players[dst]
                p["hp"] -= 10
                snap = f"P {dst} {p['name']} {p['x']} {p['y']} {p['z']} {p['yaw']} {p['hp']}"
        broadcast(line, src)
        broadcast(f"D {dst} {src}")
        if snap:
            broadcast(snap)


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        conn = self.request
        conn.settimeout(1.0)
        try:
            peek = conn.recv(4, socket.MSG_PEEK)
        except socket.timeout:
            peek = b""
        conn.settimeout(None)

        websocket = peek.startswith(b"GET ")
        first = b""
        if websocket:
            while b"\r\n\r\n" not in first:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                first += chunk
                if len(first) > 65536:
                    return
            key = ""
            for line in first.decode(errors="ignore").split("\r\n"):
                if line.lower().startswith("sec-websocket-key:"):
                    key = line.split(":", 1)[1].strip()
            accept = base64.b64encode(
                hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
            ).decode()
            _, _, rest = first.partition(b"\r\n\r\n")
            conn.sendall(
                b"HTTP/1.1 101 Switching Protocols\r\n"
                b"Upgrade: websocket\r\n"
                b"Connection: Upgrade\r\n"
                b"Sec-WebSocket-Accept: " + accept.encode() + b"\r\n\r\n" + rest
            )
            send = lambda s: conn.sendall(ws_frame(s + "\n"))
        else:
            rest = first
            send = lambda s: send_raw(conn, s)

        with lock:
            global next_id
            if free_ids:
                cid = free_ids.pop()
            else:
                cid = next_id
                next_id += 1
            clients[cid] = send
            players[cid] = {"name": "capy", "x": 0, "y": 0, "z": 0, "yaw": 0, "hp": 100}
        send(f"W {cid}")
        with lock:
            snapshot = [
                f"P {pid} {p['name']} {p['x']} {p['y']} {p['z']} {p['yaw']} {p['hp']}"
                for pid, p in players.items()
            ]
        for line in snapshot:
            send(line)

        buf = rest
        try:
            while True:
                if websocket:
                    line, buf = read_ws(conn, buf)
                    if line is None:
                        break
                    handle(line, cid)
                else:
                    while b"\n" in buf:
                        raw, buf = buf.split(b"\n", 1)
                        handle(raw.decode(errors="ignore"), cid)
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
        except (OSError, ValueError, IndexError):
            pass

        with lock:
            clients.pop(cid, None)
            players.pop(cid, None)
            free_ids.append(cid)
        broadcast(f"X {cid}")


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    with Server(("0.0.0.0", PORT), Handler) as server:
        print(f"capybara room on :{PORT}")
        server.serve_forever()
