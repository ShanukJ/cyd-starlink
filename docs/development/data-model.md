# Starlink data model

`starlink::StarlinkStatus` ([StarlinkStatus.h](../../firmware/src/starlink/StarlinkStatus.h))
is the only form in which dish telemetry reaches the UI. It is filled by
`decodeGetStatus()` ([StatusDecoder.cpp](../../firmware/src/starlink/StatusDecoder.cpp)),
which is pure C++ and unit-tested on the host.

## Unavailable is never zero

Every telemetry value is a `std::optional` (strings: `""`). The UI shows
`--` for an empty optional and must never substitute 0.

Proto3 complicates this, because it omits fields that equal their default
(0, false, ""). An absent field can therefore mean "zero" or "this firmware
doesn't send it". The decoder resolves every field with one of two policies,
and only when its **parent message is present**. If the parent is absent,
every field in it is unavailable.

| Policy | Absent field | Value 0 | Used for |
|---|---|---|---|
| zero is a value | 0 / false | 0 | throughput, drop rate, obstruction, GPS, alignment, flags, uptime |
| zero is unknown | unavailable | unavailable | latency (0 = no pings), Ethernet speed (0 = no link), signal quality (newer field, absent on older dishes), boot count, enum `UNKNOWN` values (disablement, update state) |

On top of the policy:

- A **wrong wire type** makes the field unavailable, not 0. This guards
  against a field changing type in a future API.
- **NaN / ±Inf** floats are unavailable. The dish really sends NaN, e.g. for
  `avg_prolonged_obstruction_interval_s`.
- **Implausible values are dropped:** fractions outside 0..1, elevation
  outside ±90°, tilt outside 0..90°, negative throughput, more than 200 GPS
  satellites.
- **Unknown fields are skipped.** Unknown alert flags that are set are
  counted in `Alerts::unrecognized`, so a new alert type is still reported.
- **Fallbacks:** pointing comes from `alignment_stats`, falling back to the
  older top-level boresight fields *only if the dish sent them*. The software
  update state comes from `software_update_stats`, falling back to the
  top-level enum.
- **Any malformed message fails the whole decode**, and no partial telemetry
  is returned.

The dish's device ID is never stored.

## Health

`evaluateHealth()` ([Health.cpp](../../firmware/src/starlink/Health.cpp)) turns
a snapshot into one state and one short reason. The first matching rule wins:

| State | When |
|---|---|
| CONNECTING | no WiFi yet, or no answer from the dish yet |
| OFFLINE | dish unreachable after retries · dish reports an outage (reason = outage cause, e.g. "No satellites") · thermal shutdown |
| ERROR | dish answers but is unusable: disablement code ≠ OKAY (e.g. "No active account") · not a dish · API error · undecodable data |
| DEGRADED | thermal throttling · motor problem · water detected · mast not vertical · unexpected location · Ethernet alerts · currently obstructed · ping drop ≥ 10 % · SNR persistently low |
| ONLINE | otherwise |

Missing telemetry never makes the state worse. A reachable dish with no
optional fields at all is ONLINE.

Each state comes from a single sample. Smoothing, if needed, belongs in the
UI layer.

## History

`HistoryBuffer` ([HistoryBuffer.h](../../firmware/src/starlink/HistoryBuffer.h))
keeps 15 minutes as 450 two-second slots, each holding download, upload and
latency.

- **Slots are tied to time, not to polls.** If polls fail, time still moves
  on and the slots stay empty (NaN), so outages show as gaps in the graph.
  Missing data is never drawn as 0.
- **Every successful status poll records into the current slot.**
- **After each (re)connection, the dish's own `get_history` (1 Hz) backfills
  only the empty slots.** Two dish samples are averaged per slot, and
  locally measured slots are never overwritten. A dish latency of 0 means no
  ping succeeded, so it is treated as missing.
- **The backfill needs one 22 KB block.** It is skipped, with the graphs
  starting empty, if the heap can't provide it. A failed allocation is
  reported as `OUT_OF_MEMORY` instead of aborting.

## Diagnostics checks

`evaluateChecks()` ([Diagnostics.cpp](../../firmware/src/starlink/Diagnostics.cpp))
maps the status onto nine checks, each with a level (OK / INFO / WARN / FAIL /
UNKNOWN) and a short detail. Every dish alert is routed to exactly one
check. Alerts without a natural home, including alert types added in newer
dish firmware, go to **ALERTS**, so none are hidden.

Things to know:

- **HARDWARE** uses the `scp`, `l1l2`, `xphy` and `aap` ready states.
  `cady` is ignored: the reference rev4 dish reports it as false while
  working normally.
- **RF** uses the `rf` ready state, `is_snr_persistently_low`,
  `lower_signal_than_predicted` and `is_snr_above_noise_floor`.
- **OBSTRUCTION** reports the share of sky but gives no verdict on it,
  because Starlink publishes no threshold. It only warns when the dish says
  it is obstructed right now.
- A check the dish doesn't report is UNKNOWN (`?` and `--`), never OK.
- Without a fresh poll, every check reads UNKNOWN. Old data is not shown as
  current.

## Tests

```sh
cd firmware
pio test -e native
```

These run on the development machine with AddressSanitizer and UBSan. They
use a real `get_status` response with its identifiers redacted
([test/fixtures](../../firmware/test/fixtures/README.md)). Expected values
were decoded independently with a separate Python decoder. The tests cover
every rule above, all 558 truncations of the real response, and 20 000
random mutations of it.
