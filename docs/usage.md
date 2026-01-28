# Usage

## Syntax

```bash
sampleratechanger [-v] <file1> <file2> ... <fileN> <target_sample_rate>
```

- `-v` enables verbose output
- Up to 2048 input files are supported
- `<target_sample_rate>` is given in Hz (e.g. 44100, 48000)

## Examples

Change a single file to 48 kHz:

```bash
sampleratechanger file.wav 48000
```

Batch process multiple files:

```bash
sampleratechanger -v take1.wav take2.wav take3.aiff 44100
```

## In-place modification

Files are modified **in place**. No output files are created.

Make backups if the originals matter.
