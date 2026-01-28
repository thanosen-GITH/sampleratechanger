# Supported formats

## WAV (RIFF)

- Updates `fmt` chunk sample rate
- Recalculates and updates `byteRate`
- Supports large files (> 4 GB)

## Wave64 (.w64)

- GUID-based chunk structure
- 64-bit chunk sizes
- Sample rate and byte rate updated correctly

## AIFF / AIFC

- Updates `COMM.sampleRate`
- Stored as IEEE 754 80-bit extended float (10 bytes)

## Notes on compressed WAV

Compressed WAV variants are handled on a best-effort basis and are not extensively tested.
