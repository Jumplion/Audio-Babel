---
description: "Use when writing or editing C++ unit tests, adding test cases, or fixing test failures in cpp/tests/."
applyTo: "cpp/tests/**"
---
# C++ Test Conventions

- Use **Catch2 v3** macros: `REQUIRE(expr)` for fatal checks, `CHECK(expr)` for non-fatal.
- Do NOT use the legacy `RUN_CHECK` / `OLD_CHECK` / `TestRunner` — those are being phased out.
- Group related assertions in `SECTION("name") { ... }` blocks inside `TEST_CASE`.
- Tag tests: `TEST_CASE("description", "[module]")` — e.g. `[base64]`, `[metadata]`, `[wav]`.
- For temp files use `TempFile` (RAII auto-delete) and `make_temp_path("name")` from `test_common.h`.
- Include `<catch2/catch_test_macros.h>` — Catch2 provides `main()` via `Catch2::Catch2WithMain`.
- New test files must be added to `CATCH2_TEST_SOURCES` in the root `CMakeLists.txt`.
- Run tests: `tools/powershell/run_tests.ps1 -TestMode unit` (or `tools/bash/run_tests.sh`).
