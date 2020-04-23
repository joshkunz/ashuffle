// Package fileutil provides utilities for working with files.
package fileutil

import (
	"os"
)

// Copy the src file to the destination.
func Copy(src, dest string) error {
	// Optimistically link.
	return os.Link(src, dest)
	// TODO(jkz): Revist this, if we need additional features.
}
