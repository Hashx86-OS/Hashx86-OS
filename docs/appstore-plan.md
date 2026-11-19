# Hashx86 App Store + Quake Distribution Plan

Status: the first engine-install implementation exists in `user_prog/AppStore`
(GitHub API plus a pinned fallback, SHA-256 verification, GUI status and
launch). Application-specific data installation lives in the application:
`Quake.bin` owns the pinned `quake106.zip` download, ZIP+LHA extraction and
license installation. The remote catalog and manifest-driven packages remain
planned.

## 1. Recommendation

Build a catalog-driven app store rather than hardcoding download URLs into the
store. The store should:

1. Fetch a curated `catalog.json` over HTTPS from a project-controlled origin.
2. Resolve GitHub Releases through the catalog (version, asset name, size and
   SHA-256), with the GitHub Releases API as the source of release metadata.
3. Download **streams** to a staging directory, verify size and SHA-256, then
   move the staging tree into `Apps/<id>/` atomically.
4. Never treat arbitrary third-party URLs as trusted. Data downloads must be
   explicitly listed in the catalog with a license/attribution field, expected
   size and hash. Anything not legally redistributable is user-supplied only.

For Quake specifically:

- Engine: download `Quake.bin` from `Hashx86-OS/Quake-Hx86` releases.
- Retail/full data: `pak0.pak` is proprietary and must be supplied by the user
  from a legal Quake installation. Do **not** host or download retail data.
- Shareware data: the original id Software `quake106.zip` (v1.06) is the only
  approved source. Its 1996 license permits free redistribution of the
  complete, compressed archive with the license included, but not a repacked
  or extracted PAK. Download the intact archive, verify it, extract
  `id1/pak0.pak` locally, and install the license files with it.

The local `bin/pak0.pak` is **shareware**, not retail: 18,689,235 bytes,
SHA-256 `35a9c55e...`, 339 entries, episode 1 maps only. It is byte-identical
to the PAK inside `quake106.zip`. Do not distribute the PAK by itself; keep
the original archive and license together.

## 2. Current State

Available in Hashx86:

- RTL8139 and Intel 82540EM/e1000 + lwIP, DHCP, DNS, ARP/ICMP, UDP/TCP.
- lwIP NO_SYS=0 socket layer, per-process fd table, blocking socket calls.
- `Hnet` user wrapper: `Socket`, `Bind`, `Connect`, `Send`, `Recv`, `SendTo`,
  `RecvFrom`, `Shutdown`, `Close`, `Resolve`.
- mbedTLS 2.28.8 freestanding userspace port with allocator/time/gmtime hooks,
  CA bundle at `Hashx86/cacert.pem`, TLS 1.2 and hostname verification.
- RTC-backed `time()` syscall.
- jsmn available for JSON.

Added for this iteration:

- AppStore HTTP/1.1 client with redirects, chunked transfer decoding, retries,
  progress and SHA-256 verification.
- `mkdir`, `rename`, `unlink` and `rmdir` syscalls, wired through `FileSystem`
  and FatFs. Free-space reporting is still missing.
- A generic engine-install store UI with engine status and install/update
  buttons. It deliberately has no game-data knowledge.
- The Quake app owns its data flow: a missing-PAK window offers the original
  `quake106.zip` download, shows the gamers.org/license notice, verifies the
  pinned hashes, extracts with vendored miniz+lhasa and installs the PAK plus
  license files. The original compressed archive stays on disk.

Still missing:

- Catalog/manifest-driven packages and the signed remote catalog.
- Dynamic start-menu/app registry. Apps still launch from Explorer or
  hardcoded start-menu entries.

Quake-Hx86 facts (checked at planning time):

- Latest release: `v0.1.0`.
- Asset: `Quake.bin`, 1,625,648 bytes.
- GitHub API provides asset digest
  `sha256:e27db0b59450c58542697d2646433429cf5c741b65eef03a127d05b95b227b7a`.
- Asset download redirects:
  `github.com/.../releases/download/...` → `github.com/...` →
  `release-assets.githubusercontent.com/...` (signed, expiring URL).
  The downloader must follow cross-host redirects and re-establish TLS with
  the new hostname.
- Install layout expected by the port:
  ```
  /Apps/Quake/Quake.bin
  /Apps/Quake/id1/pak0.pak
  ```
  The engine resolves `id1/pak0.pak` relative to the current working directory;
  `sys_execve` sets the child cwd to the binary's directory.
- Engine code is GPL-2.0-or-later. The store must display the license and link
  to the matching source repository/release.
- Retail/full `pak0.pak` is proprietary and cannot be redistributed. The port
  README says the shareware `pak0.pak` works for episode one; the original
  license permits redistributing the complete shareware archive, not the PAK
  alone.

## 3. Product Design

### Store application

A normal user-space GUI app:

```
Hashx86/apps/AppStore.bin        (system app, listed in start menu)
Hashx86/apps/AppStore.manifest   (optional store settings/metadata)
```

The store window has:

- Left: app list / featured entries.
- Right: name, version, description, license, source, size and data status.
- Actions: Install, Update, Launch, Uninstall, Cancel.
- A progress label/bar with transferred bytes and percentage.
- Error reporting for network, hash, disk space and install failures.

The store itself is installed by the OS image (`make hdd`/installer), while
catalog data is fetched remotely so app entries can change without rebuilding
the OS.

### Install roots

Use `/Apps/<id>/` for downloaded user applications:

```
/Apps/Quake/Quake.bin
/Apps/Quake/id1/pak0.pak
/Apps/Quake/app.manifest.json
```

The FAT filesystem is case-insensitive, so the existing `/Apps` root and the
port's `/apps` convention refer to the same directory. Prefer the canonical
`/Apps` spelling in new code.

System apps remain under `/Hashx86/apps/`.

### Launching

- The store launches an installed app with
  `syscall_execve("/Apps/Quake/Quake.bin", ...)`, ideally via a small
  `Hx86/Hgui` or `Hx86/Hx86` helper.
- Explorer can launch the same path without any store-specific registry.
- Phase 2 can scan `/Apps/*/app.manifest.json` at desktop startup and add
  dynamic start-menu entries. Phase 1 does not need dynamic registration.

### Data modes

Every data artifact must declare one of:

- `downloadable`: legal to redistribute; URL + size + SHA-256 is in the
  catalog. The store shows the license and asks for consent before downloading.
- `user-supplied`: never downloaded. The store explains which file is needed,
  where to place it, and validates it after import.
- `optional`: not required to launch (for example, an HD texture pack).

Quake gets three logical pieces:

| Piece | Mode | Source |
|---|---|---|
| `Quake.bin` | downloadable | GitHub release asset |
| Shareware episode 1 | downloadable (archive only) | original `quake106.zip` from an approved mirror; verify hash; extract locally |
| Full/retail data | user-supplied | user's Steam/GOG/CD install; never downloaded |

The shareware terms have been reviewed (see "Verified shareware source" below).
Download only the intact, compressed `quake106.zip`; pin it by size and
SHA-256; verify the license members; extract locally. Do not create a
project-controlled repack of the PAK or of the installer. A second mirror is
acceptable only if it serves a byte-identical archive.

### Third-party source policy

A disclaimer such as "downloads come from this site; we do not host the data"
is good transparency, but it is **not** a redistribution license and does not
by itself make an unauthorized mirror safe or lawful. The catalog maintainer is
still choosing and presenting the source.

Rules:

- Only list sources that are official, or that the artifact's own license
  explicitly permits to redistribute it. Record the license and evidence.
- Do not add links to mirrors of retail/full `pak0.pak`. Those files are
  proprietary and a disclaimer does not cure that. Users must obtain them from
  a legal purchase or their own copy.
- For shareware data, verify the shareware license first. Record the evidence
  (license URL/date) in the catalog review notes.
- Pin every third-party artifact by HTTPS URL, exact size and SHA-256. Fail
  closed on redirect-to-HTTP, size mismatch, hash mismatch or unexpected
  content.
- Show a consent panel with the source host, license, size, hash and a short
  "not hosted by Hashx86; you are responsible for ensuring you may use this
  data" notice. Do not hide the source behind a generic Install button.
- Purchase links (Steam/GOG) are fine for pointing users at legitimate copies,
  but direct retail-data downloads must remain user-supplied.
- If community mirrors are ever supported, keep them out of the signed/official
  catalog and clearly mark them as user-configured and untrusted. They should
  not be presented as store-approved content.

### Verified shareware source (2026-11-02)

Primary source:
`https://www.gamers.org/pub/idgames/idstuff/quake/quake106.zip` (the idgames
archive, a community mirror, not an id-owned domain). A byte-identical copy is
at
`https://raw.githubusercontent.com/Jason2Brownlee/QuakeOfficialArchive/main/bin/quake106.zip`.
Both were downloaded and compared byte-for-byte.

| File | Size | SHA-256 |
|---|---:|---|
| `quake106.zip` | 9,094,045 | `ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239` |
| `resource.1` (LHA SFX inside the ZIP) | 9,086,574 | `c192c9c71bee41750dd7d14c99378766d61e077977b9d13d1a457b8d9eabe34a` |
| `id1/pak0.pak` (episode 1) | 18,689,235 | `35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af` |
| `slicnse.txt` | 10,036 | `070cdf6a6410adef8fb5f83a4e5ccdb9e2301d2e48d460bb3a67a0f5ba9d70a8` |
| `licinfo.txt` | 9,311 | `8d654834d085c088d603fb6879a874799e8b4cc6ae9317bbf826afd83bc72e31` |

The PAK directory has 339 entries. Its `maps/` entries are `start.bsp` and
`e1m1.bsp` through `e1m8.bsp`; there are no `e2`-`e4` map entries. Strings
matching `maps/e2m1.bsp` elsewhere in the PAK are data references, not map
files. The archive's `readv106.txt` confirms that the shareware version only
plays episode 1.

License: `slicnse.txt` is the "SHAREWARE VERSION: QUAKE - LIMITED USE SOFTWARE
LICENSE AGREEMENT", dated 1996-06-21. Key terms:

- Section 1 grants use and installation of one copy of the shareware version.
- Section 2 forbids commercial use, sale, rental, pay-per-play, and
  distribution for money or any other consideration.
- Section 3 forbids modifying, translating, reproducing, publicly displaying,
  or preparing derivative works.
- Section 4 forbids use of trademarks, logos, artwork, sounds, and music
  outside the game. A store listing should avoid screenshots, logos, and
  promotional art.
- Section 6 permits Providers to distribute free of charge, by electronic
  means only, in compressed format only, with the Agreement accompanying the
  Software at all times. End users may distribute the Software **as a whole**,
  free of charge only.
- Section 7 permits copies to be given to others with no consideration.

Conclusion: the license allows linking to and downloading the intact,
compressed `quake106.zip` with the license inside. It does **not** clearly
allow a standalone extracted `pak0.pak`, a repacked archive, or any paid
distribution. The store must download the original archive, verify its hash,
extract locally, and keep `slicnse.txt`/`licinfo.txt` installed with the data.
Install the PAK unmodified; do not patch, repack, or bundle it alone. No
screenshots, Quake logos, or Quake artwork in the listing or app icon without
separate permission.

Caveat: this is the 1996 license shipped with the file; the copyright owner is
now Bethesda/ZeniMax. No current official shareware download page was found.
This is evidence, not legal advice; if the project needs certainty or any
commercial use, obtain written permission or legal review. The conservative
fallback is user-supplied data only.

The v1 policy is therefore: store-installed GPL engine + shareware episode 1
from the verified archive, with consent and license files installed; plus a
user-supplied import path for full retail data.

If in-OS ZIP/LHA extraction is deferred, keep shareware as user-supplied: the
user downloads and extracts the archive outside the OS, then imports
`id1/pak0.pak`. Linking to the archive is still allowed; hosting the bare PAK
is not.

## 4. Catalog

### Hosting

Recommended: a project-controlled repository or release asset, for example:

```
Hashx86-OS/AppStore
  catalog.json
  catalog.sig          (optional phase 5)
  README.md
```

The store fetches the catalog from a pinned HTTPS origin. A remote catalog lets
new apps/releases appear without an OS update. For v1, the catalog can also be
embedded as a fallback so a network failure does not leave the store empty.

### Schema (v1)

```json
{
  "schema": 1,
  "generated": "2026-11-02T00:00:00Z",
  "apps": [
    {
      "id": "quake",
      "name": "Quake",
      "summary": "WinQuake port for Hashx86",
      "description": "Software-rendered Quake with keyboard and mouse input.",
      "license": "GPL-2.0-or-later",
      "source_url": "https://github.com/Hashx86-OS/Quake-Hx86",
      "icon": "builtin:gamepad",
      "install_root": "Apps/Quake",
      "launch": "Quake.bin",
      "artifacts": [
        {
          "id": "engine",
          "kind": "github-release",
          "repo": "Hashx86-OS/Quake-Hx86",
          "channel": "latest",
          "asset": "Quake.bin",
          "dest": "Quake.bin",
          "required": true
        }
      ],
      "data_options": [
        {
          "id": "shareware",
          "name": "Shareware episode 1 (original archive)",
          "mode": "downloadable",
          "kind": "archive",
          "url": "https://www.gamers.org/pub/idgames/idstuff/quake/quake106.zip",
          "source_host": "www.gamers.org",
          "size": 9094045,
          "sha256": "ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239",
          "archive_chain": ["zip", "lha-sfx"],
          "extract_members": [
            {"from": "id1/pak0.pak", "to": "id1/pak0.pak"},
            {"from": "slicnse.txt", "to": "licenses/SLICNSE.TXT"},
            {"from": "licinfo.txt", "to": "licenses/LICINFO.TXT"}
          ],
          "license": "Quake Shareware Limited Use License Agreement (1996-06-21)",
          "license_member": "slicnse.txt",
          "attribution": "Shareware episode 1 data, id Software"
        },
        {
          "id": "retail",
          "name": "Full game data (you must own it)",
          "mode": "user-supplied",
          "dest": "id1/pak0.pak",
          "instructions": "Copy pak0.pak from your legal Quake installation to /Apps/Quake/id1/pak0.pak."
        }
      ]
    }
  ]
}
```

Rules:

- `github-release` resolves the latest release via the GitHub API, selects an
  asset by name, and uses the API's size/digest. If the release changes, the
  digest changes with it and is still verified.
- `url` artifacts must be reviewed, approved sources and must include
  `sha256`, `size`, `license_url` and `attribution`. The downloader fails
  closed if the response does not match. Never use `url` for retail Quake
  data mirrors.
- `archive` artifacts must download the original compressed package, verify
  the archive hash, then extract locally. Never publish a repacked or
  extracted PAK as a catalog artifact.
- Catalog URLs are parsed with a strict HTTPS-only allowlist in v1. No
  runtime user-entered URLs.
- `dest` is relative and must not contain `..`, absolute paths or drive
  prefixes.
- Keep catalog values small; the store should not need a general JSON DOM.
  jsmn gives token-level parsing.

### Release resolution

Two supported modes:

1. **Pinned/catalog-managed (recommended for deterministic installs):** the
   catalog stores the full versioned asset URL and expected SHA-256. The store
   does not need GitHub API access for every install.
2. **`github-release` latest:** the store calls
   `https://api.github.com/repos/Hashx86-OS/Quake-Hx86/releases/latest` with:
   - `User-Agent: Hashx86-AppStore/0.1`
   - `Accept: application/vnd.github+json`
   - `X-GitHub-Api-Version: 2022-11-28`

   It reads `tag_name`, the matching asset's `browser_download_url`, `size` and
   `digest` (when present). The API is rate-limited (60 requests/hour without
   auth), so the store caches the result and should not poll on every window
   open. If the API fails, use the pinned catalog artifact.

The download URL is followed through redirects. The signed
`release-assets.githubusercontent.com` URL is temporary and must not be cached
or logged.

## 5. Downloader Requirements

This is the highest-risk technical piece. Build it as a reusable user-space
library, not inside the store UI.

### API shape

```
Hx86/Net/Url.h       parse https://host[:port]/path
Hx86/Net/Http.h      HTTP/1.1 request/response, redirects, chunked
Hx86/Net/Tls.h       mbedTLS client wrapper, CA bundle
Hx86/Net/Download.h  stream to a file, progress/cancel, SHA-256
```

Suggested download API:

```cpp
struct DownloadRequest {
    const char* url;
    const char* dest_path;
    const char* expected_sha256;  // optional
    uint32_t expected_size;       // optional, 0 = unknown
    uint32_t timeout_ms;
};

enum DownloadState { IDLE, CONNECTING, HANDShaking, DOWNLOADING, VERIFYING, DONE, FAILED };

bool HttpDownload(const DownloadRequest& request, DownloadProgress* progress);
```

### Behaviours

- HTTPS only.
- DNS through `Hnet::Resolve`.
- TCP through `Hnet` blocking sockets.
- mbedTLS client with hostname/SNI verification; CA bundle
  `Hashx86/cacert.pem`.
- HTTP/1.1 `GET`, `Host`, `User-Agent`, `Connection: close`, optional `Range`.
- Follow at least 5 redirects (301/302/303/307/308), including cross-host TLS.
- Parse `Content-Length`; decode chunked transfer encoding; support
  close-delimited bodies.
- Stream through a 4–16 KiB buffer into the staging file; never buffer whole
  releases in RAM.
- Incrementally compute SHA-256 while writing. Close and verify before rename.
- Report bytes/percentage and support cancellation.
- Timeouts for DNS/connect/TLS/read; distinguish EOF from error.
- Return sanitized errors (no signed redirect URLs in logs).

## 6. Installer

### Flow

1. Resolve catalog entry.
2. Determine selected data mode.
3. Check required free space (download size + manifest).
4. Create `/Apps/.staging/<id>-<version>/`.
5. Download each artifact to its `dest` under staging.
6. Verify size and SHA-256.
7. Write `app.manifest.json` into staging.
8. If installing an update, keep user data (`id1/pak0.pak`) from the existing
   install.
9. Atomically move the staging contents into `/Apps/<id>/` (directory rename).
10. On any failure, delete the staging tree and leave the old install intact.

### Manifest

```json
{
  "schema": 1,
  "id": "quake",
  "name": "Quake",
  "version": "0.1.0",
  "license": "GPL-2.0-or-later",
  "source": "https://github.com/Hashx86-OS/Quake-Hx86",
  "release": "v0.1.0",
  "installed_files": [
    {"path": "Quake.bin", "size": 1625648, "sha256": "..."}
  ],
  "data_files": [
    {"path": "id1/pak0.pak", "source": "user-supplied"}
  ]
}
```

The store UI reads installed manifests to decide Install vs Update vs Launch.

### Quake-specific install

Engine-only install:

```
/Apps/Quake/Quake.bin      from GitHub release
/Apps/Quake/app.manifest.json
```

Engine + data install:

```
/Apps/Quake/id1/pak0.pak   from an approved shareware source or user import
```

If data is missing, the store shows "Data required" and instructions. The Quake
binary itself already shows a GUI dialog with the expected path, but the store
should prevent a confusing launch when it can.

## 7. Filesystem/OS Work

Add proper syscalls and return statuses:

| Syscall | FatFs | Purpose |
|---|---|---|
| `sys_mkdir` | `f_mkdir` | create `Apps/Quake/id1` |
| `sys_rename` | `f_rename` | atomic staging → final install |
| `sys_unlink` | `f_unlink` | remove staging/old files |
| `sys_rmdir` | `f_unlink` | uninstall |
| `sys_statfs` | `f_getfree` | free-space check |
| `sys_fsync` | `f_sync` | flush manifest/engine files |

Implementation notes:

- Extend `FileSystem` with status-returning directory/delete/rename/free-space
  methods instead of the current `void` wrappers where possible.
- Validate user pointers with `CopyUserString`/`IsUserRange`.
- Reject `..`, absolute paths and paths outside the allowed install root.
- Keep a filesystem lock or single-writer enforcement; concurrent FAT access is
  already a known risk.
- Add a user-space `mkdir`, `rename`, `unlink`, `statfs` wrapper to libhx86.

## 8. Store UI and Threading

- Use the existing user-space GUI widgets (`Window`, `ListView`, `Button`,
  `Label`, `MessageBox`). Add a user-space `ProgressBar` wrapper if needed.
- Downloading must not freeze the GUI. Options:
  1. Download worker thread created through `syscall_clone`, with progress
     state consumed by the GUI event thread.
  2. A state-machine downloader driven from the app main loop, with
     non-blocking reads and periodic UI updates.
  Prototype option 1; it matches the kernel's thread/fd model and keeps HTTP
  timeouts isolated from rendering.
- The store should not run two installs concurrently in v1.
- Show license/source/size and a consent step before third-party data download.

## 9. Milestones

### M0 - Decisions and policy

- Decide whether to include shareware episode 1 in v1. The 1996 license and
  verified source are recorded in Section 3; the conservative alternative is
  user-supplied data only.
- Choose catalog repository/origin and whether v1 catalog is embedded.
- Confirm `/Apps/<id>` as the install root.
- Confirm GPL/source-notice presentation.

### M1 - Downloader spike and file primitives

- Implement `sys_mkdir`, `sys_rename`, `sys_unlink`, `sys_statfs` and libhx86
  wrappers.
- Implement URL parser, HTTP redirects/chunked, TLS client wrapper and
  streaming SHA-256 download.
- Implement ZIP deflate + LHA/LH5 extraction for the shareware archive. Port a
  small ISC-licensed decoder such as lhasa, or implement only the needed
  paths; run it as in-process user code, not an external binary.
- Acceptance: inside QEMU, download `Quake.bin` from the real GitHub release,
  verify the SHA-256 against the GitHub API digest, and confirm the file is
  byte-identical. Also download `quake106.zip`, verify
  `ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239`,
  extract `id1/pak0.pak`, verify
  `35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af`, and
  confirm the PAK directory contains episode 1 maps only. Test a local HTTPS
  server with 302 and chunked responses.

### M2 - Store core

- Catalog parser (jsmn), GitHub release resolution, installer and manifest.
- Test: install Quake engine-only to `/Apps/Quake`, detect missing data,
  reinstall/update while preserving `id1/`, fail cleanly on bad hash/404/TLS
  failure/disk full.

### M3 - Store GUI

- Window, app list, details, install/update/launch buttons and progress.
- Add `AppStore.bin` to the OS image and start menu.
- Launch installed Quake via `syscall_execve` or Explorer.
- Acceptance: user-visible install of the engine from the start menu.

### M4 - Quake data

- Implement the approved data modes:
  - shareware: download the intact `quake106.zip` from the pinned mirror,
    verify the archive hash, extract `id1/pak0.pak` and the license files
    locally, and install them. Never host or repack the PAK.
  - retail: user-supplied `pak0.pak` import/validation.
- Validate PAK header (`PACK`), expected size and known hashes. The shareware
  PAK is 18,689,235 bytes with SHA-256
  `35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af`;
  recognize retail data by its size/entries and require the user to supply it.
- Acceptance: install engine + shareware episode 1 and launch Quake to the main
  menu; also test a user-supplied retail import.

### M5 - Hardening

- Catalog signing, package signature verification, mirror fallback, resume,
  bandwidth/disk quotas, uninstall, dynamic start-menu/app registry, update
  notifications.
- Thread-safe filesystem access and crash-safe install state.

## 10. Testing Strategy

Host tests (no OS/network):

- URL parser, redirect resolution, chunked decoder.
- Catalog schema validation and path traversal rejection.
- Installer transparent/atomic rename logic with a mock filesystem.
- SHA-256 verification and failure cleanup.

QEMU integration:

- Local HTTPS test server serving a fake catalog, fake release and redirects,
  including chunked bodies and wrong hashes.
- Real GitHub API + release download spike (network required).
- Quake engine install and launch using the verified shareware archive; keep
  the archive outside the repo and remove it after the test.
- ZIP/LHA extraction: both archive layers, member hashes, path traversal,
  truncated member and bad CRC.
- Failure injection: 404, 500, redirect loop, TLS/CA failure, truncated body,
  disk full, read-only filesystem, bad PAK.

Do not commit shareware or retail Quake data to test fixtures. Use small
synthetic archives for unit tests; use the real archive only as a local
integration input.

## 11. Risks

| Risk | Mitigation |
|---|---|
| Retail Quake data licensing | Never host/serve or link unauthorized mirrors; user-supplied only |
| Shareware license conditions | Download only intact, compressed `quake106.zip`; include the license; no repack or sale |
| Shareware public display/trademark | Text-only listing; no screenshots, logos, or art without permission |
| Disclaimer mistaken for permission | Accept only the verified archive/hash; record license evidence |
| ZIP+LHA extraction complexity | Port a small ISC decoder; test both layers with synthetic and real archives |
| GitHub redirects/chunking | Build/test HTTP client before store UI |
| API rate limits | Catalog pinning; cache; fallback URL |
| Large file size (18 MB PAK, 1.6 MB engine) | Stream to disk, incremental hash, free-space check |
| FAT concurrency/corruption | Single install writer; filesystem lock; atomic staging |
| No mkdir/rename today | Add syscalls and FS status APIs first |
| TLS 1.2 only / weak entropy | Accept for now; document; harden RNG/TLS1.3 later |
| Store UI freezes on network | Worker thread/state machine |
| Arbitrary third-party URL risks | Curated catalog, allowlist, hashes, consent |
| GPL compliance | Show license and matching source URL for engine |

## 12. Open Decisions

1. Include shareware episode 1 in v1 using the verified `quake106.zip` source
   and hash, or ship user-supplied data only? (Recommended: include, with
   consent, archive-only download, and installed license files.)
2. Is `www.gamers.org` acceptable as the primary mirror, with the GitHub
   archive copy as a hash-pinned fallback? Any mirror must serve a
   byte-identical archive.
3. Catalog hosting: `Hashx86-OS/AppStore` repo, a release asset, or a pinned
   file in the OS repository?
4. Catalog signing in v1 or M5?
5. Store UI scope: engine install only, or engine + data in the first release?
6. Use GitHub API for latest resolution, or pin versioned asset URLs in the
   catalog (recommended: pin, with API as an update resolver)?
7. Dynamic start-menu registration: required for Quake launch, or is Explorer
   / store launch enough initially?
8. ZIP/LHA implementation: port lhasa (ISC) or write a minimal in-tree
   deflate + LH5 decoder? (Recommended: minimal in-tree decoder with host
   tests; no external binary.)

## 13. Suggested First Step

Before building UI, implement the downloader spike and filesystem primitives.
The acceptance test is concrete:

```
download https://github.com/Hashx86-OS/Quake-Hx86/releases/latest/download/Quake.bin
follow redirects
verify TLS + SHA-256 from the GitHub API
save to /Apps/Quake/Quake.bin

download https://www.gamers.org/pub/idgames/idstuff/quake/quake106.zip
verify SHA-256 ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239
extract resource.1 (LHA SFX), then id1/pak0.pak
verify SHA-256 35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af
install pak0.pak and slicnse.txt under /Apps/Quake
```

If that works reliably inside QEMU, the store is mostly catalog logic, staging,
progress UI and packaging policy.
