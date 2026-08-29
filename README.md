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

`encode`/`decode` are the whole-clip path: encode a complete, continuous sample of audio, then decode it back in one call.

## Streaming API

For true live streaming (push-to-talk VOIP), both nodes also expose a per-frame API designed to sit between Godot 4's `AudioEffectCapture` and `AudioStreamGenerator`. It requires the project mix rate to be 48kHz (`audio/driver/mix_rate=48000`), since Opus does not accept 44.1kHz and no resampling is performed.

- OpusEncoderNode
  - `push_audio(frames)` : Accepts a `PackedVector2Array` of stereo float frames (as returned by `AudioEffectCapture.get_buffer()`), any length. Frames accumulate internally; at most 1 second is buffered before the oldest audio is dropped.
  - `has_packet()` : Returns `true` once at least one 20ms frame (960 samples) is buffered.
  - `pop_packet()` : Encodes one 20ms frame and returns it as a single raw Opus packet (`PackedByteArray`, no length header). Returns an empty array if not enough audio is buffered.
  - `reset_stream()` : Resets the encoder state and clears buffered audio. Call when a new talk burst starts.
- OpusDecoderNode
  - `decode_frame(packet)` : Decodes one packet from `pop_packet()` into a `PackedVector2Array` of stereo float frames, ready for `AudioStreamGeneratorPlayback.push_buffer()`.
  - `decode_dropped()` : Packet loss concealment. Call in place of `decode_frame()` when you detect a lost packet (e.g. via sequence numbers); returns one 20ms frame of the decoder's best guess.
  - `reset_stream()` : Resets the decoder state. Call between talk bursts.

Typical pump, sender side:
```gdscript
encoder.push_audio(capture.get_buffer(capture.get_frames_available()))
while encoder.has_packet():
	send_packet_rpc.rpc(encoder.pop_packet())
```
Receiver side, per packet received:
```gdscript
playback.push_buffer(decoder.decode_frame(packet))
```

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
