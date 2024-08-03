# Pebble Song Format

Designing this for the "lofi" unit; might need to change things if I write a more interesting synthesizer.

We expect MIDI files as input.

Format is like MIDI, with some differences:
 - All times are in milliseconds.
 - A sustain time is provided with each Note On; no Note Off events.
 - Fixed channel config; no changing channel properties while playing.
 - 8 channels instead of 16.
 - No aftertouch or pitch wheel.
 
File begins with a 4-byte signature: "L\0\0\xf1"

Followed by 8 channel headers, 16 bytes total:
```
0000   1 0xf8=env, 0x07=wave
0001   1 Trim 0..255
0002
```

Followed by events, distinguishable by their first byte:
```
00000000                   : EOF. Stop reading, return to the start.
0mmmmmmm                   : DELAY. (m) nonzero, milliseconds.
1cccnnnn nnnddddd dddddddd : NOTE. (c) channel id, (n) note id, (d) sustain time ms.
```
