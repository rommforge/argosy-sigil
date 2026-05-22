// SPDX-License-Identifier: MPL-2.0
package com.nendo.sigil

/** Result of a successful title-id extraction. */
data class SigilResult(
    val titleId: String,
    val rawSerial: String,
    val saveId: String,
    val platformSlug: String,
    private val sourceCode: Int,
    private val usageCode: Int,
    val experimental: Boolean = false
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

    /** EXACT vs PREFIX is load-bearing — see project README. */
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
 * Sigil — extract platform-native title IDs from console ROM files.
 *
 * For Switch, pass `prodKeysPath` to enable encrypted-NCA decryption.
 * Calls block on I/O — invoke from a background thread.
 */
object Sigil {
    init {
        System.loadLibrary("sigil-jni")
    }

    @JvmStatic external fun nativeVersion(): String

    @JvmStatic external fun nativeExtract(
        path: String,
        platformSlug: String?,
        prodKeysPath: String?
    ): SigilResult?

    fun extract(path: String, platformSlug: String? = null, prodKeysPath: String? = null): SigilResult? =
        nativeExtract(path, platformSlug, prodKeysPath)
}
