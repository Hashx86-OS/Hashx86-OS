# jsmn (vendored)

Minimal JSON parser used by the kernel's persisted-settings loader.

- Upstream: https://github.com/zserge/jsmn
- Commit: `25647e692c7906b96ffd2b05ca54c097948e879c` (2021-10-14)
- License: MIT — see `LICENSE`. Copyright (c) 2010 Serge A. Zaitsev.

`jsmn.h` is vendored verbatim; no local modifications.

## Integration notes

- jsmn is **header-only**. The implementation lives behind `JSMN_HEADER`, so
  exactly one translation unit per binary should include it. The kernel's
  `core/settings.cpp` is the only one that does.
- It only depends on `<stddef.h>` (for `size_t`) and pulls in no libc
  functions, so it builds under the kernel's `-ffreestanding -nostdlib
  -fno-builtin` flags without a shim.
- The header is `extern "C"`-guarded, so it can be included from C++ directly.
- Tokens are supplied by the caller. `core/settings.cpp` sizes its token array
  for the expected schema and treats `JSMN_ERROR_NOMEM` as "file too complex
  for the current schema" rather than retrying.
