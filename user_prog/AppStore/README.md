# App Store

First implementation of the Hashx86 application store. It installs application
binaries and launches them.

## What it does

- Resolves the latest `Quake-Hx86` release through the GitHub Releases API.
- Falls back to a pinned `v0.1.0` catalog entry when the API is unavailable.
- Downloads `Quake.bin` over HTTPS with mbedTLS, verifies the release size and
  SHA-256 digest, and installs it as `/Apps/Quake/Quake.bin`.
- Detects an existing engine and offers Update or Launch.

The store does not download or extract game data. Data packages, mirrors and
archive formats vary by application, so each application owns its data install
flow. Quake.bin detects a missing `id1/pak0.pak`, offers the shareware download
and extracts it locally.

## Runtime dependencies

- Kernel networking (`Hnet`, lwIP, DHCP/DNS) and an RTL8139 or Intel
  82540EM/e1000 NIC.
- `/Hashx86/cacert.pem`, installed by `make hdd` and the installer.
- `libmbedtls.a`, built by `user_prog/Makefile`.

## Notes

- The install runs synchronously on the GUI event thread. The libhx86 heap and
  mbedTLS session are not thread-safe, so a second user thread would race GUI
  allocations. The window may stop responding during a download while the
  progress label continues to update.
- The kernel socket syscalls cap a single send/receive at 4 KiB, while mbedTLS
  asks for larger TLS records. `HttpClient.cpp` splits every Hnet transfer to
  `kHnetChunk` (4096) and uses blocking `Hnet::Recv`; do not remove either
  without changing the kernel bounce-buffer limit.
- The pinned catalog fallback is temporary until the signed remote catalog and
  package manifest work from `docs/appstore-plan.md` lands.
- Uninstall, dynamic start-menu registration and a remote catalog are not
  implemented yet.
