import sys
import numpy as np
import soundfile as sf
import tqdm
from pathlib import Path

# Fixed WAV properties
SAMPLE_RATE:int = 48000
BIT_DEPTH:int = 32
CHANNELS:int = 1
DURATION:int = 8
N_SAMPLES:int = SAMPLE_RATE * DURATION

# Base for encoding samples
BASE:int = 1 << BIT_DEPTH
OFFSET:int = BASE // 2

def ensure_int_str_digits(n, safety_margin=2):
    """Ensure sys.int_max_str_digits is large enough to stringify n.
    Uses bit_length to estimate decimal digits without converting to str().
    """
    
    get_limit = getattr(sys, "get_int_max_str_digits", None)
    set_limit = getattr(sys, "set_int_max_str_digits", None)
    if get_limit is None or set_limit is None:
        return

    current_limit = sys.get_int_max_str_digits()
    # estimate decimal digits safely from bit_length (log10(2) ≈ 0.30103)
    est_digits = int(n.bit_length() * 0.30102999566398114) + 1
    needed = est_digits * safety_margin
    if needed > current_limit:
        # Increase to at least needed, or double current to avoid repeated small bumps
        new_limit = max(int(needed), current_limit * 2)
        sys.set_int_max_str_digits(new_limit)

# -----------------------------------------------------
# WAV -> Index
# -----------------------------------------------------

def wav_to_index(filename:str) -> int:
    """Convert WAV file to fixed-parameter index representation."""
    samples, sr = sf.read(filename, dtype="int32", always_2d=False)

    # Force mono (average if multiple channels)
    if samples.ndim > 1:
        samples = samples.mean(axis=1).astype(np.int32)

    # Resample check
    if sr != SAMPLE_RATE:
        raise ValueError(f"Expected {SAMPLE_RATE}Hz, got {sr}. Resample externally first.")

    # Pad or truncate to exactly N_SAMPLES
    if len(samples) < N_SAMPLES:
        pad = np.zeros(N_SAMPLES - len(samples), dtype=np.int32)
        samples = np.concatenate([samples, pad])
    elif len(samples) > N_SAMPLES:
        samples = samples[:N_SAMPLES]

    # Encode into big integer
    index = 0
    for s in tqdm.tqdm(samples, desc="Encoding"):
        digit = int(s) + OFFSET
        index = index * BASE + digit

    return index

# -----------------------------------------------------
# Index -> WAV
# -----------------------------------------------------

def index_to_wav(index_str: str, filename: str) -> None:
    """Convert index back to WAV file with fixed parameters."""
    index = int(index_str)
    samples = []

    for _ in tqdm.tqdm(range(N_SAMPLES), desc="Decoding"):
        index, rem = divmod(index, BASE)
        samples.append(rem - OFFSET)
    samples.reverse()

    samples = np.array(samples, dtype=np.int32)
    sf.write(filename, samples, SAMPLE_RATE, subtype="PCM_32")
    print(f"Generated {filename} with fixed format: {SAMPLE_RATE}Hz, {BIT_DEPTH}bit, {CHANNELS}ch, {DURATION}s")


# -----------------------------------------------------
# Binary index IO helpers
# -----------------------------------------------------

def write_index_binary(path: str, index: int) -> None:
    """Write index as: 4-byte little-endian length + big-endian payload bytes."""
    byte_len = (index.bit_length() + 7) // 8
    if byte_len == 0:
        byte_len = 1
    with open(path, "wb") as f:
        f.write(byte_len.to_bytes(4, byteorder="little", signed=False))
        f.write(index.to_bytes(byte_len, byteorder="big", signed=False))


def read_index_binary(path: str) -> int:
    """Read index written by write_index_binary and return as int."""
    with open(path, "rb") as f:
        len_bytes = f.read(4)
        if len(len_bytes) < 4:
            raise ValueError("Index file too short or corrupted")
        payload_len = int.from_bytes(len_bytes, byteorder="little", signed=False)
        payload = f.read(payload_len)
        if len(payload) < payload_len:
            raise ValueError("Index payload truncated")
        return int.from_bytes(payload, byteorder="big", signed=False)

# -----------------------------------------------------
# Pipeline
# -----------------------------------------------------

def process_wav(input_wav: str, txt_file: str, output_wav: str) -> None:
    # WAV -> Index
    index = wav_to_index(input_wav)

    ensure_int_str_digits(index)
    # Save index using helper (binary format)
    write_index_binary(txt_file, index)
    byte_len = (index.bit_length() + 7) // 8
    if byte_len == 0:
        byte_len = 1
    print(f"Index written to {txt_file} (binary, {byte_len} bytes)")

    # Read index back using helper
    index_read = read_index_binary(txt_file)

    # Index -> WAV
    index_to_wav(index_read, output_wav)

# -----------------------------------------------------
# Example usage
# -----------------------------------------------------

if __name__ == "__main__":
    # Use repository root input/ and output/ directories (one level above src)
    repo_dir = Path(__file__).resolve().parent.parent
    input_dir = repo_dir / "input"
    output_dir = repo_dir / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    input_wav = input_dir / "input.wav"
    index_file = output_dir / "index.bin"
    output_wav = output_dir / "recreated.wav"

    if not input_wav.exists():
        print(f"Input file not found: {input_wav}\nPlease put your source WAV at this path and re-run.")
    else:
        process_wav(str(input_wav), str(index_file), str(output_wav))
