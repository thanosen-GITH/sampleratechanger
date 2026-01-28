# sampleratechanger

A small command-line utility to **fix or override the sample rate stored in audio file headers**.

Supported containers:
- **WAV (RIFF)**  
- **Wave64 (.w64)** (GUID-based chunks, 64-bit sizes)  
- **AIFF / AIFC (.aif / .aiff)** (sample rate stored as IEEE 80-bit extended float)

> ⚠️ This tool modifies **header metadata only**.  
> It does **not** resample audio data.  
> Changing the header sample rate will change playback speed/pitch accordingly.

---

## Why?

Sometimes audio files contain an incorrect or misleading sample rate in the header:
- broken exports
- faulty conversions
- legacy tools
- DAW/import issues

Many players and DAWs trust the header blindly.  
This tool lets you **batch-correct the header sample rate** safely and consistently.

---

## What gets updated?

- **WAV / Wave64**
  - `sampleRate`
  - dependent `byteRate` field in the `fmt` chunk
- **AIFF / AIFC**
  - `COMM.sampleRate` written as a proper **10-byte IEEE 754 extended (80-bit) float**

Large files (>4 GB) are supported.

---

## Build

### macOS / Linux
```sh
cc -O2 -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 main.c -lm -o sampleratechanger
```
even easier: make

### Windows (MinGW)
```sh
gcc -O2 -Wall -Wextra -std=c11 main.c -lm -o sampleratechanger.exe
```

---

## Usage
```sh
./sampleratechanger -v <wavefile1> <wavefile2> ... <wavefileN> <target_sample_rate>
```
-v:
Optional verbose output

wavefile1 ... wavefileN:
One or more audio files (up to 2048 files per invocation)

target_sample_rate:
Desired sample rate in Hz (e.g. 44100, 48000, 96000, etc.)

### Examples
```sh
# Change a single file to 48000 Hz
./sampleratechanger file.wav 48000

# Batch process multiple files verbosely
./sampleratechanger -v take1.wav take2.w64 take3.aiff 44100
```

---

## Notes / Limitations
This is not a resampler — audio data is untouched.

Always work on copies or backups when batch-processing important material.

For compressed WAV formats, behavior is best-effort and not extensively tested

---
