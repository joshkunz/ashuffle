// Package fileutil provides utilities for working with files.
package fileutil

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"log"
	"os"

	"meta/exec"
)

// Copy the src file to the destination.
func Copy(src, dest string) error {
	// There's probably a better way to do this, but we know that cp will
	// handle permission and mode bits correctly. So just use that.
	return exec.Command("cp", src, dest).Run()
}

// Verify the given file has the given sha256 hashsum.
func Verify(file, want string) error {
	log.Printf("VERIFY %q (%s)", file, want)

	f, err := os.Open(file)
	if err != nil {
		return err
	}
	defer f.Close()

	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return err
	}

	hs := hex.EncodeToString(h.Sum(nil))
	if hs != want {
		return fmt.Errorf("hashes do not match got %q, but wanted %q", hs, want)
	}

	log.Printf("VERIFY OK %q ", file)
	return nil
}

// RemoveRPath removes the DT_RPATH entry from the given ELF file using
// `patchelf` from the build system.
func RemoveRPath(file string) error {
	return exec.Command("patchelf", "--remove-rpath", file).Run()
}
