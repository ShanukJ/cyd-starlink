# Test fixtures

## get_status_api43_redacted.pb

A real `SpaceX.API.Device.Response` to `get_status` (the protobuf inside the
gRPC-Web data frame), captured October 2026 from a `rev4_panda_prod2` dish on
firmware `2026.09.18.mr87170.1.55184.2`, api_version 43.

Redacted before committing:

- the dish ID and router IDs (`device_info.id`, `connected_routers`,
  `downstream_routers` keys) were overwritten with same-length placeholders
  (`REDACTED-ID-n…`), so every length prefix stays valid;
- the country code was replaced with `XX`.

`get_status_api43.h` is the same bytes as a C array:

```sh
xxd -i -n kGetStatusApi43 get_status_api43_redacted.pb
```

(followed by changing the types to `static const uint8_t` / `size_t`).

When adding a fixture from another dish or firmware, redact the same fields
and check that no identifiers remain before committing.
