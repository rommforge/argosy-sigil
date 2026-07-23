# sigil Python binding

cffi-based (API mode) Python binding for the sigil C library. The extension
statically links libsigil and its bundled decompression/crypto libs, so the
resulting module has no runtime dependency on the C build tree.

## Build

Requires Python >= 3.10, cffi, CMake, and a C compiler.

```sh
cd bindings/python
python3 -m venv .venv && . .venv/bin/activate
pip install cffi setuptools pytest
make build        # cmake the static libs into ../../build-python, then compile the extension in-place
make test         # build + pytest
```

Prebuilt wheels will come later via CI; for now build from source as above.

## Usage

```python
import sigil

result = sigil.extract("game.iso", platform="ps2")
print(result.title_id, result.platform, result.source)

# Switch titles may need key material:
result = sigil.extract("game.nsp", platform="switch",
                       prod_keys_path="~/.switch/prod.keys")
```

`extract()` raises a `SigilError` subclass on failure (`SigilNeedsKeyError`,
`SigilNotFoundError`, `SigilIOError`, ...); the C error code is on `.code`.

Note: filename fallback is disabled by default (the C library enables it by
default). Pass `filename_fallback=True` to opt in.
