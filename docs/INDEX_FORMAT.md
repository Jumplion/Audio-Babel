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
user is likely to type) decodes to only a few samples of near-silence. An
**optional, reversible** scramble can be enabled to scatter neighbours across the
space — and to give short inputs a wide, interesting range of audio lengths —
while keeping the mapping a perfect bijection.

### Length tiers

Length-bands are grouped into contiguous **tiers**, each capped at a target
decoded duration at 44100 Hz. The default tier table is:

| Tier | Max duration | Max samples |
|------|--------------|-------------|
| 1    | 1s           | 44,100      |
| 2    | 5s           | 220,500     |
| 3    | 10s          | 441,000     |
| 4    | 15s          | 661,500     |

Tier *i* covers all sample counts from the previous tier's cap + 1 up to its own
cap (tier 1 starts at 1 sample). Payloads longer than 15s (tier 4) keep the
original length-preserving permutation.

The tier table is deliberately short: each Feistel round's cost scales with the
*tier's* byte width (not the input's), so a much larger top tier costs multiple
seconds per `scramble()`/`unscramble()` call — an earlier 240s/11-tier default
measured multiple seconds per call in the top few tiers.

The tier durations (and how many tiers there are) are a compile-time setting,
`AUDIOBABEL_SCRAMBLE_TIER_SECONDS` in `cpp/include/IndexScramble.h` — a
brace-initializer list of seconds, e.g. the default `{1, 5, 10, 15}`. Override
it at build time (`-DAUDIOBABEL_SCRAMBLE_TIER_SECONDS="{2, 30}"`) to use a
different number of tiers or different cutoffs; it must be non-empty and
strictly increasing, which is enforced by `static_assert` in
`cpp/src/IndexScramble.cpp`. There is no runtime setting for this — it is
baked into the binary, like the scramble seed.

### Keyed permutation across a tier

For an index whose band falls in a tier, the scramble subtracts the tier's low
end `S_lo` to get a value `y` in `[0, N)` (where `N = S_hi − S_lo` is the tier
width), then permutes `y` with a 4-round **Feistel network** built over the
smallest even-bit power-of-two domain `2^e ≥ N`, keyed by a seed and the tier
index. Because `N` need not be a power of two, the result is **cycle-walked**
(the permutation is re-applied until the value lands back in `[0, N)`); choosing
an even `e` with `2^e < 4N` keeps that to ~1 extra step on average. A Feistel
network is a bijection for any round function and is undone by running the rounds
in reverse, so both directions are **O(tier width)** — no modular inverse is
needed.

Because each extra sample multiplies a band's size by `B = 65536`, a tier's top
band holds `~(1 − 1/B)` of the whole tier. So a short index almost always lands
near the tier's **maximum** length: a typed 13-character index now yields ~1
second of audio rather than a handful of samples.

### Invariants

- The permutation never leaves its tier, `0 → 0`, and every index still decodes
  (nothing is rejected) — the bijection holds.
- It is **not** length-preserving inside a tier: the decoded length may change,
  but is bounded by the tier's cap. Every length still has exactly as many
  indices mapping to it as before (a tier maps onto itself), so short audio —
  even a 3-sample payload — remains fully reachable; it is just astronomically
  unlikely to be produced from a casually chosen index.
- Cost scales with the **tier width**, not the input size, so even a short
  tier-1 input does ~1 second's worth of permutation work (a few ms); a 15s
  tier-4 index does proportionally more (still sub-second).

The seed is effectively part of the format: changing it (or toggling the
scramble) changes every index. The feature is controlled by the compile-time
flag `AUDIOBABEL_SCRAMBLE` (optionally `AUDIOBABEL_SCRAMBLE_SEED`), with a runtime
override available for testing. See `cpp/include/IndexScramble.h`.

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
