// Package semver provides utilities for working with semver-like versions.
package semver

import (
	"fmt"
	"regexp"
	"strconv"
)

// Version is the type of "semver" versions.
type Version struct {
	Major, Minor, Patch int
}

// String implements fmt.Stringer for Version.
func (v Version) String() string {
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

// Version should be a stringer.
var _ fmt.Stringer = (*Version)(nil)

var versionRe = regexp.MustCompile(`v?(\d+)(\.(\d+))?(\.(\d+))?`)

// Parse parses the given version string into a Semver version if possible.
// It allows some components to be missing (interpreted as zeros), and it
// allows a leading `v` to be present in the version string.
func Parse(s string) (Version, error) {
	match := versionRe.FindStringSubmatch(s)
	if match == nil {
		return Version{}, fmt.Errorf("%q did not match %q", s, versionRe)
	}
	var nums []int
	for _, part := range []string{match[1], match[3], match[5]} {
		if part == "" {
			nums = append(nums, 0)
			continue
		}
		num, err := strconv.ParseInt(part, 10, 32)
		if err != nil {
			return Version{}, fmt.Errorf("failed to parse %q in %q as int: %v", s, part, err)
		}
		nums = append(nums, int(num))
	}
	return Version{nums[0], nums[1], nums[2]}, nil
}

// Less returns true if version a is a lower version than version b.
func Less(a, b Version) bool {
	switch {
	case a.Major != b.Major:
		return a.Major < b.Major
	case a.Minor != b.Minor:
		return a.Minor < b.Minor
	case a.Patch != b.Patch:
		return a.Patch < b.Patch
	}
	// If we got this far, they are equal.
	return false
}
