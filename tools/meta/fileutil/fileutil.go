// Package fileutil provides utilities for working with files.
package fileutil

import (
	"meta/exec"
)

// Copy the src file to the destination.
func Copy(src, dest string) error {
	// There's probably a better way to do this, but we know that cp will
	// handle permission and mode bits correctly. So just use that.
	return exec.Command("cp", src, dest).Run()
}
