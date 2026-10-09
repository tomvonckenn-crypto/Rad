"""Local UI fixture only: this server never contacts the Wemos or motor."""
import json
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

PROJECT = Path(__file__).resolve().parent.parent
STATUS = dict(device="rad-wemos", version="0.2.0-handmatig", build="Lokale UI-proef",
              station_connected=True, station_ip="127.0.0.1:8765", ap_ip="192.168.4.1",
              ota_busy=False, ota_failed=False, state="locked", stop_reason="boot",
              direction="none", control_armed=False, motor_enabled=False,
              target_pwm=0, actual_pwm=0, rpwm=0, lpwm=0, max_pwm=300,
              safety_epoch=1, uptime_ms=0)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def respond(self, body, code=200, content_type="application/json"):
        raw = body.encode() if isinstance(body, str) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path == "/api/status":
            self.respond(STATUS)
        elif self.path == "/":
            source = (PROJECT / "include/control_page.h").read_text(encoding="utf-8")
            html = source.split('R"HTML(', 1)[1].split(')HTML";', 1)[0]
            html = html.replace("Handbediening</div>", "Lokale proef — geen motor aangesloten</div>")
            self.respond(html, content_type="text/html; charset=utf-8")
        else:
            self.respond({"error": "Onbekend adres"}, 404)

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        data = {k: v[0] for k, v in parse_qs(body).items()}
        if self.path == "/api/stop":
            STATUS.update(state="locked", stop_reason="emergency", direction="none",
                          control_armed=False, motor_enabled=False, target_pwm=0, actual_pwm=0)
            STATUS["safety_epoch"] += 1
            self.respond(STATUS)
        elif self.path == "/api/heartbeat" and data.get("token") == "42" and STATUS["control_armed"]:
            self.respond(STATUS)
        elif self.path == "/api/control":
            action = data.get("action")
            if action == "arm" and data.get("confirm") == "stil" and data.get("epoch") == str(STATUS["safety_epoch"]) and STATUS["state"] == "locked":
                STATUS.update(state="ready", stop_reason="none", control_armed=True)
                self.respond(dict(STATUS, session_token=42))
            elif data.get("token") == "42" and STATUS["control_armed"]:
                if action == "start" and STATUS["state"] == "ready":
                    STATUS.update(state="running", motor_enabled=True, direction=data["direction"])
                elif action == "step" and STATUS["state"] == "running":
                    STATUS["target_pwm"] = max(0, min(300, STATUS["target_pwm"] + int(data["delta"])))
                elif action == "slow_stop":
                    STATUS.update(state="await_standstill", stop_reason="stopped", control_armed=False, motor_enabled=False, target_pwm=0, actual_pwm=0)
                else:
                    self.respond({"error": "Commando geweigerd"}, 409)
                    return
                self.respond(STATUS)
            else:
                self.respond({"error": "Bediening vergrendeld"}, 403)
        else:
            self.respond({"error": "Bediening vergrendeld"}, 403)


if __name__ == "__main__":
    print("Lokale UI-proef op http://127.0.0.1:8765/ — geen Wemos-verbinding", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
