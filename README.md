# libOpus-gdnative
GDnative integration of libOpus for Godot

## So far this is only proof of concept quality, and is in no way ready to be used in production.

One key component Godot is missing for enabling VOIP features, is a, audio code that can be used at run-time to compress and decompress audio for transmission.

libOpus is the gold standard for open source real-time voice codecs.

So this is an attempt at exposing libOpus's functionality to Godot via a GDNative library.

This adds 2 nodes to Godot:
- OpusEncoder
  - `encode(data)` : Accepts a `PoolByteArray` of 44.1kH 16bit PCM Stereo audio samples. Returns a `PoolByteArray` of Opus data Packets interleaved with header data describing their individual lengths. (*This is a bit custom, and not the real™ way to pack these Opus data packets. But it is the simplest.*)
- OpusDecoder
  - `decode(data)` : Accepts a `PoolByteArray` of Opus data Packets packed in our custom interleaves format. Returns raw 16bit stereo PCM data at 44.1kH
  
The current Nodes and methods are meant to be used to encode a whole, continues sample of audio. Then again decode that whole continious audio sample. ***THIS DOES NOT YET ENABLE TRUE AUDIO STREAMING.***
All though there is nothing that should prevent us from using all of libOpus's advanced streaming related features, I need to get my head around how that will actually integrate with both Godot's mic capture system, and it's audio playback system.
