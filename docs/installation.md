# Installation

## Requirements

- C compiler (clang or gcc)
- Standard C library
- Math library (`-lm`)

## macOS / Linux

### Build

```bash
make
```

Or manually:

```bash
cc -O2 -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 main.c -lm -o sampleratechanger
```

### Install (optional)

```bash
sudo install -m 755 sampleratechanger /usr/local/bin/sampleratechanger
```

## Windows (MinGW-w64)

```bash
gcc -O2 -Wall -Wextra -std=c11 main.c -lm -o sampleratechanger.exe
```

You can run the executable directly or add its directory to your PATH.
