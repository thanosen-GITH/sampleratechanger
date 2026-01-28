# Troubleshooting

## Command not found

Make sure:

- You are in the directory containing the binary, or
- The binary is installed somewhere on your PATH

On macOS/Linux, try:

```bash
./sampleratechanger ...
```

## Permission denied

```bash
chmod +x sampleratechanger
```

## Audio plays at wrong speed or pitch

This means the header sample rate does not match the actual audio data.

This is expected if you changed the rate intentionally.
