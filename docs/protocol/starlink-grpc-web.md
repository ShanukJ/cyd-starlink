# Starlink local API: gRPC-Web transport

How the firmware talks to the dish, and what was observed on real hardware.
Verified October 2026 against a `rev4_panda_prod2` dish running
`2026.09.18.mr87170.1.55184.2`, which reports `api_version` 43.

## Endpoints

The dish at `192.168.100.1` serves the same service on two ports:

| Port | Protocol | Notes |
|---|---|---|
| 9200 | native gRPC (HTTP/2, h2c) | used by `grpcurl`; supports server reflection |
| **9201** | **gRPC-Web over HTTP/1.1** | used by the Starlink web app, and by this firmware |

The firmware uses 9201: one plain HTTP/1.1 POST per call. Native gRPC would
need an HTTP/2 + HPACK client on the ESP32 for no benefit.

The Starlink **router** (`192.168.1.1` on a default setup) does not accept
connections on 9201.

## Request

```
POST /SpaceX.API.Device.Device/Handle HTTP/1.1
Host: 192.168.100.1:9201
Content-Type: application/grpc-web+proto
X-Grpc-Web: 1
Content-Length: 8
Connection: close

00 00000003 E23E00
```

The body is one gRPC-Web frame: a flag byte (`00` = data), a 4-byte
big-endian length, then the protobuf `SpaceX.API.Device.Request`. Only
`Content-Type` is actually required. `X-Grpc-Web` is sent for compatibility.

`get_status` is field **1004** of `Request` (a `GetStatusRequest`, sent empty).
Its tag is `(1004 << 3) | 2 = 8034`, which encodes as varint `E2 3E`, followed
by length `00`.

## Response

Success, which is always `Transfer-Encoding: chunked` (even for HTTP/1.0
requests):

```
HTTP/1.1 200 OK
Content-Type: application/grpc-web+proto
Transfer-Encoding: chunked

00 0000022F <559-byte SpaceX.API.Device.Response>
80 00000010 "grpc-status: 0\r\n"
```

Errors are *trailers-only*: HTTP **200**, an empty body, and the status in
the HTTP headers:

| Request | `Grpc-Status` | `Grpc-Message` |
|---|---|---|
| unknown request field | 12 | `Unimplemented: <nil>` |
| garbage body | 8 | `grpc: received message larger than max (…)` |

A client therefore has to check `grpc-status` in both places: the HTTP
headers and the trailer frame.

## Messages used

Field numbers come from the dish's own reflection
(`grpcurl -plaintext 192.168.100.1:9200 describe <type>`):

```
Response                 id=1  status=2 (Status{code=1,message=2})  api_version=3
                         dish_get_status=2004
DishGetStatusResponse    device_info=1  device_state=2
                         pop_ping_drop_rate=1003 (float)
                         obstruction_stats=1004  alerts=1005
                         downlink_throughput_bps=1007  uplink_throughput_bps=1008 (float)
                         pop_ping_latency_ms=1009 (float)
                         boresight_azimuth_deg=1011  boresight_elevation_deg=1012
                         gps_stats=1015  eth_speed_mbps=1016 (int32)
                         ready_states=1019  software_update_state=1021 (enum)
                         disablement_code=1024 (enum)  alignment_stats=1027
                         signal_quality=1057 (float)
DeviceInfo               id=1  hardware_version=2  software_version=3
DeviceState              uptime_s=1 (uint64)
```

To refresh this list after a dish firmware update, re-run reflection. The
parser skips unknown fields, so new fields never break it. Fields that are
renumbered or removed will simply read as unavailable.

## Proto3 default values

Proto3 does not transmit scalars that hold their default value (0, false,
""). An absent `downlink_throughput_bps` inside a present
`DishGetStatusResponse` therefore means **0 bps**, not "unknown". The data
model (Milestone 4) has to decide field by field whether an absent value is
a real zero or unavailable telemetry.

## Measured behaviour (ESP32, WiFi)

| | |
|---|---|
| Response size (`get_status`) | 559 B protobuf |
| Round trip, new TCP connection per call | 55–120 ms with WiFi power-save off; 40–335 ms with it on |
| Unreachable host | fails at the 2 s connect timeout |
| Heap over repeated calls | stable (no growth) |
