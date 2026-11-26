# App Store

Catalog-driven application store for Hashx86. The store fetches the latest
catalog release from a GitHub repository, renders the app list, and downloads
each application from the latest release of that application's own repository.

## How the catalog is discovered

1. The store requests:

   ```text
   https://api.github.com/repos/<APPSTORE_CATALOG_REPO>/releases/latest
   ```

2. It finds the asset named `APPSTORE_CATALOG_ASSET` (`catalog.json` by
   default), downloads it through the GitHub asset API with
   `Accept: application/octet-stream`, and verifies the asset size and
   SHA-256 digest supplied by GitHub.
3. It parses the catalog. The catalog release contains `catalog.json` and the
   JPEG artwork it references; it does not contain application binaries.
4. When an app with a `repo` entry is installed, the store requests that
   repository's latest release, finds the asset named by `asset`, and uses the
   GitHub-provided size and SHA-256 digest before downloading it. App releases
   are not contacted during the initial catalog fetch.

There is no pinned fallback. If the catalog release cannot be fetched or
verified, the store shows the error and lets the user press Refresh.

### Configuring the repository

`user_prog/AppStore/Makefile` defaults to:

```make
APPSTORE_CATALOG_REPO ?= Hashx86-OS/AppStore-Hx86
APPSTORE_CATALOG_ASSET ?= catalog.json
APPSTORE_OS_VERSION ?= 0.1.0
```

Change `APPSTORE_CATALOG_REPO` to the repository that hosts the catalog, or
override it:

```sh
make -C user_prog/AppStore APPSTORE_CATALOG_REPO=owner/repo
```

Until that repository and its latest release exist, the lookup returns 404 and
the store displays `Catalog: unavailable`. `APPSTORE_CATALOG_ASSET` is the
catalog file name inside the latest release. `APPSTORE_OS_VERSION` is the
version checked against each entry's `minOsVersion`.

### Publishing a catalog

Create a GitHub release in the catalog repository and attach:

- `catalog.json`
- every JPEG artwork file referenced by `iconAsset` or `bannerAsset`

Do not attach application binaries to the catalog release. Publish each
application's binary in its own repository's release; the catalog entry points
at that repository.

Artwork files must be named by their content hash:

```text
<sha256-of-file>.jpg
```

The catalog repository's release workflow rejects non-`.jpg` files and verifies
that each filename matches its content. This keeps names unique when a release
contains artwork for many apps. Only JPEG is supported; PNG and BMP are not.

Release assets must have GitHub-provided `sha256:` digests; assets without a
digest are rejected.

## Catalog schema

See [catalog.example.json](catalog.example.json) for a complete example.

Top-level fields:

| Field | Required | Meaning |
|---|---|---|
| `schema` | yes | Must be `1`. |
| `catalogVersion` | no | Human-readable catalog version. |
| `osVersion` | no | Version of Hashx86 the catalog was prepared for. |
| `apps` | yes | Array of app entries, up to 32. |

App fields:

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Safe directory name under `/Apps/`. Allowed: letters, digits, `.`, `-`, `_`. |
| `name` | yes | Display name. |
| `version` | yes | Catalog-advertised version. For repo entries, the resolved release tag is displayed after resolution. |
| `description` | no | Short description shown in the details pane. |
| `icon` | no | Built-in icon-font name, e.g. `fa-gamepad`, `fa-rocket`, `fa-file`. |
| `category` | no | Display category. |
| `license` | no | Display license identifier. |
| `source` | no | Project/source URL shown in the details pane. |
| `minOsVersion` | no | Minimum Hashx86 version, e.g. `0.1.0`. |
| `repo` | yes* | GitHub repository that supplies the binary. Accepts `owner/repo` or `https://github.com/owner/repo`. |
| `iconAsset` | no | Release asset name for a content-hashed `.jpg` icon. |
| `bannerAsset` | no | Release asset name for a content-hashed `.jpg` banner. |
| `asset` | yes* | Release asset name to download from `repo`'s latest release. |
| `url` | alternative | Direct HTTPS download URL. Requires `size` and `sha256`. |
| `size` | with `url` | Exact file size in bytes. |
| `sha256` | with `url` | 64 hexadecimal characters. |
| `binary` | no | Installed file name under `/Apps/<id>/`. Defaults to the asset name or the URL's last path segment. |

`repo` and `url` are mutually exclusive. `asset` is required when `repo` is
present. For compatibility, an entry with `asset` and no `repo` resolves that
asset from the catalog release itself; new catalogs should use `repo` so the
catalog release stays binary-free.

The store verifies the downloaded size and SHA-256 before moving the file into
place. Binary files are limited to 64 MiB and the catalog to 128 KiB. A release
may contain up to 256 assets in total, including `catalog.json`, which leaves
room for artwork from many apps.

### Icons

`icon` is a built-in icon-font glyph fallback. `iconAsset` and `bannerAsset`
name JPEG release assets; the App Store downloads them, decodes them in user
space with stb_image, and displays them through the ImageView GUI widget. The
kernel has no JPEG codec. Artwork assets are content-addressed, so the same
image may be shared by multiple catalog entries. Verified downloads are cached
under `/Hashx86/cache/appstore/<sha256>.jpg`, so reopening the store or
revisiting an app does not download the same artwork again.

## Installation and launch

An app is installed as:

```text
/Apps/<id>/<binary>
```

The store creates `/Apps` and `/Apps/<id>` as needed, resolves the app's latest
release if necessary, downloads to `/Apps/<id>/<binary>.part`, verifies it,
removes the previous binary and renames the verified temporary file into place.
Launch uses `syscall_execve("/Apps/<id>/<binary>", nullptr, nullptr)`; the
kernel sets the child's working directory to the binary's directory.

Application data files are intentionally not part of the store. Quake, for
example, owns its own shareware/PAK install flow and discovers it relative to
`Quake.bin`'s directory. A future catalog can add multi-file bundles without
making the store understand game-data formats.

## Runtime dependencies

- Hardware entropy (x86 RDRAND) is required for the kernel RNG and TLS.
  Networking stays disabled and TLS entropy requests fail when it is unavailable.
- Kernel networking (`Hnet`, lwIP, DHCP/DNS) and an RTL8139 or Intel
  82540EM/e1000 NIC.
- `/Hashx86/appstore-cacert.pem`, the small GitHub-focused trust store installed
  by `make hdd` and the installer. The full `/Hashx86/cacert.pem` bundle is kept
  for other apps; using it here makes TCG-emulated QEMU miss GitHub's TLS
  handshake timeout.
- `libmbedtls.a`, built by `user_prog/Makefile`.
- The ImageView GUI ABI (`IMAGEVIEW` / `SET_IMAGE`) in the kernel and
  `libhx86`.
- `third_party/stb_image/stb_image.h`, compiled JPEG-only into the App Store.

## Notes

- Catalog fetches and installs run synchronously on the GUI event thread. The
  libhx86 heap and mbedTLS session are not thread-safe, so a second user
  thread would race GUI allocations. The window may stop responding during a
  transfer while labels continue to update.
- The kernel socket syscalls cap a single send/receive at 4 KiB, while mbedTLS
  asks for larger TLS records. `HttpClient.cpp` splits every Hnet transfer to
  `kHnetChunk` (4096) and uses blocking `Hnet::Recv`; do not remove either
  without changing the kernel bounce-buffer limit.
- The catalog is trusted for app names, descriptions, icons and repository
  locations, but every binary is pinned by the exact size and SHA-256 from
  GitHub's release asset metadata. The catalog asset itself is verified against
  GitHub's release-asset digest.
- Only JPEG artwork is decoded. The catalog release workflow rejects PNG, BMP
  and other image files.
- Uninstall and dynamic start-menu registration are not implemented yet.
