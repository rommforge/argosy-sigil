// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/rommforge/argosy-sigil/bindings/go"
)

func main() {
	platform := flag.String("platform", "auto", "platform slug or 'auto'")
	prodKeys := flag.String("prod-keys", "", "Switch prod.keys path")
	flag.Parse()

	if flag.NArg() != 1 {
		fmt.Fprintln(os.Stderr, "usage: example [--platform=<slug>] [--prod-keys=<path>] <rom>")
		os.Exit(2)
	}

	opts := &sigil.Options{SwitchProdKeysPath: *prodKeys}
	r, err := sigil.Extract(flag.Arg(0), sigil.PlatformFromSlug(*platform), opts)
	if err != nil {
		fmt.Fprintf(os.Stderr, "sigil: %v\n", err)
		os.Exit(1)
	}
	fmt.Printf("platform=%s title_id=%s raw_serial=%s usage=%s source=%s\n",
		r.Platform, r.TitleID, r.RawSerial, r.Usage, r.Source)
}
