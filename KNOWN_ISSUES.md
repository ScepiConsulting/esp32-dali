# Known issues

Pre-existing bugs found but not fixed. Nothing here is urgent — there is no active DALI
project right now — but they should be resolved before the next DALI deployment.

---

## `DALI_QUERY_LIMIT_ERROR` is defined twice, with two different values

**Status:** open · **Severity:** likely a real protocol bug, not just a warning
**Affects:** both products (`bridge` and `ballast`)

Every build prints:

```
esp32_dali_ballast/project_dali_lib.h:299:9: warning: "DALI_QUERY_LIMIT_ERROR" redefined
  299 | #define DALI_QUERY_LIMIT_ERROR 148
note: this is the location of the previous definition
   33 | #define DALI_QUERY_LIMIT_ERROR 0x95
```

The same constant is defined in two headers, with **different values**:

| File | Line | Value |
|---|---|---|
| `project_dali_protocol.h` | 29 (bridge) / 33 (ballast) | `0x95` = **149** |
| `project_dali_lib.h` | 299 (both) | `148` = **0x94** |

These are not the same opcode — they differ by one, so one of them is addressing a *different*
DALI query command.

### Why this is more than a warning

The value that actually ends up in the binary depends on **include order**. In translation units
that pull in both headers, `project_dali_lib.h` wins and the constant is `148` (`0x94`). In a
translation unit that only includes `project_dali_protocol.h`, it is `0x95`. So the same named
constant can compile to two different opcodes in different `.cpp` files of the same firmware —
which is exactly the kind of thing that produces "works sometimes" DALI bus behaviour.

### What needs doing

1. Check IEC 62386-102 for the correct **QUERY LIMIT ERROR** opcode. (Working assumption, needs
   confirming against the spec: the query block runs `0x91` QUERY CONTROL GEAR PRESENT,
   `0x92` QUERY LAMP FAILURE, `0x93` QUERY LAMP POWER ON, `0x94` QUERY LIMIT ERROR,
   `0x95` QUERY RESET STATE — which would make `148`/`0x94` the correct one and `0x95` a
   mislabelled QUERY RESET STATE.)
2. Keep the definition in **exactly one** header and delete the other.
3. Do it in **both** `esp32_dali_bridge/` and `esp32_dali_ballast/` — each has its own copy of
   these two headers, and both carry the duplication.
4. While in there, check whether the surrounding query opcodes are shifted by one for the same
   reason.

Verify with:

```bash
pio run          # builds both envs; the warning must be gone and both must still build clean
```

> Unrelated to the PlatformIO migration — the duplication predates it.
