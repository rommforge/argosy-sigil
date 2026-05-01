# Sigil Kotlin facade is a thin wrapper around JNI; no public surface
# beyond com.nendo.sigil.Sigil and SigilResult.
-keep class com.nendo.sigil.Sigil { *; }
-keep class com.nendo.sigil.SigilResult { *; }
-keep class com.nendo.sigil.SigilResult$Source { *; }
-keep class com.nendo.sigil.SigilResult$Usage { *; }
