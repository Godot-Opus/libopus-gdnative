# opus-gdnative
GDnative integration of libOpus for Godot

## So far this is only proof of concept quality, and is in no way ready to be used in production.

One key component Godot is missing for enavling VOIP, is a proper voice oriented Codec. libOpus is the gold standard for open source real-time voice codecs.

So this is an attempt and exposing libOpus's functionality to Godot via a GDNative library.

This adds 2 nodes to Godot:
- OpusEncoder
- OpusDecoder
