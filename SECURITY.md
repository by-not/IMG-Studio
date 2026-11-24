# Security Policy

## Supported Versions

Only the latest release branch/tag receives security fixes and stability updates:

| Version | Supported |
| --- | --- |
| 2.1.x (`develop` / upcoming release) | Yes |
| 2.0.0 (`v2.0.0`) | Security fixes backported where critical |
| 1.0.0 (`v1.0.0`) | Archived legacy release |

## Security Model & Parsing Boundaries

IMG Studio parses untrusted binary `.img`, `.dff`, and `.txd` files that originate from user archives or community mods. Accordingly:

- **Archive Integrity**: IMG VER2 edits are staged in memory and written atomically to temporary files, verified with SHA-256 checksums, and backed up before replacement.
- **Read-Only Inspection**: `DFF Studio`, `TXD Studio`, and `IMG Compare` operate strictly read-only against source `.img` archives.
- **Bounds & Overflow Checks**: Parsers validate chunk sizes, sector bounds, filename lengths (23 ASCII chars + null terminator), path traversal safety on export (`build_safe_export_path`), and integer arithmetic before allocating buffers.
- **Sandboxed Locale Loading**: Locale JSON files in `assets/locales/` use strict UTF-8 parsing and fall back to English on corrupt or partial definitions.

## Reporting a Vulnerability

If you discover a crash, out-of-bounds read/write, path traversal issue, or archive corruption risk when parsing malformed `.img`, `.dff`, `.txd`, or locale files:

1. **Do not** open a public GitHub issue with exploit payloads before maintainers review the report.
2. Report privately through GitHub Security Advisories (**Security -> Advisories -> Report a vulnerability**) once the repository is published, or contact the repository owner directly.
3. Include:
   - Affected IMG Studio version and Windows/Linux build target
   - Minimal synthetic reproduction steps or sanitizer stack trace (ASan/UBSan output preferred)
   - Do **not** attach copyrighted commercial game archives (`.img`/`.dff`/`.txd` files extracted from commercial installations).

We aim to acknowledge reports promptly, investigate with AddressSanitizer/UBSan test cases, and release a patched build along with a credit (unless you prefer to remain anonymous).
