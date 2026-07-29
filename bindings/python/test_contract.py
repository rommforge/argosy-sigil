# SPDX-License-Identifier: MPL-2.0
"""Cross-language contract tests.

The C `sigil_usage` enum, the Python and CLI string maps, the Kotlin `Usage`
enum, and the JNI constructor descriptor are five hand-maintained copies of
the same facts. Nothing at compile time keeps them in step: a JNI descriptor
that disagrees with the Kotlin constructor only fails at first extraction on a
device, and a usage value missing from a string map only shows up at runtime.

These tests parse the sources directly (no compiled extension, no Android
toolchain) so a drift between any two copies fails in plain CI.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "include" / "sigil.h"
CLI = ROOT / "cli" / "sigil.c"
PY_INIT = ROOT / "bindings" / "python" / "sigil" / "__init__.py"
KOTLIN = ROOT / "bindings" / "android" / "src" / "main" / "kotlin" / "com" / "nendo" / "sigil" / "Sigil.kt"
JNI = ROOT / "bindings" / "android" / "src" / "main" / "cpp" / "sigil_jni.c"

# Kotlin/JVM primitive and reference types to their field descriptors.
_JVM_DESCRIPTORS = {
    "String": "Ljava/lang/String;",
    "Int": "I",
    "Long": "J",
    "Boolean": "Z",
    "Float": "F",
    "Double": "D",
}


def _canonical_string(enum_name: str) -> str:
    """FOLDER_EXACT -> folder-exact: the string form the maps must emit."""
    return enum_name.lower().replace("_", "-")


def _pascal_to_upper_snake(name: str) -> str:
    """FolderSplit -> FOLDER_SPLIT, to compare Kotlin names against C names."""
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()


def _c_usage_enum() -> list[tuple[str, int]]:
    block = re.search(
        r"typedef enum\s*\{(.*?)\}\s*sigil_usage;", HEADER.read_text(), re.DOTALL
    )
    assert block, "sigil_usage enum not found in sigil.h"
    pairs = re.findall(r"SIGIL_USAGE_(\w+)\s*=\s*(\d+)", block.group(1))
    assert pairs, "no SIGIL_USAGE_* members parsed"
    return [(name, int(value)) for name, value in pairs]


def _kotlin_usage_entries() -> list[tuple[str, int]]:
    # Enum entries run from the opening brace to the ';' before the companion.
    block = re.search(
        r"enum class Usage\(val code: Int\)\s*\{(.*?);", KOTLIN.read_text(), re.DOTALL
    )
    assert block, "Kotlin Usage enum not found"
    entries = re.findall(r"(\w+)\((\d+)\)", block.group(1))
    return [(_pascal_to_upper_snake(n), int(v)) for n, v in entries]


def _kotlin_ctor_types() -> list[str]:
    block = re.search(
        r"data class SigilResult\((.*?)\)", KOTLIN.read_text(), re.DOTALL
    )
    assert block, "Kotlin SigilResult constructor not found"
    types = re.findall(r"\bval\s+\w+:\s*(\w+)", block.group(1))
    assert types, "no SigilResult constructor params parsed"
    return types


def test_python_usage_map_covers_c_enum():
    c_names = {name for name, _ in _c_usage_enum()}
    py_map = dict(
        re.findall(r'lib\.SIGIL_USAGE_(\w+):\s*"([^"]+)"', PY_INIT.read_text())
    )
    assert set(py_map) == c_names, (
        "_USAGE_NAMES is out of sync with sigil_usage: "
        f"missing {c_names - set(py_map)}, extra {set(py_map) - c_names}"
    )
    for name, string in py_map.items():
        assert string == _canonical_string(name), f"{name} maps to {string!r}"


def test_cli_usage_switch_covers_c_enum():
    c_names = {name for name, _ in _c_usage_enum()}
    cli_map = dict(
        re.findall(r'case SIGIL_USAGE_(\w+):\s*return\s*"([^"]+)";', CLI.read_text())
    )
    assert c_names <= set(cli_map), (
        f"usage_to_str is missing cases: {c_names - set(cli_map)}"
    )
    for name, string in cli_map.items():
        assert string == _canonical_string(name), f"{name} returns {string!r}"


def test_kotlin_usage_enum_matches_c_enum():
    kotlin = _kotlin_usage_entries()
    c_enum = _c_usage_enum()
    assert kotlin == c_enum, (
        f"Kotlin Usage enum {kotlin} disagrees with sigil_usage {c_enum}"
    )


def _kotlin_ctor_descriptor() -> str:
    tokens = []
    for t in _kotlin_ctor_types():
        assert t in _JVM_DESCRIPTORS, f"unmapped Kotlin type {t!r}"
        tokens.append(_JVM_DESCRIPTORS[t])
    return "(" + "".join(tokens) + ")V"


def _jni_ctor_descriptor() -> str:
    match = re.search(r'"<init>",\s*"([^"]+)"', JNI.read_text(), re.DOTALL)
    assert match, "GetMethodID <init> descriptor not found in sigil_jni.c"
    return match.group(1)


def test_jni_descriptor_matches_kotlin_constructor():
    """The bug in cde2301: the JNI descriptor kept a String param the Kotlin
    constructor had dropped, so GetMethodID missed the constructor and every
    extraction crashed. This locks the two together."""
    assert _jni_ctor_descriptor() == _kotlin_ctor_descriptor()


def test_jni_string_args_match_descriptor():
    string_params = _jni_ctor_descriptor().count("Ljava/lang/String;")
    # Each String constructor arg is filled by one `jstring jX = ...` local.
    jstring_locals = len(re.findall(r"\bjstring\s+\w+\s*=", JNI.read_text()))
    assert jstring_locals == string_params, (
        f"{jstring_locals} jstring locals feed a {string_params}-String constructor"
    )
