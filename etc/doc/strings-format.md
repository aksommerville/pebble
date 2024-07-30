# Pebble Strings Format

Loose text for use in-game is stored in `strings` resources.
Each contains any number of strings, indexed from zero, length limited to 65535.
UTF-8 is encouraged but in truth we don't care much about encoding.

The rid of a strings resource, for most purposes, is in 1..63 instead of 1..65535.
The high 10 bits are a language ID.
```
actual_rid=(
  ((lang[0]-'a')<<11)|
  ((lang[1]-'a')<<6)|
  stated_rid
)
```

Binary file begins with a 4-byte signature: "\x00PS\xff"
Followed by strings, starting with index zero, each prefixed with their length, 2 bytes big-endian.
Length zero is perfectly legal, it's not a terminator or anything.

Source format is line-oriented text:
- '#' begins a line comment, start of line only.
- Empty lines are quietly ignored.
- Line begins with INDEX: a decimal integer >=0.
- Lines must be sorted by INDEX.
- Remainder of line is loose text or a JSON string token.
- You must use JSON if there's leading space, trailing space, embedded newlines, or a leading quote.
- We do not provide facilities for naming strings or auto-assigning index, that bookkeeping is up to you.
- Strings get stored sequentially. It's fine to leave index gaps, but don't overdo it.
