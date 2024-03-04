// Package mpdver provides version resolvers, and a version definition for
// MPD.
package mpdver

import (
	"fmt"
	"log"

	"meta/fetch"
	"meta/semver"
)

// GitURL is the git repo URL for MPD
const GitURL = "https://github.com/MusicPlayerDaemon/MPD.git"

// Version is the type of an MPD version. It is based on semver with some
// additional printing changes.
type Version semver.Version

func (v Version) String() string {
	if v.Patch == 0 {
		return fmt.Sprintf("%d.%d", v.Major, v.Minor)
	}
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

// ReleaseURL is the release URL for this version.
func (v Version) ReleaseURL() string {
	return fmt.Sprintf("http://www.musicpd.org/download/mpd/%d.%d/mpd-%s.tar.xz", v.Major, v.Minor, v)
}

// Parse parses the given version.
func Parse(s string) (Version, error) {
	parsed, err := semver.Parse(s)
	if err != nil {
		return Version{}, err
	}
	return Version(parsed), nil
}

// Resolve resolves the latest version or parses the given given version.
func Resolve(s string) (Version, error) {
	if s == "latest" {
		log.Printf("version == latest, looking up latest version")
		latest, err := fetch.GitLatest(GitURL)
		if err != nil {
			return Version{}, err
		}
		return Version(latest), nil
	}

	return Parse(s)
}
