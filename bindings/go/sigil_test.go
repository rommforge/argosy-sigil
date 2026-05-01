// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

package sigil

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestVersion(t *testing.T) {
	if Version() == "" {
		t.Fatal("Version() returned empty string")
	}
}

func TestPlatformSlugRoundtrip(t *testing.T) {
	cases := []struct {
		p    Platform
		slug string
	}{
		{PlatformPSP, "psp"},
		{PlatformPSX, "psx"},
		{PlatformPS2, "ps2"},
		{PlatformPSVita, "psvita"},
		{PlatformSwitch, "switch"},
		{Platform3DS, "3ds"},
		{PlatformWii, "wii"},
		{PlatformWiiU, "wiiu"},
		{PlatformGameCube, "gamecube"},
	}
	for _, c := range cases {
		if got := c.p.Slug(); got != c.slug {
			t.Errorf("%v.Slug() = %q, want %q", c.p, got, c.slug)
		}
		if got := PlatformFromSlug(c.slug); got != c.p {
			t.Errorf("PlatformFromSlug(%q) = %v, want %v", c.slug, got, c.p)
		}
	}
}

func TestUnknownSlug(t *testing.T) {
	if got := PlatformFromSlug("plystation"); got != PlatformAuto {
		t.Errorf("got %v, want PlatformAuto", got)
	}
}

func TestIntegration(t *testing.T) {
	dir := os.Getenv("SIGIL_ROM_DIR")
	if dir == "" {
		t.Skip("SIGIL_ROM_DIR not set")
	}

	cases := []struct {
		platform     Platform
		subdir       string
		exts         []string
		expectBinary bool // false for filename-only platforms (Vita)
	}{
		{PlatformPSP, "psp", []string{".chd", ".iso"}, true},
		{PlatformPSX, "psx", []string{".chd", ".bin", ".iso"}, true},
		{PlatformPS2, "ps2", []string{".chd", ".iso"}, true},
		{PlatformPSVita, "psvita", []string{".zip"}, false},
		{Platform3DS, "3ds", []string{".3ds", ".cci"}, true},
		{PlatformWii, "wii", []string{".rvz", ".iso"}, true},
		{PlatformGameCube, "ngc", []string{".rvz", ".iso"}, true},
		{PlatformWiiU, "wiiu", []string{".wua"}, true},
	}

	const limit = 5

	for _, c := range cases {
		t.Run(c.platform.Slug(), func(t *testing.T) {
			platDir := filepath.Join(dir, c.subdir)
			entries, err := os.ReadDir(platDir)
			if err != nil {
				t.Skipf("no %s samples: %v", c.platform, err)
			}

			seen := 0
			for _, e := range entries {
				if seen >= limit {
					break
				}
				if e.IsDir() || strings.HasPrefix(e.Name(), ".") {
					continue
				}
				ext := strings.ToLower(filepath.Ext(e.Name()))
				keep := false
				for _, want := range c.exts {
					if ext == want {
						keep = true
						break
					}
				}
				if !keep {
					continue
				}

				path := filepath.Join(platDir, e.Name())
				r, err := Extract(path, c.platform, nil)
				if err != nil {
					t.Errorf("%s: %v", e.Name(), err)
					continue
				}
				if r.TitleID == "" {
					t.Errorf("%s: empty TitleID", e.Name())
				}
				if c.expectBinary && r.Source != SourceBinary {
					t.Errorf("%s: source=%v, want binary", e.Name(), r.Source)
				}
				seen++
			}
			if seen == 0 {
				t.Skipf("no matching files in %s", platDir)
			}
		})
	}
}

func TestSwitchWithKeys(t *testing.T) {
	dir := os.Getenv("SIGIL_ROM_DIR")
	keys := os.Getenv("SIGIL_PROD_KEYS")
	if dir == "" || keys == "" {
		t.Skip("SIGIL_ROM_DIR or SIGIL_PROD_KEYS not set")
	}

	platDir := filepath.Join(dir, "switch")
	entries, err := os.ReadDir(platDir)
	if err != nil {
		t.Skipf("no Switch samples: %v", err)
	}

	opts := &Options{SwitchProdKeysPath: keys}

	const limit = 5
	seen := 0
	for _, e := range entries {
		if seen >= limit {
			break
		}
		if e.IsDir() || !strings.EqualFold(filepath.Ext(e.Name()), ".xci") {
			continue
		}
		path := filepath.Join(platDir, e.Name())
		r, err := Extract(path, PlatformSwitch, opts)
		if err != nil {
			t.Errorf("%s: %v", e.Name(), err)
			continue
		}
		if len(r.TitleID) != 16 || !strings.HasPrefix(r.TitleID, "01") {
			t.Errorf("%s: title_id=%q, want 16 hex starting 01", e.Name(), r.TitleID)
		}
		if r.Source != SourceBinary {
			t.Errorf("%s: source=%v, want binary", e.Name(), r.Source)
		}
		seen++
	}
	if seen == 0 {
		t.Skip("no Switch .xci samples")
	}
}
