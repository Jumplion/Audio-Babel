# Performance benchmark JSON schema

This is the schema produced by `performance_benchmarks` (see
`cpp/tests/test_performance.cpp`, `BenchmarkRunner::generateJsonReport`) and
consumed by `tools/node/compare-benchmarks.mjs` and
`tools/node/update-baseline.mjs`. It is the single source of truth for the
shape of both `build/performance_results.json` (a fresh run) and
`cpp/perf/baseline.json` (the committed comparison target) — the two files
share this schema, with the baseline adding one extra top-level field,
`tolerancePercent`.

## Top-level fields

| Field          | Type   | Description                                                            |
|----------------|--------|--------------------------------------------------------------------------|
| `schemaVersion`| number | Schema version. Currently `1`. Bump on breaking changes.                |
| `generatedAt`  | string | ISO-8601 UTC timestamp of when the run was produced.                    |
| `gitCommit`    | string | Short git commit hash the binary was built from, or `"unknown"`.        |
| `platform`     | object | See [Platform object](#platform-object).                                |
| `benchmarks`   | array  | List of [Benchmark entries](#benchmark-entry).                          |
| `tolerancePercent` | number | **Baseline only.** Allowed percent regression before a benchmark is flagged. Absent in fresh run output. |

## Platform object

| Field       | Type    | Description                                              |
|-------------|---------|------------------------------------------------------------|
| `os`        | string  | `"Linux"`, `"macOS"`, `"Windows"`, or `"Unknown"`.        |
| `compiler`  | string  | e.g. `"GCC 13.2.0"`, `"Clang 17.0.0"`, `"MSVC 1939"`.     |
| `buildType` | string  | CMake build type (`"Release"`, `"Debug"`, `"Unspecified"`). |
| `archBits`  | number  | `32` or `64`, from `sizeof(void*) * 8`.                   |

## Benchmark entry

| Field           | Type   | Description                                                          |
|-----------------|--------|------------------------------------------------------------------------|
| `name`          | string | Human-readable benchmark name, e.g. `"Index Generation (1s audio)"`. Matching key between a run and the baseline. |
| `category`      | string | Grouping used for table display, e.g. `"Index Operations"`.          |
| `medianMs`      | number | Median wall-clock time per iteration, in milliseconds.                |
| `minMs`         | number | Minimum observed time across iterations.                              |
| `maxMs`         | number | Maximum observed time across iterations.                              |
| `stddevMs`      | number | Standard deviation across iterations.                                 |
| `iterations`    | number | Number of timed iterations.                                           |
| `throughput`    | number | Derived throughput value (meaning depends on `throughputUnit`).       |
| `throughputUnit`| string | Unit for `throughput`, e.g. `"samples/sec"`, `"chars/sec"`, `"MB/sec"`, `"scrambles/sec"`. |

## Matching rule for comparison

`compare-benchmarks.mjs` matches a current run's benchmarks against the
baseline by `name` (not array position):

- A benchmark present in both: compared (see regression rule below).
- A benchmark in the current run but not the baseline: reported informationally as new, not a failure.
- A benchmark in the baseline but not the current run: reported informationally as missing, not a failure.

## Regression rule

A benchmark regresses when:

```
currentMedianMs > baselineMedianMs * (1 + tolerancePercent / 100)
```

`tolerancePercent` defaults to the baseline's own `tolerancePercent` field
(20 by default) and can be overridden with `compare-benchmarks.mjs
--tolerance <percent>`.
