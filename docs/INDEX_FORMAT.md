# Audio Index Format Specification

**Version:** 3.0
**Project:** Speaker of Babel - Audio Indexing Library

---

## Overview

The Speaker of Babel audio indexing system maps a PCM audio payload to a single
large integer (a `boost::multiprecision::cpp_int`) and back. The mapping is a
**TRUE BIJECTION**: every possible index maps to exactly one PCM payload, and
every payload maps to exactly one index — no gaps, no collisions, and no
rejected inputs.

The index encodes the **PCM sample payload only**. It contains **no header**,
version byte, frame count, sample rate, bit depth, or channel count. A fixed
default header is applied **only** when writing a WAV file on decode:

- PCM, 44100 Hz, 16-bit, 1 channel (mono).

---

## The Payload Is Indexed by Whole Samples

The atomic unit of the payload is **one PCM sample**, not one byte. At the 16-bit
default the sample alphabet size is:

```
B = 1u << DEFAULT_BIT_DEPTH = 1u << 16 = 65536
```

Each 16-bit sample is interpreted as an **unsigned little-endian** value in the
range `0 .. 65535`. Signedness is irrelevant to the bijection. Indexing whole
samples (rather than raw bytes) guarantees that every index decodes to a whole
number of samples at the default bit depth.

The empty payload corresponds to the integer `0`, which corresponds to the empty
index string.

---

## Core Algorithm: Bijective Numeration (digit = value + 1)

The integer is built with **bijective numeration**, where each digit is the
sample value plus one. This is what allows trailing-zero (silence) samples to be
preserved: a zero sample contributes a real digit (`1`) instead of vanishing.

### Payload samples → integer (bijective base `B`)

```text
n = 0
for each sample v in order (v in 0 .. B-1):
    n = n * B + (v + 1)
```

### Integer → payload samples

```text
while n > 0:
    n  -= 1
    v   = n mod B      // emit v
    n   = n / B
// then reverse the emitted samples
// serialize each sample little-endian into the payload bytes
```

### Properties

- `Index::decode(Index::encode(x))` reproduces `x`'s samples exactly,
  including any leading **and** trailing zero (silence) samples and the exact
  sample count.
- A payload with `k` trailing zero samples and one with `k+1` produce
  **different** indices.

---

## Index String: Bijective Base-64

For storage and transmission the integer index is rendered as a string over the
URL-safe alphabet (64 characters, no padding):

```
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_
```

The string encoding is itself a **bijection** using the same bijective
numeration (digit = value + 1). It is **not** the bit-packing base-64 used for
arbitrary byte blobs.

### Integer → index string

```text
while n > 0:
    n -= 1
    emit ALPHA[n mod 64]
    n  = n / 64
// then reverse
```

### Index string → integer

```text
n = 0
for each char c in order:
    n = n * 64 + (alphaValue(c) + 1)
```

### Properties

- `b64ToIndex(indexToB64(n)) == n` for all `n >= 0`.
- `indexToB64(b64ToIndex(s)) == s` for every string `s` over the 64-symbol
  alphabet.
- The empty string maps to `0`; `0` maps to the empty string.
- Every alphabet-valid string of any length `>= 0` decodes to a valid payload.
  Nothing throws on alphabet-valid input.

---

## No Integrity Check (By Design)

There is intentionally **no checksum and no version byte**. Every alphabet-valid
index decodes to a valid payload. The accepted consequence is that a truncated or
mistyped index silently decodes to a different valid WAV. This is a deliberate
property of the bijection, not a bug.

The only rejection that can occur is a character outside the 64-symbol alphabet
when parsing an index string (`b64ToIndex` throws `std::invalid_argument`).

---

## Performance

The naive definition (`n = n*B + (v+1)` per sample) is O(L²) in bignum
arithmetic. The implementation never runs that loop. Instead it uses the exact
algebraic identity

```
n = V + S_L
```

where `V` is the payload read as a base-B number (the sample bytes themselves)
and `S_L = (Bᴸ − 1)/(B − 1)` is the base-B repunit (every digit `1`). Encoding is
then two linear `import_bits` passes plus one big-integer addition; decoding
recovers the sample count from `L = msb(n·(B−1)+1) / 16` (no bignum division),
subtracts the repunit, and reads off the digits. Both directions are **O(N)** in
the payload size. The bijective base-64 string conversion uses the identical
identity in base 64, so it is O(N) as well.

In practice a ~2.8 MB / 1.4 M-sample clip indexes and reconstructs in tens of
milliseconds end to end.

---

## Optional Index Scrambling

By default, similar payloads map to numerically nearby indices, so neighbouring
"library" positions hold near-identical audio, and a **short** index (the kind a
user is likely to type, and every browseable library position) decodes to only
one or two samples of near-silence. An **optional, reversible** scramble can be
enabled to scatter neighbours across the space — and to give short inputs a wide,
varied range of audio *lengths* — while keeping the mapping a perfect bijection.

The scramble is `lengthSpread(contentScramble(index))`, two keyed bijections:

### Content scramble (length-preserving)

`contentScramble()` runs a 4-round **Feistel network** *within the index's own
length-band* (it subtracts the band repunit `S_L`, permutes the in-band value,
and adds `S_L` back). A Feistel network is a bijection for any round function and
is undone by running the rounds in reverse, so no modular inverse is needed and
both directions are O(band width). This scatters neighbours: two payloads that
differ only in their last sample now have unrelated sample values throughout. It
does **not** change the band, so by itself it preserves decoded length.

### Length spread (diversifies short-index duration)

A `B = 65536` bijective-numeration index puts almost all small integers in tiny
bands, so without this step every short index would still decode to near-silence.
`lengthSpread()` is a keyed **involution** that swaps the block of short indices
`[1, 2^P)` (the *prefix*, `P = 256` bits by default — every browseable position
and any casually typed index) with a set of *target* slots scattered across `T`
distinct, log-spaced bands from a short minimum (default **100 ms**) up to a long
maximum (default **5 s**):

1. The short index `i` is first run through a Feistel over the whole prefix
   domain `[0, 2^P)` so that even *structured* inputs (e.g. browse indices, which
   step by a fixed stride) spread evenly rather than cycling through a couple of
   lengths.
2. The mixed value is split as `o·T + j`. The sub-index `j` (bit-reversed, so
   numerically adjacent indices land on *far-apart* durations) selects one of the
   `T` target lengths; `o` is run through a second Feistel over
   `[0, 2^(P − log2 T))` to choose the sample values within that band.

Because the map is a fixed pairing between two equal-size, disjoint sets, it is
its own inverse and trivially a bijection; the displaced *target* indices map
back down into the short block. Indices that are neither in the prefix nor a
target slot (already a non-trivial length) pass through unchanged.

The bounds and resolution are compile-time settings in
`cpp/include/IndexScramble.h`: `AUDIOBABEL_SCRAMBLE_MIN_MS` /
`AUDIOBABEL_SCRAMBLE_MAX_MS` (default `100` / `5000`),
`AUDIOBABEL_SCRAMBLE_LENGTH_COUNT_LOG2` (default `8` ⇒ 256 distinct lengths), and
`AUDIOBABEL_SCRAMBLE_PREFIX_BITS` (default `256`). Their consistency
(min < max, disjoint prefix/target bands, even Feistel domains) is enforced by
`static_assert` in `cpp/src/IndexScramble.cpp`.

### Invariants

- `0 → 0`, and every index still decodes (nothing is rejected) — the bijection
  holds. Both stages are bijections, so their composition is too.
- Short audio — even a 3-sample payload — remains fully reachable; encoding it
  still produces a valid (scattered, much larger) index that decodes back
  exactly. It is just astronomically unlikely to be produced from a casually
  chosen index.
- Browsing consecutive library positions now yields clips whose **durations** jump
  across the whole 100 ms .. 5 s range and whose **contents** are unrelated,
  instead of a stream of indistinguishable sub-millisecond near-silence.
- Cost scales with the chosen *target* band, so a short index does at most ~5 s
  worth of permutation work (tens of ms); most land on much shorter bands and
  cost proportionally less.

The seed is effectively part of the format: changing it (or toggling the
scramble) changes every index. The feature is controlled by the compile-time
flag `AUDIOBABEL_SCRAMBLE` (optionally `AUDIOBABEL_SCRAMBLE_SEED`), with a runtime
override available for testing, and is **enabled in the shipped WASM build**. See
`cpp/include/IndexScramble.h`.

## Reference Implementation

- **Payload ↔ integer:** `cpp/src/Index.cpp` —
  `Index::encode()` / `Index::decode()`
- **Integer ↔ index string:** `cpp/include/Utilities.h` —
  `indexToB64()` / `b64ToIndex()`
- **Constants:** `cpp/include/Constants.h` —
  `DEFAULT_BIT_DEPTH`, `DEFAULT_SAMPLE_RATE`, `SAMPLE_ALPHABET_SIZE`,
  `BASE64_ALPHABET_SIZE`
- **WAV parsing / writing:** `cpp/src/FileIO.cpp` —
  `FileIO::readWav()` / `FileIO::writeWav()`
- **Optional scrambling:** `cpp/include/IndexScramble.h` /
  `cpp/src/IndexScramble.cpp` — `scramble()` / `unscramble()`
