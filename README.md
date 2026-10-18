# Caecilia Notch Island

Native Windows top-center overlay, specified by [PRD.md](PRD.md) and
[DESIGN.md](DESIGN.md). The interactive HTML file is a reference prototype,
not the application runtime.

## Current status

**M0 preparation only; no executable yet.** The repository contains the original
requirements/prototype, the verbatim Appendix A header, and an offline spec
consistency check. No later milestone has been started.

See [docs/m0-status.md](docs/m0-status.md) for implementation blockers and the
planned M0 verification checklist. A successful native build or visual/performance
validation has **not** been claimed.

## Offline spec verification

With a locally installed Node.js:

```powershell
node tools/verify-spec.cjs
```

This checks the header against DESIGN Appendix A and compares its numeric
geometry/layout/color/spring declarations and geometry builder with the HTML's
C++ exporter. It does not load the HTML in a browser or download its dependencies.
Node.js is a development-only tool, not an application dependency.

## Intended build environment

- Windows 10 build 19041+ / Windows 11
- MSVC (Visual Studio 2022), C++20, Windows SDK
- CMake 3.25+
- C++/WinRT provisioned locally through NuGet or vcpkg

Build configuration and application sources will be added after the unresolved
M0 specification decisions are confirmed. Dependency downloads, fetch, and push
are not performed by the preparation workflow.
