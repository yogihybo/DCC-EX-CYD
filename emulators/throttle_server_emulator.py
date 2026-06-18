import socketserver
import os
import json
from http.server import SimpleHTTPRequestHandler
from urllib.parse import urlparse

# Paths relative to the repo root (one level up from this script)
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WWW_DIR   = os.path.join(REPO_ROOT, "data", "www")
SD_DIR    = os.path.join(REPO_ROOT, "sd")
DATA_DIR  = os.path.join(REPO_ROOT, "data")


class ThrottleHandler(SimpleHTTPRequestHandler):

    def _clean_path(self, raw_path):
        """Strip /$ prefix (mirrors real server) and return the path component."""
        path = urlparse(raw_path).path
        if path.startswith("/$/"):
            path = path[2:]
        return path

    def _send_json(self, payload):
        body = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _sd_path(self, path):
        return os.path.join(SD_DIR, path.lstrip("/"))

    def translate_path(self, path):
        """Serve static files from data/www, then data/, fallback to sd/."""
        clean = urlparse(path).path
        if clean.startswith("/$/"):
            clean = clean[2:]  # strip /$ to get real path
        www_target = os.path.join(WWW_DIR, clean.lstrip("/"))
        if os.path.exists(www_target):
            return www_target
        data_target = os.path.join(DATA_DIR, clean.lstrip("/"))
        if os.path.exists(data_target):
            return data_target
        sd_target = os.path.join(SD_DIR, clean.lstrip("/"))
        if os.path.exists(sd_target):
            return sd_target
        return ""

    def do_HEAD(self):
        path = self._clean_path(self.path)

        if path == "/cs":
            self.send_response(204)
        elif path == "/throttle-programming":
            # Always report programming mode as active in the emulator
            self.send_response(200)
            self.send_header("Cache-Control", "no-store")
        elif os.path.exists(self._sd_path(path)):
            self.send_response(204)
        else:
            self.send_response(404)
        self.end_headers()

    def do_GET(self):
        path = self._clean_path(self.path)

        if path == "/":
            path = "/index.html"

        if path == "/cs":
            self._send_json({
                "ssid": "my_ssid",
                "password": "my_password",
                "server": "dccex",
                "port": 2560,
                "storageMode": 0,
                "has_sd": os.path.isdir(SD_DIR),
                "version": "v0.0.0",
                "platform": "Emulator",
                "free_ram": 999,
                "ip": "127.0.0.1",
            })
            return

        if path in ("/locos", "/fns"):
            items = []
            dir_path = self._sd_path(path)
            if os.path.isdir(dir_path):
                for entry in os.scandir(dir_path):
                    if entry.is_file() and entry.name.endswith(".json"):
                        try:
                            with open(entry.path) as f:
                                data = json.load(f)
                            items.append({"file": path + "/" + entry.name, "name": data.get("name", "")})
                        except Exception:
                            pass
            self._send_json(items)
            return

        if path == "/icons":
            icons = []
            # Built-in icons from data/icons/ — served with /$ prefix (read-only)
            builtin_dir = os.path.join(DATA_DIR, "icons")
            if os.path.isdir(builtin_dir):
                for entry in sorted(os.scandir(builtin_dir), key=lambda e: e.name):
                    if entry.is_file() and entry.name.endswith(".bmp"):
                        icons.append("/$" + "/icons/" + entry.name)
            # User icons from sd/icons/ — served without /$ prefix (deleteable)
            sd_icons_dir = self._sd_path("/icons")
            if os.path.isdir(sd_icons_dir):
                for entry in sorted(os.scandir(sd_icons_dir), key=lambda e: e.name):
                    if entry.is_file() and entry.name.endswith(".bmp"):
                        icons.append("/icons/" + entry.name)
            self._send_json(icons)
            return

        return SimpleHTTPRequestHandler.do_GET(self)

    def do_PUT(self):
        path = self._clean_path(self.path)

        if path == "/cs":
            self.send_response(204)
            self.end_headers()
            return

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        target = self._sd_path(path)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as f:
            f.write(body)

        self.send_response(204)
        self.end_headers()

    def do_DELETE(self):
        path = self._clean_path(self.path)

        # Icons are read-only in the emulator
        if path.startswith("/icons/"):
            self.send_response(404)
            self.end_headers()
            return

        target = self._sd_path(path)
        if os.path.exists(target):
            os.unlink(target)
            self.send_response(204)
        else:
            self.send_response(404)
        self.end_headers()

    def log_message(self, fmt, *args):
        print(f"[Web] {self.command} {self.path} -> {args[1] if len(args) > 1 else ''}")


if __name__ == "__main__":
    port = 8000
    with socketserver.TCPServer(("", port), ThrottleHandler) as httpd:
        httpd.allow_reuse_address = True
        print(f"Throttle server emulator running at http://localhost:{port}")
        print(f"  Serving web assets from: {WWW_DIR}")
        print(f"  Serving SD data from:    {SD_DIR}")
        httpd.serve_forever()
