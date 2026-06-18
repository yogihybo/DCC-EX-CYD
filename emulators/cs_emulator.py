import socket
import random
import re
import time
from threading import Thread

sockets = []

def broadcast(msg):
    for s in list(sockets):
        try:
            s.send(msg)
        except Exception:
            sockets.remove(s)

def throttle(c: socket.socket):
    sockets.append(c)

    while True:
        data = c.recv(1024)
        if not data:
            c.close()
            sockets.remove(c)
            break

        cmd = data.decode().strip()
        print("<< " + cmd)

        if len(cmd) < 2:
            c.send(b"<X>")
            continue

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
                # Match TeeStream parser: "iDCC-EX V-x.x.x / BOARD / SHIELD / BUILD"
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
                # Format: <t ADDR SPEED DIR>
                r = re.search(r"<t (?P<l>\d+) (?P<s>\d+) (?P<d>\d+)>", cmd)
                if r:
                    speed_dir = (int(r.group("s")) + 1 & 0x7F) + int(r.group("d")) * 128
                    reply = f"<l {r.group('l')} 0 {speed_dir} 0>\n"
                    print(">> " + reply)
                    broadcast(reply.encode())
                else:
                    c.send(b"<l 1 1 0 1>\n")

            case "U":
                c.send(b"<p1 MAIN>\n")

            case _:
                c.send(b"<X>\n")


s = socket.socket()
port = 2560

s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', port))
s.listen(5)

print(f"CS emulator listening on port {port}")

while True:
    c, addr = s.accept()
    print("Got connection from", addr)
    Thread(target=throttle, args=(c,), daemon=True).start()
