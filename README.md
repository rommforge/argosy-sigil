# argosy-sigil

A native C library for extracting platform-native title IDs / serials from
console ROM files. Designed for emulator launchers and save-sync tools that
need to pair local saves with the right RomM library entry.

`sigil` reads the ID directly from the disc/cart binary — the only reliable
source — and falls back to filename heuristics when binary extraction isn't
possible. It exposes both the canonical save-matching form and the raw form
as it appears in the binary, plus a classification of how the platform uses
that ID for save artifacts on disk.

## Status

Pre-1.0. Internal milestones M1–M5 implement progressively wider platform
coverage; 1.0 ships when all platforms are green against a real-ROM
integration test suite.

## License

MPL-2.0. See [LICENSE](LICENSE) for the full text and [TRADEMARKS.md](TRADEMARKS.md)
for the project naming policy.

## Supported platforms

| Slug | Platform | Sample inputs | Save form |
|---|---|---|---|
| `psp` | PSP | `.iso`, `.chd` | `ULUS10064` (folder prefix) |
| `psx` | PlayStation | `.iso`, `.bin`, `.chd` | `SLUS-12345` (file prefix) |
| `ps2` | PlayStation 2 | `.iso`, `.chd` | `SLUS-12345` (folder exact) |
| `psvita` | PS Vita | `.zip` (filename only) | `PCSE12345` (folder exact) |
| `switch` | Nintendo Switch | `.nsp`, `.xci` | `0100ABCD12345000` (folder exact) |
| `3ds` | Nintendo 3DS | `.3ds`, `.cci`, `.z3ds`, `.zcci` | `0004000000123456` (folder exact) |
| `wii` | Wii | `.iso`, `.rvz` | `525A5445` (folder exact, hex of ASCII) |
| `wiiu` | Wii U | `.wua` | `10143500` (folder exact, last 8 of folder name) |
| `gamecube` | GameCube | `.iso`, `.rvz` | `475A4C45` (file prefix, hex of ASCII) |

The slugs above are the canonical strings sigil's bindings expose to end
users (CLIs, config files). Internally the C API uses the `sigil_platform`
enum; `sigil_platform_from_slug()` converts.

## Quick example (C)

```c
#include <sigil.h>

sigil_result r;
int rc = sigil_extract_from_path("/path/to/game.iso", SIGIL_PLATFORM_AUTO,
                                 NULL, &r);
if (rc == 0) {
    printf("title_id=%s raw_serial=%s usage=%d source=%d\n",
           r.title_id, r.raw_serial, r.usage, r.source);
}
```

## Switch keys

Switch NCA header decryption requires `prod.keys`. Pass it via the support
struct:

```c
sigil_support sup = {
    .struct_version = SIGIL_SUPPORT_V1,
    .switch_prod_keys_path = "/path/to/prod.keys",
};
sigil_options opts = { .struct_version = SIGIL_OPTIONS_V1, .support = &sup };
sigil_extract_from_path("game.xci", SIGIL_PLATFORM_SWITCH, &opts, &r);
```

If keys are missing, sigil tries the unencrypted fallback path (NCAs whose
filenames are themselves the title ID) before giving up. Decrypted/homebrew
Switch dumps work without keys.

## PS2 save folder convention

Sigil emits the ROM serial (`SLUS-20675`). Real PS2 save folders on
AetherSX2/NetherSX2/PCSX2 are named `BASLUS-20675` or `BASLUS20675` —
literal `BA` prefix, with-or-without the internal dash. Consumers handle
this prefix themselves.

## Building

```sh
cmake -B build -S .
cmake --build build
```

Default build includes every platform. Feature flags for slim consumers:

- `-DSIGIL_WITH_CHD=OFF` — drop libchdr (~200KB)
- `-DSIGIL_WITH_SWITCH=OFF` — drop AES-XTS / Switch
- `-DSIGIL_WITH_WIIU=OFF` — drop WUA reader
- `-DSIGIL_WITH_3DS=OFF` — drop 3DS / zstd-streaming variant
- `-DSIGIL_WITH_FILENAME=OFF` — drop filename fallback regex

## Bindings

Reference bindings live under `bindings/`:
- `bindings/android/` — JNI shim for argosy-launcher
- `bindings/go/` — cgo wrapper for Grout

These are examples consumers may copy or reference; sigil itself is C.

## Testing

```sh
cmake --build build --target test     # synthetic unit tests
SIGIL_ROM_DIR=/path/to/roms ctest -R integration   # real-ROM smoke tests
```

## Contributing

PRs welcome. See TRADEMARKS.md for the naming policy.
