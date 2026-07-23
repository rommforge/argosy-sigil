# SPDX-License-Identifier: MPL-2.0
"""Tests for the Python binding. Requires the compiled extension (make build)."""

import struct

import pytest

import sigil


def test_version_is_string():
    v = sigil.version()
    assert isinstance(v, str)
    assert v


def test_platform_slug_roundtrip():
    for slug in ("ps2", "psp", "switch", "xbox360"):
        p = sigil.platform_from_slug(slug)
        assert p != sigil.PLATFORM_AUTO
        assert sigil.platform_to_slug(p) == slug


def test_unknown_slug_maps_to_auto():
    p = sigil.platform_from_slug("commodore64")
    assert p == sigil.PLATFORM_AUTO
    assert sigil.platform_to_slug(p) == "auto"


def test_nonexistent_path_raises_io_error(tmp_path):
    missing = tmp_path / "no-such-file.iso"
    with pytest.raises(sigil.SigilIOError) as excinfo:
        sigil.extract(missing, platform="ps2")
    assert excinfo.value.code < 0


def _write_xex_fixture(path):
    # Minimal XEX2: one optional header (exec info) pointing at 0x100,
    # title ID bytes at 0x10C. Mirrors tests/unit_xex.c.
    buf = bytearray(1024)
    buf[0:4] = b"XEX2"
    buf[0x14:0x18] = struct.pack(">I", 1)
    buf[0x18:0x1C] = struct.pack(">I", 0x00040006)
    buf[0x1C:0x20] = struct.pack(">I", 0x100)
    buf[0x10C:0x110] = bytes([0x41, 0x4D, 0x07, 0xD1])
    path.write_bytes(bytes(buf))


def test_xex_extraction(tmp_path):
    xex = tmp_path / "default.xex"
    _write_xex_fixture(xex)

    result = sigil.extract(xex, platform="xbox360")
    assert result.title_id == "414D07D1"
    assert result.platform == "xbox360"
    assert result.source == "binary"
    assert result.experimental is True
