# argosy-sigil

A native C helper library that derives the game-native serial / title ID
from a ROM file. Designed for applications that want per-game save and
state files instead of per-platform — pair a local save to the right
upstream game record by reading the platform's own ID out of the ROM,
not by guessing from filenames.

## What it does

You hand sigil a path to a ROM. Sigil reads the platform-native
identifier directly from the disc/cart binary and hands back:

- `title_id` — the canonical save-matching form the platform's save
  system expects (`ULUS10064`, `SLUS-12345`, `0100ABCD12345000`, etc.)
- `raw_serial` — the ID exactly as it appears in the binary, before
  any normalization (`ULUS-10064`, `SLUS_123.45`, `RZTE`)
- `usage` — how the platform uses `title_id` to lay out save artifacts
  on disk (one folder per game, multiple folders sharing a prefix,
  one file, multiple files sharing a prefix)
- `source` — `binary` if the ID came from the file content
  (high-confidence, lockable) or `filename` if it had to fall back to
  scanning the filename for a community-naming bracket pattern

## What it isn't

- Not a hash-based game identifier. CRC32/MD5/SHA-1 matching against
  No-Intro / Redump / RetroAchievements is a different problem;
  hashing tells you "which dump is this," sigil tells you "what does
  the platform call this game."
- Not a save manager or sync client. Sigil tells you the ID; what you
  do with it (find a save folder, upload it somewhere, restore it) is
  on you.
- Not a generic ROM info library. Sigil extracts one thing — the
  identifier the platform's own save/state subsystem keys on. It does
  not give you region, language, version, hash, header dump, etc.

## Status

Pre-1.0. The API may evolve before 1.0. Every public struct has a
`struct_version` field so new fields can be added without breaking
existing consumers — but the function signatures and existing field
layouts can still change. Consumers vendoring sigil should pin a
specific commit; once 1.0 ships the C ABI freezes.

## License

MPL-2.0. See [LICENSE](LICENSE) for the full text and
[TRADEMARKS.md](TRADEMARKS.md) for the project naming policy. Anyone
can use sigil in any application (proprietary or open); modifications
to sigil's own files must remain MPL-2.0.

## Supported platforms

| Slug | Platform | Inputs | `title_id` example | `usage` |
|---|---|---|---|---|
| `psp` | PSP | `.iso`, `.chd` | `ULUS10064` | folder-prefix |
| `psx` | PlayStation | `.iso`, `.bin`, `.chd` | `SLUS-12345` | file-prefix |
| `ps2` | PlayStation 2 | `.iso`, `.chd` | `SLUS-20675` | folder-exact |
| `psvita` | PS Vita | `.zip` (filename only) | `PCSE12345` | folder-exact |
| `switch` | Nintendo Switch | `.nsp`, `.xci` | `0100ABCD12345000` | folder-exact |
| `3ds` | Nintendo 3DS | `.3ds`, `.cci`, `.z3ds`, `.zcci` | `0004000000123456` | folder-exact |
| `wii` | Wii | `.iso`, `.rvz` | `525A5445` (hex of ASCII gameId) | folder-exact |
| `wiiu` | Wii U | `.wua` | `10143500` (last 8 of folder name) | folder-exact |
| `gamecube` | GameCube | `.iso`, `.rvz` | `475A4C45` (hex of ASCII gameId) | file-prefix |

The slugs are stable and match argosy's internal platform identifiers.
The C API uses the `sigil_platform` enum; `sigil_platform_from_slug()`
converts strings if your binding accepts user input.

## `usage` — what to do with the title ID

| Value | Meaning | Example |
|---|---|---|
| `folder-exact` | One folder per game named exactly `title_id` | `Switch/saves/0100ABCD12345000/` |
| `folder-prefix` | Multiple folders per game, all starting with `title_id` and a profile/slot suffix. Consumers MUST enumerate and bundle all matches. | PSP: `ULUS10064DATA00`, `ULUS10064SETTINGS`, `ULUS10064SAVE01` |
| `file-exact` | One file per game named with `title_id` | rare; emulator-specific |
| `file-prefix` | Multiple files per game, all containing `title_id` in the basename | GameCube GCI: `<maker>-<gameId>-<name>.gci` (e.g. `01-GZLE-Animal Crossing.gci`) |

Treating a `prefix` platform as `exact` silently misses every save for
that platform — sigil emits this classification so dispatch is correct
without you re-deriving it.

## Quick example (C)

```c
#include <sigil.h>
#include <stdio.h>

int main(void) {
    sigil_result r;
    int rc = sigil_extract_from_path("/path/to/game.iso",
                                     SIGIL_PLATFORM_AUTO, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "sigil: %s\n", sigil_strerror(rc));
        return 1;
    }
    printf("platform=%s\n",   sigil_platform_to_slug(r.platform));
    printf("title_id=%s\n",   r.title_id);     /* what to use for save lookup */
    printf("raw_serial=%s\n", r.raw_serial);   /* as it appears in the binary */
    printf("source=%s\n",     r.source == SIGIL_SOURCE_BINARY ? "binary" : "filename");
    /* r.usage: SIGIL_USAGE_FOLDER_EXACT | _FOLDER_PREFIX | _FILE_EXACT | _FILE_PREFIX */
    return 0;
}
```

## CLI

A reference `sigil(1)` is built alongside the library. Useful for
spot-checks during integration without writing any code:

```sh
$ sigil /path/to/game.xci --platform=switch --prod-keys=/path/to/prod.keys
platform=switch title_id=0100ABCD12345000 raw_serial=0100ABCD12345000 usage=folder-exact source=binary

$ sigil "/path/to/Persona 2 - Eternal Punishment (USA).chd"
platform=psx title_id=SLUS-01158 raw_serial=SLUS_011.58 usage=file-prefix source=binary
```

Pass `--platform=auto` (the default) to sniff from the file extension.

## Switch keys

Switch NCAs are encrypted; extracting the title ID from a retail XCI
or NSP requires the `header_key` from a `prod.keys` file. Pass it via
the support struct in any of three forms — sigil resolves them in
this priority order:

```c
sigil_support sup = {
    .struct_version = SIGIL_SUPPORT_V1,

    /* (1) Highest priority — raw 32-byte key, for callers that
     *     already loaded prod.keys themselves. */
    .switch_header_key = my_header_key_bytes,

    /* (2) In-memory text blob — for sandboxed environments like
     *     Android SAF where direct file I/O is mediated. */
    .switch_prod_keys_text = prod_keys_blob,
    .switch_prod_keys_text_len = prod_keys_blob_len,

    /* (3) Path on disk — sigil opens and parses the file. */
    .switch_prod_keys_path = "/path/to/prod.keys",
};
sigil_options opts = { .struct_version = SIGIL_OPTIONS_V1, .support = &sup };
sigil_extract_from_path("game.xci", SIGIL_PLATFORM_SWITCH, &opts, &r);
```

If no key is provided, sigil tries the unencrypted-NCA fallback path
(NSPs / XCIs whose NCA filenames are themselves the 16-hex title ID).
This works for decrypted dumps and homebrew but encrypted retail
content will fall through to the filename source — set
`SIGIL_FLAG_FILENAME_FALLBACK` in `opts.flags` to allow that, or
unset it to fail cleanly.

## Platform-specific notes

**PSP — folder prefix.** A single game produces multiple sibling
folders under `PSP/SAVEDATA/`, e.g. `ULUS10064DATA00`,
`ULUS10064SETTINGS`, `ULUS10064SAVE01`. Sigil emits the 9-char prefix
(`ULUS10064`); consumers MUST enumerate every folder under the parent
that starts with that prefix.

**GameCube — file prefix.** Saves are `.gci` files with the
convention `<makerCode>-<gameId>-<internalName>.gci`. Sigil emits the
hex-encoded ASCII gameId (`475A4C45` for `GZLE`); consumers match
files whose basename contains `-<gameId>-`. argosy's `GciSaveHandler`
is a reference implementation.

**PS2 — `BA` save-side prefix.** Sigil emits the ROM serial
(`SLUS-20675`). Real PS2 save folders on AetherSX2/NetherSX2/PCSX2
are named `BASLUS-20675` or `BASLUS20675` — literal `BA` prefix,
with-or-without the internal dash. Consumers prepend `BA` and match
both dash variants.

**Wii / GameCube — title ID is hex of ASCII.** The disc header
carries a 4-character ASCII gameId (`RZTE`, `GZLE`). The save form
is the hex encoding of those bytes (`52535445`, `475A4C45`) — that's
what Dolphin's NAND structure uses. `raw_serial` preserves the ASCII
form for human-readable logging; use `title_id` for actual save
lookup.

**Wii U — last 8 of 16-hex.** WUA archives carry a top-level folder
named `00050000<8 hex>_v0`. The full 16 hex is the formal title ID;
the last 8 chars are what the save system keys on. Sigil emits the
last 8 as `title_id`, the full 16 as `raw_serial`.

**3DS — `0004` retail filter.** NCSD program IDs not starting with
`0004` are filtered as non-retail (system titles, CIAs, etc.). Set
`SIGIL_FLAG_3DS_ALLOW_HOMEBREW` in `opts.flags` to disable the gate
for CIA/homebrew workflows.

## Building

```sh
cmake -B build -S .
cmake --build build
```

Default build includes every platform. Feature flags for slim
consumers:

| Flag | Drops |
|---|---|
| `-DSIGIL_WITH_CHD=OFF` | libchdr + zlib + lzma + zstd (~200KB) |
| `-DSIGIL_WITH_SWITCH=OFF` | AES-XTS + tiny-AES + Switch NSP/XCI/NCA |
| `-DSIGIL_WITH_WIIU=OFF` | WUA reader |
| `-DSIGIL_WITH_3DS=OFF` | 3DS NCSD + zstd-streaming variant |
| `-DSIGIL_WITH_FILENAME=OFF` | Per-platform filename pattern scanners |
| `-DSIGIL_BUILD_SHARED=ON` | Build `libsigil.so` instead of `.a` |
| `-DSIGIL_BUILD_CLI=OFF` | Skip the `sigil(1)` reference CLI |
| `-DSIGIL_BUILD_TESTS=OFF` | Skip tests |

## Bindings

Reference bindings live under `bindings/`:

- `bindings/android/` — Gradle library module wrapping the C ABI for
  Kotlin/Java consumers via JNI. Used by argosy-launcher; usable by
  any Android app. See [bindings/android/README.md](bindings/android/README.md).

The C library itself is the supported distribution; bindings are
examples consumers may copy or extend. Additional bindings (Go via
cgo for Grout, Rust via bindgen, etc.) can be added under
`bindings/<lang>/` — sigil's small public API and stable enum
numbering keep these straightforward.

## Testing

```sh
# Synthetic unit tests (fast, no ROMs needed)
cmake --build build && ctest --test-dir build

# Real-ROM integration tests — point at a directory with platform
# subdirs (psp/, psx/, ps2/, switch/, 3ds/, wii/, wiiu/, ngc/, psvita/).
SIGIL_ROM_DIR=/path/to/roms ctest --test-dir build -R integration

# Switch tests additionally need a prod.keys file
SIGIL_ROM_DIR=/path/to/roms \
SIGIL_PROD_KEYS=/path/to/prod.keys \
ctest --test-dir build -R integration_switch

# Cap samples per platform (default 25). Useful when iterating on a
# library with hundreds of CHDs per platform.
SIGIL_SAMPLE_LIMIT=10 SIGIL_ROM_DIR=/path/to/roms \
ctest --test-dir build -R integration
```

Integration tests skip cleanly with exit code 77 when env vars are
unset, so the public CI without ROMs can still run unit tests.

## Contributing

PRs welcome. See [TRADEMARKS.md](TRADEMARKS.md) for the naming
policy: forks-for-contribution (standard fork → PR-upstream flow)
are encouraged; forks-and-republish-as-a-separate-project under the
`argosy-sigil` name are not.
