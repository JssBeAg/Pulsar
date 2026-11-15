"""Checks that two laptops on the station network (PULSAR-BASE) can reach each other.

Standard library only, so it runs on any station laptop with plain Python 3.

On laptop A (the one being reached):

    python station/network/tools/test_clients.py --serve

On laptop B:

    python station/network/tools/test_clients.py --peer <IP of A>

--serve listens on TCP port 8000 on all interfaces and answers every
connection with "pulsar-ok". --peer pings A, then opens a TCP connection to
A:8000 and checks that reply. A ping can pass while TCP fails (a firewall
blocks the port) and the reverse (a firewall blocks ICMP), so both are
checked and reported separately.
"""

import argparse
import platform
import socket
import subprocess
import sys

PORT = 8000
REPLY = b"pulsar-ok\n"
TCP_TIMEOUT_S = 3.0
PING_COUNT = 2
# accept() is given a timeout so the loop wakes up regularly: on Windows a
# blocking accept() does not see Ctrl+C until a connection arrives.
ACCEPT_POLL_S = 1.0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Station network reachability test between two laptops.")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--serve", action="store_true", help=f"answer TCP connections on port {PORT}")
    mode.add_argument("--peer", metavar="IP", help="check that the laptop at IP is reachable")
    return parser.parse_args()


def serve() -> None:
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", PORT))  # All IPv4 interfaces, including the PULSAR-BASE one
    server.listen()
    server.settimeout(ACCEPT_POLL_S)
    print(f"serving on 0.0.0.0:{PORT} (Ctrl+C to stop)", flush=True)
    try:
        while True:
            try:
                conn, (address, port) = server.accept()
            except socket.timeout:
                continue
            with conn:
                print(f"connection from {address}:{port}", flush=True)
                conn.sendall(REPLY)
    except KeyboardInterrupt:
        pass
    finally:
        server.close()
        print("server stopped", flush=True)


def check_ping(ip: str) -> tuple[bool, str]:
    """Pings ip PING_COUNT times. Returns (passed, reason)."""
    count_flag = "-n" if platform.system() == "Windows" else "-c"
    try:
        result = subprocess.run(["ping", count_flag, str(PING_COUNT), ip], capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        return False, f"could not run ping: {error}"
    print(result.stdout.strip(), flush=True)
    # Windows ping exits with 0 even for "Destination host unreachable",
    # because that is a reply from somebody. Only a line with a TTL is an
    # echo reply from the target itself.
    if result.returncode == 0 and "ttl=" in result.stdout.lower():
        return True, "echo replies received"
    return False, f"no echo reply (exit code {result.returncode})"


def check_tcp(ip: str) -> tuple[bool, str]:
    """Connects to ip:PORT and checks the reply. Returns (passed, reason)."""
    try:
        with socket.create_connection((ip, PORT), timeout=TCP_TIMEOUT_S) as conn:
            data = b""
            while not data.endswith(b"\n") and len(data) < 64:
                chunk = conn.recv(64)
                if not chunk:
                    break
                data += chunk
    except OSError as error:
        # The exact error tells the cases apart: a timeout means nothing
        # answered (wrong IP, firewall dropping); "refused" means the laptop
        # is reachable but --serve is not running.
        return False, f"{type(error).__name__}: {error}"
    reply = data.decode("utf-8", errors="replace").strip()
    if reply == REPLY.decode().strip():
        return True, f"connected, reply {reply!r}"
    return False, f"connected, but unexpected reply {reply!r}"


def peer(ip: str) -> int:
    print(f"--- ping {ip} ---", flush=True)
    ping_ok, ping_reason = check_ping(ip)
    print(f"--- tcp {ip}:{PORT} ---", flush=True)
    tcp_ok, tcp_reason = check_tcp(ip)
    print(f"tcp: {tcp_reason}", flush=True)

    print("--- summary ---")
    print(f"ping {ip}: {'PASS' if ping_ok else 'FAIL'} ({ping_reason})")
    print(f"tcp {ip}:{PORT}: {'PASS' if tcp_ok else 'FAIL'} ({tcp_reason})")
    return 0 if ping_ok and tcp_ok else 1


def main() -> None:
    args = parse_args()
    if args.serve:
        serve()
    else:
        sys.exit(peer(args.peer))


if __name__ == "__main__":
    main()
