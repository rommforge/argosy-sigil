// SPDX-License-Identifier: MPL-2.0

// Package sigil derives the platform-native title ID from a ROM file.
package sigil

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lsigil -lsigil_chdr -lsigil_zstd -lsigil_zlib -lsigil_lzma -lsigil_aes

#include <stdlib.h>
#include "sigil.h"
*/
import "C"

import (
	"errors"
	"fmt"
	"unsafe"
)

// Platform identifies a console platform.
type Platform int

const (
	PlatformAuto     Platform = C.SIGIL_PLATFORM_AUTO
	PlatformPSP      Platform = C.SIGIL_PLATFORM_PSP
	PlatformPSX      Platform = C.SIGIL_PLATFORM_PSX
	PlatformPS2      Platform = C.SIGIL_PLATFORM_PS2
	PlatformPSVita   Platform = C.SIGIL_PLATFORM_PSVITA
	PlatformSwitch   Platform = C.SIGIL_PLATFORM_SWITCH
	Platform3DS      Platform = C.SIGIL_PLATFORM_3DS
	PlatformWii      Platform = C.SIGIL_PLATFORM_WII
	PlatformWiiU     Platform = C.SIGIL_PLATFORM_WIIU
	PlatformGameCube Platform = C.SIGIL_PLATFORM_GAMECUBE
)

// Slug returns the canonical string slug for this platform.
func (p Platform) Slug() string {
	return C.GoString(C.sigil_platform_to_slug(C.sigil_platform(p)))
}

func (p Platform) String() string { return p.Slug() }

// PlatformFromSlug parses a slug; unknown slugs return PlatformAuto.
func PlatformFromSlug(slug string) Platform {
	cs := C.CString(slug)
	defer C.free(unsafe.Pointer(cs))
	return Platform(C.sigil_platform_from_slug(cs))
}

// Source distinguishes binary-extracted IDs from filename-pattern matches.
type Source int

const (
	SourceBinary   Source = C.SIGIL_SOURCE_BINARY
	SourceFilename Source = C.SIGIL_SOURCE_FILENAME
)

func (s Source) String() string {
	switch s {
	case SourceBinary:
		return "binary"
	case SourceFilename:
		return "filename"
	default:
		return fmt.Sprintf("Source(%d)", int(s))
	}
}

// Usage classifies how the platform uses the title ID for save artifacts.
// EXACT vs PREFIX is load-bearing — see README.
type Usage int

const (
	UsageFolderExact  Usage = C.SIGIL_USAGE_FOLDER_EXACT
	UsageFolderPrefix Usage = C.SIGIL_USAGE_FOLDER_PREFIX
	UsageFileExact    Usage = C.SIGIL_USAGE_FILE_EXACT
	UsageFilePrefix   Usage = C.SIGIL_USAGE_FILE_PREFIX
)

func (u Usage) String() string {
	switch u {
	case UsageFolderExact:
		return "folder-exact"
	case UsageFolderPrefix:
		return "folder-prefix"
	case UsageFileExact:
		return "file-exact"
	case UsageFilePrefix:
		return "file-prefix"
	default:
		return fmt.Sprintf("Usage(%d)", int(u))
	}
}

// Result is a successful extraction.
type Result struct {
	TitleID   string
	RawSerial string
	Platform  Platform
	Source    Source
	Usage     Usage
}

// Options controls extraction behavior. Zero value is valid.
type Options struct {
	SwitchHeaderKey         []byte // 32 bytes; wins over path/blob if set
	SwitchProdKeysPath      string
	SwitchProdKeysBlob      []byte
	DisableFilenameFallback bool
	Allow3DSHomebrew        bool
}

var (
	ErrInvalidArg        = errors.New("sigil: invalid argument")
	ErrIO                = errors.New("sigil: I/O error")
	ErrUnknownPlatform   = errors.New("sigil: unknown platform")
	ErrUnsupportedFormat = errors.New("sigil: unsupported format")
	ErrNotFound          = errors.New("sigil: title id not found")
	ErrNeedsKey          = errors.New("sigil: decryption key required")
	ErrCrypto            = errors.New("sigil: crypto failure")
	ErrOOM               = errors.New("sigil: out of memory")
)

func errFromCode(rc C.int) error {
	switch rc {
	case C.SIGIL_OK:
		return nil
	case C.SIGIL_ERR_INVALID_ARG:
		return ErrInvalidArg
	case C.SIGIL_ERR_IO:
		return ErrIO
	case C.SIGIL_ERR_UNKNOWN_PLATFORM:
		return ErrUnknownPlatform
	case C.SIGIL_ERR_UNSUPPORTED_FORMAT:
		return ErrUnsupportedFormat
	case C.SIGIL_ERR_NOT_FOUND:
		return ErrNotFound
	case C.SIGIL_ERR_NEEDS_KEY:
		return ErrNeedsKey
	case C.SIGIL_ERR_CRYPTO:
		return ErrCrypto
	case C.SIGIL_ERR_OOM:
		return ErrOOM
	default:
		return fmt.Errorf("sigil: error %d", int(rc))
	}
}

// Extract reads the title ID from path. Pass PlatformAuto to sniff from
// the file extension. opts may be nil.
func Extract(path string, hint Platform, opts *Options) (*Result, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	// Allocate options + support on the C heap. cgo forbids passing a Go
	// struct that contains a Go pointer to another Go-allocated struct.
	coptions := (*C.sigil_options)(C.calloc(1, C.sizeof_sigil_options))
	defer C.free(unsafe.Pointer(coptions))
	coptions.struct_version = C.SIGIL_OPTIONS_V1

	var (
		csupport     *C.sigil_support
		ckeyBytes    unsafe.Pointer
		ckeysPath    *C.char
		ckeysBlob    unsafe.Pointer
	)
	defer func() {
		if csupport != nil {
			C.free(unsafe.Pointer(csupport))
		}
		if ckeyBytes != nil {
			C.free(ckeyBytes)
		}
		if ckeysPath != nil {
			C.free(unsafe.Pointer(ckeysPath))
		}
		if ckeysBlob != nil {
			C.free(ckeysBlob)
		}
	}()

	if opts != nil {
		if !opts.DisableFilenameFallback {
			coptions.flags |= C.SIGIL_FLAG_FILENAME_FALLBACK
		}
		if opts.Allow3DSHomebrew {
			coptions.flags |= C.SIGIL_FLAG_3DS_ALLOW_HOMEBREW
		}

		needSupport := len(opts.SwitchHeaderKey) > 0 ||
			opts.SwitchProdKeysPath != "" ||
			len(opts.SwitchProdKeysBlob) > 0
		if needSupport {
			csupport = (*C.sigil_support)(C.calloc(1, C.sizeof_sigil_support))
			csupport.struct_version = C.SIGIL_SUPPORT_V1

			if len(opts.SwitchHeaderKey) > 0 {
				if len(opts.SwitchHeaderKey) != 32 {
					return nil, fmt.Errorf("sigil: SwitchHeaderKey must be 32 bytes, got %d", len(opts.SwitchHeaderKey))
				}
				ckeyBytes = C.CBytes(opts.SwitchHeaderKey)
				csupport.switch_header_key = (*C.uchar)(ckeyBytes)
			}
			if opts.SwitchProdKeysPath != "" {
				ckeysPath = C.CString(opts.SwitchProdKeysPath)
				csupport.switch_prod_keys_path = ckeysPath
			}
			if len(opts.SwitchProdKeysBlob) > 0 {
				ckeysBlob = C.CBytes(opts.SwitchProdKeysBlob)
				csupport.switch_prod_keys_text = (*C.char)(ckeysBlob)
				csupport.switch_prod_keys_text_len = C.size_t(len(opts.SwitchProdKeysBlob))
			}
			coptions.support = csupport
		}
	} else {
		coptions.flags = C.SIGIL_FLAG_FILENAME_FALLBACK
	}

	var cresult C.sigil_result
	rc := C.sigil_extract_from_path(cpath, C.sigil_platform(hint), coptions, &cresult)
	if err := errFromCode(rc); err != nil {
		return nil, err
	}

	return &Result{
		TitleID:   C.GoString(&cresult.title_id[0]),
		RawSerial: C.GoString(&cresult.raw_serial[0]),
		Platform:  Platform(cresult.platform),
		Source:    Source(cresult.source),
		Usage:     Usage(cresult.usage),
	}, nil
}

// Version returns the sigil C library version.
func Version() string { return C.GoString(C.sigil_version()) }
