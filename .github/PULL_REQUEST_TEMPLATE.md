## Summary

<!-- Briefly describe what this PR changes and why. -->

## Related Issue(s)

<!-- Fixes #... -->

## Type of Change

- [ ] Bug fix
- [ ] New feature / enhancement
- [ ] Localization / translation update
- [ ] Documentation / GitHub template update
- [ ] Build / test / packaging update

## Verification Checklist

- [ ] `python3 scripts/validate_locales.py` passes with zero errors
- [ ] Linux Debug tests pass under ASan/UBSan (`ctest --preset linux-debug-tests --output-on-failure`)
- [ ] Windows x64 Release and Debug builds compile cleanly with zero warnings
- [ ] Updated or added automated tests for changed logic
- [ ] Confirmed no copyrighted game assets (`.img`, `.dff`, `.txd`), secrets, or private local paths are included
- [ ] Followed C++20 layer boundaries (`core/`, `platform/`, `app/`, `ui/`)
