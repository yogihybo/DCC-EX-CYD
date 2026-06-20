import socket
import re
from threading import Thread
from zeroconf import Zeroconf, ServiceInfo

sockets = []

def broadcast(msg):
    for s in list(sockets):
        try:
            s.send(msg)
        except Exception:
            sockets.remove(s)

def split_commands(raw: str) -> list[str]:
    """Split a raw DCC-EX stream into individual <...> commands."""
    return re.findall(r'<[^>]+>', raw)

def handle_cmd(c: socket.socket, cmd: str):
    if len(cmd) < 2:
        c.send(b"<X>\n")
        return

    print("<< " + cmd)

    match cmd[1]:
        case "0":
            if len(cmd) > 3 and cmd[3] == "M":
                c.send(b"<p0 MAIN>\n")
            elif len(cmd) > 3 and cmd[3] == "P":
                c.send(b"<p0 PROG>\n")
            else:
                c.send(b"<p0>\n")

        case "1":
            if len(cmd) > 3 and cmd[3] == "M":
                c.send(b"<p1 MAIN>\n")
            elif len(cmd) > 3 and cmd[3] == "P":
                c.send(b"<p1 PROG>\n")
            elif len(cmd) > 3 and cmd[3] == "J":
                c.send(b"<p1 JOIN>\n")
            else:
                c.send(b"<p1>\n")

        case "s":
            c.send(b"<iDCC-EX V-5.2.76 / MEGA / STANDARD_MOTOR_SHIELD / 12345>\n")

        case "W":
            c.send(b"<w 4321>\n")

        case "R":
            c.send(b"<r 1423>\n")

        case "B":
            c.send(b"<r12345|32767|3 2 1 >\n")

        case "F":
            c.send(b"<l 1 1 146 7>\n")

        case "l":
            c.send(b"<l 1 1 0 0>\n")

        case "t":
            # <t ADDR SPEED DIR>
            r = re.search(r"<t (?P<l>\d+) (?P<s>\d+) (?P<d>\d+)>", cmd)
            if r:
                speed_dir = (int(r.group("s")) + 1 & 0x7F) + int(r.group("d")) * 128
                reply = f"<l {r.group('l')} 0 {speed_dir} 0>\n"
                print(">> " + reply.strip())
                broadcast(reply.encode())
            else:
                c.send(b"<l 1 1 0 1>\n")

        case "U":
            c.send(b"<p1 MAIN>\n")

        case "^":
            # CS consist: <^ addr1 [-]addr2 ...> = create/update, <^ addr> = delete
            inner = cmd[3:-1].strip()
            parts = inner.split()
            if len(parts) >= 2:
                # Echo back as CS confirmation so the library processes the consist
                reply = cmd + "\n"
                print(">> " + cmd)
                broadcast(reply.encode())
            elif len(parts) == 1:
                c.send(b"<O>\n")
            else:
                c.send(b"<X>\n")

        case _:
            c.send(b"<X>\n")

def throttle(c: socket.socket):
    sockets.append(c)
    buf = ""

    while True:
        try:
            data = c.recv(1024)
        except (ConnectionAbortedError, ConnectionResetError):
            break
        if not data:
            break

        buf += data.decode(errors="replace")
        cmds = split_commands(buf)
        # Trim processed commands from buffer; keep any partial tail
        last = buf.rfind('>')
        buf = buf[last + 1:] if last >= 0 else buf

        for cmd in cmds:
            handle_cmd(c, cmd)

    c.close()
    if c in sockets:
        sockets.remove(c)


s = socket.socket()
port = 2560

s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', port))
s.listen(5)

# Advertise via mDNS as _dccex._tcp, matching real DCC-EX CommandStation behaviour
local_ip = socket.gethostbyname(socket.gethostname())
zc = Zeroconf()
mdns_info = ServiceInfo(
    "_dccex._tcp.local.",
    "DCC-EX._dccex._tcp.local.",
    addresses=[socket.inet_aton(local_ip)],
    port=port,
    properties={"version": "5.2.76"},
)
zc.register_service(mdns_info)
print(f"CS emulator listening on port {port}, mDNS advertising as DCC-EX._dccex._tcp.local ({local_ip})")

try:
    while True:
        c, addr = s.accept()
        print("Got connection from", addr)
        Thread(target=throttle, args=(c,), daemon=True).start()
finally:
    zc.unregister_service(mdns_info)
    zc.close()
