# Naming Policy

This file documents the naming policy for `argosy-sigil`. It is separate from
the LICENSE file because trademark/naming concerns are conceptually distinct
from license terms.

## Project name

The canonical project name is **`argosy-sigil`**. This is the only name under
which the project is distributed.

## Forks for contribution

Standard GitHub fork → branch → pull-request-upstream flow is welcomed and
encouraged. Forks created for the purpose of contributing back may keep the
`argosy-sigil` name during development, since they are intended to merge
upstream rather than ship as a separate project.

## Forks for divergent projects

Anyone wishing to ship a divergent project derived from sigil's source —
that is, a fork that is not intended to merge upstream — must rename the
project. The fork must not represent itself as `argosy-sigil` or any obvious
variation that could be confused with `argosy-sigil`.

The MPL-2.0 license under which sigil is distributed requires that
modifications to sigil's source files remain open and licensed under MPL-2.0.
This naming policy is in addition to the license: the license keeps your
changes visible; the naming policy keeps your project distinguishable.

## Library symbol prefix

The C library identifier prefix is **`sigil_`** (e.g.
`sigil_extract_from_path`, `sigil_io`, `sigil_result`). This is a code-level
naming convention, not a separate brand — it is short for the compound
project name.

Bindings should follow the same convention:
- Go: package name `sigil`
- Java/Kotlin: package `com.nendo.sigil`
- Rust: crate name `sigil` or `sigil-sys`
- Python: module name `sigil`

## Questions

If you are unsure whether a planned use of the name is appropriate, open an
issue at the project's GitHub repository.
