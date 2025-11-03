// Package libmpdclientver provides a version type for libmpdclient, and a
// resolver for looking up the latest version.
package libmpdclientver

import (
	"fmt"
	"log"

	"meta/fetch"
	"meta/semver"
)

// GitURL is the URL of the libmpdclient git project.
const GitURL = "https://github.com/MusicPlayerDaemon/libmpdclient.git"

// Version is the type of a libmpdclient version.
type Version semver.Version

func (v Version) String() string {
	return fmt.Sprintf("%d.%d", v.Major, v.Minor)
}

// ReleaseURL returns the release URL for this version of libmpdclient.
func (v Version) ReleaseURL() string {
	return fmt.Sprintf("https://github.com/MusicPlayerDaemon/libmpdclient/archive/refs/tags/v%s.tar.gz", v)
}

// Parse parses a given version string into a version.
func Parse(v string) (Version, error) {
	parsed, err := semver.Parse(v)
	if err != nil {
		return Version{}, err
	}
	return Version(parsed), nil
}

// Resolve either looks up, or parses the given version.
func Resolve(v string) (Version, error) {
	if v == "latest" {
		log.Printf("version == latest, searching for latest version")
		latest, err := fetch.GitLatest(GitURL)
		if err != nil {
			return Version{}, err
		}
		return Version(latest), err
	}
	return Parse(v)
}
