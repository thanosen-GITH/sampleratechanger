# Safety notes

## This tool edits files directly

There is no undo.

Always work on copies when processing important material.

## Header-only modification

No resampling is performed. Audio data is not changed.

If you set an incorrect sample rate, the audio will sound wrong. This is expected behavior.

## Suggested workflow

```bash
cp original.wav working_copy.wav
sampleratechanger working_copy.wav 48000
```
