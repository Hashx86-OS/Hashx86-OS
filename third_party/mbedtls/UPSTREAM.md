# Mbed TLS

- Upstream: https://github.com/Mbed-TLS/mbedtls
- Version: **v2.28.8**
- License: dual Apache-2.0 OR GPL-2.0-or-later (see `LICENSE`); Hashx86 uses the
  Apache-2.0 option.
- Imported: 2026-11-02

Only `include/`, `library/`, `LICENSE`, `README.md` and `ChangeLog` are
vendored. All Hashx86 configuration and platform hooks live in
`user_prog/libmbedtls/`; nothing inside `include/` or `library/` is patched.
