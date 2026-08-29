# libOpus-gdnative
GDExtension integration of libOpus for Godot 4.

Looking for the Godot 3.x GDNative version? It lives on the [`godot3` branch](../../tree/godot3).

One key component Godot is missing for enabling VOIP features, is an audio codec that can be used at run-time to compress and decompress audio for transmission.

libOpus is the gold standard for open source real-time voice codecs.

This extension adds 2 nodes to Godot:
- OpusEncoderNode
  - `encode(raw_pcm)` : Accepts a `PackedByteArray` of 48kHz 16bit PCM Stereo audio samples. Returns a `PackedByteArray` of Opus data Packets interleaved with header data describing their individual lengths. (*This is a bit custom, and not the real™ way to pack these Opus data packets. But it is the simplest.*)
  - `bit_rate` : Property controlling the encoder bitrate, default 15000.
- OpusDecoderNode
  - `decode(opus_encoded)` : Accepts a `PackedByteArray` of Opus data Packets packed in our custom interleaved format. Returns raw 16bit stereo PCM data at 48kHz.

The current nodes and methods are meant to be used to encode a whole, continuous sample of audio. Then again decode that whole continuous audio sample. ***THIS DOES NOT YET ENABLE TRUE AUDIO STREAMING.***
Godot 4's `AudioEffectCapture` and `AudioStreamGenerator` provide the mic capture and playback primitives needed for true streaming, so that is now feasible future work.

## Installing

Download the addon zip from the [releases page](../../releases) and extract it into your project. The `OpusEncoderNode` and `OpusDecoderNode` types appear in the Create Node dialog.

Requires Godot 4.1 or newer.

## Building

Opus is built from source (a git submodule) and statically linked, so each platform ships a single library file.

```
git submodule update --init --recursive

# Build libopus for your platform, e.g. linux x86_64:
cmake -S thirdparty/opus -B thirdparty/opus/build/linux.x86_64 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF
cmake --build thirdparty/opus/build/linux.x86_64

# Build the extension
scons platform=linux target=template_debug
scons platform=linux target=template_release
```

CI builds all supported platforms (Linux, Windows, macOS universal, Android arm64) on every push, and pushing a `v*` tag attaches a ready-to-use addon zip to a GitHub Release.
