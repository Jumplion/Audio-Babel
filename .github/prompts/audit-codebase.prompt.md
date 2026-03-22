---
description: "Audit the codebase for correctness, redundancy, and simplification opportunities"
agent: "agent"
argument-hint: "Scope: e.g. 'C++ core', 'JS web app', 'WASM bindings', 'tests', or 'full'"
---
Audit the codebase for correctness, redundancy, and simplification opportunities.
Scope the audit to what the user specifies (e.g., C++ core, JS web app, WASM bindings, tests, or full).

## Process

1. **Read every file** in scope — do not rely on filenames or summaries alone. Open each file and read its contents.
2. Evaluate along the three axes below.
3. Cross-reference findings across files to catch duplication that spans module boundaries.

## Audit dimensions

For every file in scope, evaluate along **three axes**:

### 1. Correctness — is the code doing what it claims?
- Compare function names, doc comments, and variable names against actual behavior
- Flag silent data corruption (e.g., truncation/padding instead of throwing)
- Flag silent failures (e.g., file I/O returning without error on write failure)
- Flag placeholder/stub implementations still exposed as production API
- Check spec-vs-implementation mismatches (e.g., header says "throws" but function never throws)
- Verify thread-safety claims (static vs thread_local)
- Check for undefined behavior (sign-extension on bit shifts, integer overflow)

### 2. Redundancy — duplicated logic that should be consolidated
- Identical or near-identical code across files (e.g., WAV creation in two JS modules, base64 encoder reimplemented in test helpers when library already provides one)
- Repeated patterns that should be extracted (e.g., result-object construction pattern repeated across five page modules)
- Validation logic duplicated with different approaches (regex replace vs split/filter)
- Constants hardcoded in multiple places instead of centralized

### 3. Simplification — complex code that could be simpler at exact same functionality
- Overly complex algorithms where a closed-form solution exists (e.g., weighted distribution with redistribute+trim loops vs single remainder assignment)
- Manual bit-shifting where utility templates already exist (`write_le<>`, `push_be<>`)
- Wrapper indirection that adds no value (char*/string round-trips, thin forwarding methods)
- Dead code: unused constants, unused imports, commented-out code, deprecated helpers still present

## Known hotspots in this codebase

Reference these when auditing — they represent verified categories of findings:

| Area | Known pattern | Files |
|------|--------------|-------|
| Silent corruption | PCM truncation/padding warnings instead of exceptions | [AudioIndex.cpp](../../cpp/src/AudioIndex.cpp) |
| Silent failure | `writeIndexToFile` returns silently on open error | [FileWriters.cpp](../../cpp/src/FileWriters.cpp) |
| Stub exposed | `generateIndexFromSamples()` returns hardcoded placeholder | [wasm_bindings.cpp](../../cpp/wasm/wasm_bindings.cpp) |
| Unsafe JSON | Manual string concatenation without escaping | [wasm_bindings.cpp](../../cpp/wasm/wasm_bindings.cpp) |
| Duplicate WAV gen | `samplesToWav()` vs `createWavFile()` | [audioIndexWasm.js](../../docs/js/core/audioIndexWasm.js), [wavUtils.js](../../docs/js/utils/wavUtils.js) |
| Result boilerplate | Same result-object shape built 5×  | [fileUpload.js](../../docs/js/pages/fileUpload.js), [randomIndex.js](../../docs/js/pages/randomIndex.js), [recorder.js](../../docs/js/pages/recorder.js), [search.js](../../docs/js/pages/search.js), [browse.js](../../docs/js/pages/browse.js) |
| Hardcoded formats | 44100/16/1 scattered across JS modules | Multiple JS page files |
| Complex lens logic | Redistribute+trim loops in metadata splitting | [IndexMetadata.cpp](../../cpp/src/IndexMetadata.cpp) |
| Inconsistent errors | JSON `"error"` key vs `"error:msg"` string prefix in WASM | [wasm_bindings.cpp](../../cpp/wasm/wasm_bindings.cpp) |
| Spec mismatch | `reconstructIndexFromPosition` should validate but doesn't | [LibraryPosition.cpp](../../cpp/src/LibraryPosition.cpp) |
| Stack overflow risk | `btoa(String.fromCharCode(...largeArray))` for large WAV | [resultHandler.js](../../docs/js/core/resultHandler.js) |
| Unused code | `HEADER_BYTES_CONST`, unused import in `audioIndex.js` | [Constants.h](../../cpp/include/Constants.h), [audioIndex.js](../../docs/js/utils/audioIndex.js) |

## Output format

Produce a table per audit dimension, grouped by severity (Critical / High / Medium / Low).
Each row: **file, line(s), issue description, suggested fix** (concise).

End with a prioritized **action plan**: numbered list of fixes in recommended order, grouping related changes that should ship together.
