# Schedule API (CRUD)

This document describes the four schedule endpoints exposed by the ESP32 MAC‑SYS controller. The schedule is persisted to EEPROM and automatically loaded at boot.

Status: Active (CRUD only)

- Scope: Endpoints operate on zone `0`.
- Time format: `"HH:MM"` (24‑hour).
- Days bitmask: `bit0=Sunday(1)`, `bit1=Monday(2)`, `bit2=Tuesday(4)`, `bit3=Wednesday(8)`, `bit4=Thursday(16)`, `bit5=Friday(32)`, `bit6=Saturday(64)`. Combine with OR (e.g., Mon–Fri = `2+4+8+16+32 = 62`).
- Event fields: 
  - `enabled` (bool, default `true`)
  - `time` (string, required) — e.g., `"08:00"`
  - `setpoint` (float, default `24.0`)
  - `delta` (float, default `1.0`)
  - `days` (uint8 bitmask, default `1` for Sunday)
  - `description` (string, default `"Schedule Event"`)

---

## GET /api/schedule
Read the current persisted schedule for zone 0.

Response 200 (application/json):
```json
{
  "global_enabled": true,
  "zone_enabled": true,
  "zone_name": "Office",
  "active_events": 2,
  "events": [
    {
      "enabled": true,
      "time": "08:00",
      "setpoint": 24.0,
      "delta": 1.0,
      "days": 2,
      "description": "Monday Morning"
    },
    {
      "enabled": true,
      "time": "09:00",
      "setpoint": 23.5,
      "delta": 1.0,
      "days": 4,
      "description": "Tuesday Morning"
    }
  ]
}
```

Example:
```bash
curl -s http://192.168.1.2/api/schedule | python3 -m json.tool
```

---

## POST /api/schedule
Create/replace the full schedule for zone 0. Accepts either a single event object or an array of event objects. Automatically persists to EEPROM. If at least one event is provided, the scheduler (global and zone) is enabled.

Request (application/json) — single event:
```json
{
  "enabled": true,
  "time": "08:00",
  "setpoint": 24.0,
  "delta": 1.0,
  "days": 2,
  "description": "Monday 8am"
}
```

Request (application/json) — multiple events:
```json
[
  {"enabled": true, "time": "07:00", "setpoint": 22.0, "delta": 1.0, "days": 1,  "description": "Sun"},
  {"enabled": true, "time": "08:00", "setpoint": 23.0, "delta": 1.0, "days": 2,  "description": "Mon"},
  {"enabled": true, "time": "09:00", "setpoint": 24.0, "delta": 1.0, "days": 4,  "description": "Tue"}
]
```

Example:
```bash
curl -X POST http://192.168.1.2/api/schedule \
  -H "Content-Type: application/json" \
  -d '{
        "enabled": true,
        "time": "08:00",
        "setpoint": 24.0,
        "delta": 1.0,
        "days": 2,
        "description": "Monday 8am"
      }'
```

Success response 200:
```json
{"success": true, "message": "Schedule saved to EEPROM"}
```

Error response 400 (examples):
```json
{"success": false, "error": "No data"}
{"success": false, "error": "Invalid JSON"}
{"success": false, "error": "No events saved"}
```

Notes:
- This endpoint fully replaces the schedule for zone 0. To add/modify incrementally, re‑POST the full set you want to keep.

---

## PUT /api/schedule
Idempotent update (also replaces the full schedule for zone 0). Same request and response format as POST. Use PUT when your client prefers idempotent semantics for replacing all events.

Example:
```bash
curl -X PUT http://192.168.1.2/api/schedule \
  -H "Content-Type: application/json" \
  -d '[
        {"enabled": true, "time": "07:00", "setpoint": 22.0, "delta": 1.0, "days": 1,  "description": "Sun"},
        {"enabled": true, "time": "08:00", "setpoint": 23.0, "delta": 1.0, "days": 2,  "description": "Mon"}
      ]'
```

Response:
```json
{"success": true, "message": "Schedule saved to EEPROM"}
```

---

## DELETE /api/schedule
Delete (clear) all events for zone 0 and persist.

Example:
```bash
curl -X DELETE http://192.168.1.2/api/schedule
```

Success response 200:
```json
{"success": true, "message": "Schedule cleared"}
```

---

## Persistence & Behavior
- All changes are persisted via EEPROM (`saveConfig()`) and loaded on boot (`loadConfig()`).
- Events are validated and sorted by time. Conflicting or invalid submissions may be rejected.
- When at least one event is saved (POST/PUT), zone 0 and the global scheduler are enabled automatically.
- If you need per‑event updates without replacing all events, re‑POST/PUT the complete array reflecting your updates.

---

## Removed/Legacy Endpoints
The following endpoints are removed in CRUD‑only mode:
- `/api/schedule/event` (POST/DELETE)
- `/api/schedule/export`, `/api/schedule/import`
- `/api/schedule/global`, `/api/schedule/zone`, `/api/schedule/holiday`
- `/api/schedule/control-mode`, `/api/schedule/setpoint-mode`, `/api/schedule/toggle`

Use only the CRUD endpoints documented above.
