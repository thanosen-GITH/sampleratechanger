# Overview

`sampleratechanger` is a command-line utility that changes the **sample rate value stored in an audio file’s header**.

It does **not** resample audio or modify sample data.

## What this means

Changing the header sample rate without resampling will cause the audio to:

- Play faster or slower
- Change pitch accordingly

This is sometimes exactly what you want, but it is not a substitute for resampling.

## Typical use cases

- Fixing files recorded at the wrong sample rate
- Adjusting playback speed or pitch intentionally
- Preparing files for software that expects a specific rate

## What this tool is not

- Not a resampler
- Not a format converter
- Not a batch audio processor beyond header edits
