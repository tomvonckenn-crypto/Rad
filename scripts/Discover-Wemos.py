"""Find this project's Wemos on the current wifi, without switching networks."""
import argparse
import concurrent.futures
import ipaddress
import json
from pathlib import Path
import re
import socket
import subprocess
import urllib.request

PROJECT = Path(__file__).resolve().parent.parent


def probe(address):
    try:
        # Local device requests should not use an internet proxy.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(f"http://{address}/api/status", timeout=1.2) as response:
            status = json.loads(response.read(4096))
        known_device = status.get("device") == "rad-wemos" or status.get("version") == "0.1.0-netwerk"
        if known_device and status.get("ap_ip") == "192.168.4.1":
            return address, status
    except (OSError, ValueError):
        pass
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="Wi-Fi")
    parser.add_argument("--network", help="Explicit local subnet, e.g. 192.168.1.0/24")
    args = parser.parse_args()
    found = None
    try:
        found = probe(socket.gethostbyname("rad-wemos.local"))
    except OSError:
        pass
    if not found:
        network_text = args.network
        if not network_text:
            config = subprocess.check_output(
                ["netsh", "interface", "ipv4", "show", "config", f"name={args.interface}"],
                text=True,
                errors="replace",
            )
            match = re.search(r"\b(\d{1,3}(?:\.\d{1,3}){3}/\d{1,2})\b", config)
            if not match:
                raise SystemExit("Geen lokaal wifi-subnet gevonden. Geef --network op.")
            network_text = match.group(1)
        network = ipaddress.ip_network(network_text, strict=False)
        if network.version != 4 or network.num_addresses > 1024 or network.is_global:
            raise SystemExit("Geef een klein lokaal IPv4-subnet op (maximaal 1024 adressen).")
        print(f"Zoeken naar Wemos op {network}...", flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=32) as pool:
            results = pool.map(probe, map(str, network.hosts()))
            for result in results:
                if result:
                    found = result
                    break
    if not found:
        raise SystemExit("Geen Wemos-statuspagina gevonden. Controleer wifi-aanmelding en verkeer tussen hotspot-apparaten.")
    address, status = found
    if not status.get("station_connected") or status.get("station_ip") != address:
        raise SystemExit("Wemos gevonden, maar niet via zijn gewone wifi-verbinding.")
    local_folder = PROJECT / ".local"
    local_folder.mkdir(exist_ok=True)
    (local_folder / "wemos-address.txt").write_text(address + "\n", encoding="ascii")
    print(f"Wemos bereikbaar: http://{address}/")
    print(json.dumps(status, ensure_ascii=True))


if __name__ == "__main__":
    main()
