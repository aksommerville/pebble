# Pebble Metadata Format

The input file `DATAROOT/metadata` is line-oriented text.
- '#' begins a line comment, at the start of line only.
- Non-empty lines are `KEY=VALUE`, or `KEY:VALUE`, with optional whitespace between all tokens.
- Text should be ASCII.
- Both KEY and VALUE are limited to 255 bytes.
- KEY must not be empty.
- KEY beginning with a lowercase letter are reserved for Standard Fields, defined here.
- Custom KEY are perfectly legal, but please start with an uppercase letter or punctuation.
- VALUE may be empty but there's no reason to; an empty VALUE is equivalent to a missing field.
- Duplicate KEY is an error. Decoder's behavior will be undefined.

This compiles to a binary format in the ROM file.
```
0000   4 Signature = "\x00PM\xff"
0004   1 Key length.
0005   1 Value length.
0006 ... Key.
.... ... Value.
....   1 Key length
.... ... etc
....   1 Terminator: 0
```

The Terminator is required. It ensures that zero-length keys are impossible, and permits
runtime to scan metadata without checking lengths, after having validated just once.

Every ROM file must contain a resource `metadata:1` which must be the first resource.
Even if there is no content in it. (actually, it's required to contain at least "fb").

## Key Conventions

First character is lowercase for Standard Fields, anything else for custom fields.

Last character may be punctuation for a processing hint:
- `#`: Value is numeric, should be evaluated as a signed decimal integer.
- `$`: Value is decimal index of a string in `strings:1` (+language) to substitute.
- `@`: Value is plain text intended for human consumption.

Tools that show a ROM's metadata may opt to display unknown `@` fields, even if they otherwise hide unknown fields.
Intent is for things that you want to tell all users, but I failed to enumerate in the Standard Fields.
You may also include a string version, replacing `@` with `$`.

Example:
```
FDA Approval Disclaimer@ = This software has not been approved for internal consumption. Eat at your own risk.
FDA Approval Disclaimer$ = 3
```
and `strings:1`, index 3, would contain the same message possibly translated or in longer form.

## Standard Fields

All '@' fields may also have a '$' companion; I'm not listing both.

| Key                | Description |
|--------------------|-------------|
| fb                 | REQUIRED. "WIDTHxHEIGHT", the size of your framebuffer. |
| audioRate#         | Hz. If zero, we'll pick something nice. Not guaranteed to get the rate you ask for. |
| audioChannels#     | Typically 1 or 2. Same rules as audioRate# |
| title@             | Game's full title for display. |
| author@            | Your name, or your company's. |
| time               | Publication date, ISO 8601, eg "YYYY" or "YYYY-MM-DD". |
| version            | Version of your game (nothing to do with the pebble runtime version). Freeform, but I recommend "MAJOR.MINOR.REVISION". |
| lang               | Two-letter ISO 639 codes, comma-delimited, in order of your preference. |
| genre@             | A single string, what kind of game is it. Recommend using strings popular on itch.io, so we have a common reference. |
| tags@              | Comma-delimited list of tags. Recommend using ones popular on itch.io. |
| rating             | Machine-readable ratings from official agencies. (TODO format) |
| advisory@          | Freeform content advisory, eg "Contains profanity and nudity." |
| desc@              | Freeform description. Recommend using the string version, so no length limit. |
| freedom            | "restricted","limited","intact","free". See below. |
| required           | Comma-delimited list of runtime features you can't run without. (TODO define these) |
| optional           | Same as `required`, but you can operate without if needed. |
| players            | Integer or "LOW..HIGH", how many can play. "1..2" is likelier than "2", which would mean "exactly two players" |
| iconImage#         | ID of an image resource to use as window manager icon or favicon. Recommend PNG, 16x16, with alpha. |
| copyright@         | Freeform text eg "(c) 2024 AK Sommerville" |
| persistKey         | If present, all builds with the same title and persistKey have compatible persistence. Otherwise we use version too. |

`freedom`: A quick answer to copyright questions, are users allowed to copy this file?
Default is "limited".
- `restricted`: Users are not necessarily allowed even to play the game. Authors should include a license explaining.
- `limited`: Users may assume they're allowed to play it, but not to copy it.
- `intact`: Game may be redistributed freely as long as it's not modified. Do not assume you're allowed to extract assets.
- `free`: Users may assume they're allowed to yoink assets or distribute modified versions of the game.
