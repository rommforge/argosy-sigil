// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

package com.nendo.sigil

/**
 * Result of a successful title-id extraction. Mirrors the C `sigil_result`
 * struct one-to-one.
 *
 * - [titleId]: canonical save-matching form. PSP `ULUS10064`, PSX/PS2
 *   `SLUS-12345`, Switch `0100ABCD12345000`, Wii `525A5445`, etc. This is
 *   the value to pass to the platform's save subsystem.
 * - [rawSerial]: ID as it appears in the binary, before normalization. PSP
 *   `ULUS-10064` (with dash), PSX/PS2 `SLUS_123.45` (with separators), Wii
 *   `RZTE` (4 ASCII chars). Equal to [titleId] for platforms that already
 *   store the canonical form on disc (Switch, 3DS, Vita).
 * - [platformSlug]: canonical platform identifier (`psp`, `psx`, `ps2`,
 *   `psvita`, `switch`, `3ds`, `wii`, `wiiu`, `gamecube`).
 * - [source]: [Source.Binary] when extracted from the file content (locked
 *   high-confidence), [Source.Filename] when the binary parser failed and a
 *   filename pattern was used (low-confidence hint).
 * - [usage]: how the platform uses [titleId] for save artifacts on disk.
 *   See [Usage] — `EXACT` is one-folder/file per game, `PREFIX` is multiple.
 */
data class SigilResult(
    val titleId: String,
    val rawSerial: String,
    val platformSlug: String,
    private val sourceCode: Int,
    private val usageCode: Int
) {
    val source: Source get() = Source.fromCode(sourceCode)
    val usage: Usage get() = Usage.fromCode(usageCode)

    enum class Source(val code: Int) {
        Binary(0),
        Filename(1);
        companion object {
            fun fromCode(c: Int): Source = values().firstOrNull { it.code == c } ?: Filename
        }
    }

    enum class Usage(val code: Int) {
        FolderExact(0),
        FolderPrefix(1),
        FileExact(2),
        FilePrefix(3);
        companion object {
            fun fromCode(c: Int): Usage = values().firstOrNull { it.code == c } ?: FolderExact
        }
    }
}

/**
 * Sigil — extract platform-native title IDs / serials from console ROM
 * files. Designed for emulator launchers and save-sync tools that need
 * to pair local saves with the right RomM library entry.
 *
 * Usage:
 * ```
 * val r = Sigil.extract("/path/to/game.iso", platformSlug = "ps2")
 * if (r != null) {
 *     // r.titleId  -> "SLUS-20675"
 *     // r.usage    -> Usage.FolderExact
 *     // r.source   -> Source.Binary
 * }
 * ```
 *
 * For Switch ROMs, pass `prodKeysPath` to enable encrypted-NCA decryption.
 * Without it, sigil falls back to the unencrypted-NCA filename path; this
 * works for decrypted/homebrew dumps but encrypted retail XCIs return
 * a filename-source result (or null).
 */
object Sigil {
    init {
        System.loadLibrary("sigil-jni")
    }

    /**
     * Returns the sigil C library version string.
     */
    @JvmStatic external fun nativeVersion(): String

    /**
     * Native entry. Returns null on extraction failure. Prefer the [extract]
     * convenience wrapper for normal use.
     */
    @JvmStatic external fun nativeExtract(
        path: String,
        platformSlug: String?,
        prodKeysPath: String?
    ): SigilResult?

    /**
     * Extract a title ID from a ROM file.
     *
     * @param path absolute path to the ROM file
     * @param platformSlug canonical platform slug (e.g. "psp", "switch") or
     *                     null/"auto" to sniff from the file extension
     * @param prodKeysPath path to a Switch prod.keys file, or null. Only used
     *                     for SIGIL_PLATFORM_SWITCH; ignored otherwise.
     */
    fun extract(path: String, platformSlug: String? = null, prodKeysPath: String? = null): SigilResult? =
        nativeExtract(path, platformSlug, prodKeysPath)
}
