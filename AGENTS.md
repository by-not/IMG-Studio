# IMG STUDIO — PERMANENT AGENT RULES

## Project mission

IMG Studio is a professional Windows desktop application for managing GTA San Andreas IMG VER2 archives.

You are responsible for the complete engineering lifecycle: architecture, implementation, UI, testing, debugging, optimization, documentation and packaging.

The project must reach a usable, tested and distributable Windows x64 release.

## Technical baseline

- C++20.
- Windows 10/11 x64.
- Dear ImGui.
- GLFW.
- OpenGL 3.3.
- CMake.
- Ninja.
- MinGW-w64 cross-compilation from WSL.
- Git.
- Native Windows validation whenever possible.

Use maintained dependencies and pin or record dependency versions.

Do not add unnecessary frameworks.

## Architecture

Maintain separation between:

- Core binary archive engine.
- Application services.
- Filesystem and platform services.
- User interface.
- Tests.

The UI must never manipulate IMG binary structures directly.

Keep the archive engine independently testable.

## Format correctness

Support GTA San Andreas IMG VER2 only during the initial release.

Implement the 8-byte header and 32-byte directory entries using explicit little-endian parsing.

Preserve the 32-bit sector offset, both independent 16-bit size fields and the 24-byte filename.

Use 2048-byte sectors.

Do not invent undocumented format behavior.

Investigate ambiguous format details using independent references and real test archives.

Document verified behavior and remaining uncertainty.

## Safety

Never directly overwrite an original IMG during rebuild.

Use backups, temporary files, validation and safe replacement procedures.

Validate every offset, size and arithmetic operation.

Protect against integer overflow, malformed archives, truncated files, overlaps and invalid directory entries.

Do not touch user game archives without explicit authorization.

Use synthetic or disposable archives for destructive tests.

## Engineering quality

Use modern C++20, RAII and explicit ownership.

Avoid undefined behavior, unsafe binary casts, unchecked allocations and unnecessary global state.

Enable strict compiler warnings.

Keep code modular and maintainable.

Add regression tests whenever a bug is fixed.

Do not suppress warnings simply to obtain a successful build.

## Autonomous execution

You are authorized to continue through all project milestones without asking for approval at each stage.

You must:

1. Inspect the repository.
2. Plan the implementation.
3. Build the foundation.
4. Implement the complete archive engine.
5. Implement the complete user interface.
6. Integrate all supported operations.
7. Test and debug.
8. Package the Windows application.
9. Document installation and usage.
10. Perform a final quality audit.

Do not stop after creating a prototype, a window or a basic parser.

If a feature cannot be implemented safely, investigate alternatives and continue with the remaining work.

Do not wait for user approval for ordinary architectural or implementation decisions.

## Permissions and boundaries

You may create, edit and reorganize files inside this repository.

You may install project-local dependencies and configure build scripts.

Do not:
- Delete user data outside the repository.
- Overwrite original game archives.
- Modify unrelated projects.
- Reset Git history.
- Discard user changes.
- Perform irreversible system modifications.
- Claim that a test passed without executing it.

If a system-level operation requires authorization, explain the requirement and continue with independent tasks.

## Verification

Every completed feature must have appropriate tests.

Run builds and tests after meaningful changes.

Test the actual Windows x64 target, not only Linux binaries.

Where native Windows execution is unavailable, distinguish cross-compilation success from runtime verification.

Do not mark a milestone complete based only on source-code generation.

## Documentation

Maintain:

- README.md
- ARCHITECTURE.md
- IMG_FORMAT.md
- ROADMAP.md
- TESTING.md
- CHANGELOG.md

Keep documentation synchronized with the implementation.

## Reporting

Communicate in Spanish.

Use natural, direct and understandable language.

Do not use emojis.

Clearly distinguish implemented, compiled, tested and verified features.

Never fabricate test results or claim completion prematurely.

## Final objective

Deliver a complete Windows x64 IMG Studio application with a polished interface, reliable IMG VER2 manipulation, automatic backups, validation, extraction, import, replacement, deletion, renaming, rebuilding and documented installation.

Do not confuse an impressive prototype with a finished product.

Continue working autonomously until all achievable acceptance criteria are satisfied.
