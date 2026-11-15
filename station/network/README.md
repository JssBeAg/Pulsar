# Station network (PULSAR-BASE)

The lunar base's local network, simulated with an ESP32 running as a WiFi
access point. The station computers and the EVA-LINK ESP32 connect to it.

| Setting | Value |
|---|---|
| SSID | `PULSAR-BASE` |
| Password | `pulsar-base-2026` (demo only) |
| Access point IP | `10.10.0.1` |
| Subnet | `10.10.0.0/24` (clients get `10.10.0.x` by DHCP) |
| Channel | 6 (2.4 GHz; the ESP32 has no 5 GHz) |
| Max clients | 4 |

The subnet is not `192.168.4.0/24` because EVA-LINK already uses it, and
EVA-LINK is on both networks at once. Overlapping subnets would break routing.

## What it does and does not do

- It has **no internet**: no upstream connection, no NAT, no routing. It only
  lets its clients reach each other.
- The **EVA-LINK ESP32** (`comms/access-point/`) joins it as a client, and
  forwards the astronauts' sensor traffic onto it.
- **Up to 4 clients.** One slot is for EVA-LINK, because without it no sensor
  data reaches the base. The other three are for station computers: the laptop
  that runs Mosquitto and the ingestor, and laptops that only open the
  dashboard in a browser (they do not need to run the broker).

## Build and flash

```
pio run -d station/network
pio run -d station/network -t upload
pio device monitor -d station/network
```

The serial monitor prints the SSID, IP, channel and client limit at boot, one
line when a client connects or disconnects, and every 5 s the client list
with each client's MAC and IP.

## Test: can two laptops reach each other?

The test tool needs only Python 3 (standard library).

1. Flash this firmware and open the serial monitor.
2. Connect laptop A and laptop B to `PULSAR-BASE`. Their IPs appear in the
   serial monitor, or run `ipconfig` (Windows) / `ip addr` (Linux) on each.
3. On laptop A:
   ```
   python station/network/tools/test_clients.py --serve
   ```
4. On laptop B, with the IP of A:
   ```
   python station/network/tools/test_clients.py --peer <IP of A>
   ```
5. Both summary lines must say `PASS`:
   ```
   ping <IP of A>: PASS (echo replies received)
   tcp <IP of A>:8000: PASS (connected, reply 'pulsar-ok')
   ```
6. Stop the server on A with Ctrl+C.

If a check fails on Windows, check the firewall on laptop A first:

- Windows marks a new WiFi network as **Public**, and on that profile it
  blocks incoming ping by default. Set `PULSAR-BASE` to Private.
- Python needs to be allowed through the firewall to accept connections on
  port 8000. Windows asks the first time `--serve` runs.
- A TCP `TimeoutError` means nothing answered (wrong IP or a firewall
  dropping the packets). `ConnectionRefusedError` means A is reachable but
  `--serve` is not running.

## Client limit test

With laptops A and B still connected, connect a third laptop, then a fourth:
both must connect, and the serial monitor shows `clients=4/4`. A fifth device
must be refused.
