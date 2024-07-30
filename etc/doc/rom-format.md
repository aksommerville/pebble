# Pebble ROM Format

Begins with an 8-byte signature: "\x00\xffPBLROM"

Followed by payload.
Start with a state machine: `{ tid=1, rid=1 }`.

Read one byte, its high two bits to determine the next action:
```
00000000                    EOF. Stop processing.
00dddddd                    TID. (d) nonzero. tid+=d, rid=1
01dddddd dddddddd           RID. rid+=d
10llllll                    SHORT. Followed by resource (tid:rid), length (l+1). rid+=1
11llllll llllllll llllllll  LONG. Followed by resource (tid:rid), length (l+65). rid+=1
```

`tid` is in 1..63.
`rid` is in 1..65535.
Longest possible resource is 4194368 bytes, just a hair over 4 MB.
Owing to the state machine, resources must be sorted by (tid,rid).
Every ROM must begin with metadata:1 (metadata is tid 1), then code:1 (tid 2).

## Data Directories

Typically, the ROM's content come from a single directory "data", formatted like so:
```
DATAROOT/
  metadata
  code.wasm
  strings/
    en-1-ui
    en-2-dialogue
    fr-1-ui
    fr-2-dialogue
    ...
  image/
    1-splash.png
    2-hero.png
    3.png
    4-etcetc.png
  TID[-TYPENAME]/
    RID[-NAME][[.COMMENT].FORMAT]
```

Structure of the data directory is pretty strict, to avoid the need for fuzzy resolution rules.
To wit:
- `DATAROOT/metadata` becomes metadata:1, it is special.
- `DATAROOT/code.wasm` becomes code:1, it is special.
- Everything else goes directly under a directory named for its type.
- Standard types may name these directories with the type name alone.
- Custom types must specify tid, and optionally name.
- `strings` resources may begin with the two-letter ISO 639 language code, which is transformed into the upper 10 bits of rid.
- - Alphabet: "abcdefghijklmnopqrstuvwxyz012345", first letter in 0xf800, second in 0x07c0.
- All other resources must begin with the decimal rid.
- A COMMENT or FORMAT of "raw" means `pbltool pack` will emit the file verbatim instead of compiling.

## Resource Types

|    tid | Name        | Description |
|--------|-------------|-------------|
|      0 |             | Illegal. |
|      1 | metadata    | See metadata-format.md. |
|      2 | code        | Wasm module. rid must be 1. |
|      3 | strings     | See strings-format.md. |
|      4 | image       | See image-format.md. |
|  5..15 |             | Reserved for future standard types. |
| 16..63 |             | Reserved for user-defined types. |
| 64..   |             | Illegal. |
