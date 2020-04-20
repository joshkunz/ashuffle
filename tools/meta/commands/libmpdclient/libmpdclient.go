// Package libmpdclient provides a subcommand that is capable of installing
// and configuring libmpdclient in a target system.
package libmpdclient

import (
	"errors"
	"fmt"
	"log"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"github.com/urfave/cli/v2"

	"meta/exec"
	"meta/fetch"
	"meta/workspace"
)

const gitURL = "https://github.com/MusicPlayerDaemon/libmpdclient.git"

type version struct {
	major, minor int
}

func (v version) String() string {
	return fmt.Sprintf("%d.%d", v.major, v.minor)
}

func (v version) ReleaseURL() string {
	return fmt.Sprintf("https://www.musicpd.org/download/libmpdclient/%d/libmpdclient-%s.tar.xz", v.major, v)
}

var versionRe = regexp.MustCompile(`v(\d+)\.(\d+)`)

func parseVersion(v string) (version, error) {
	match := versionRe.FindStringSubmatch(v)
	if match == nil {
		return version{}, fmt.Errorf("%q does not match %q", v, versionRe)
	}
	if match[0] != v {
		return version{}, fmt.Errorf("did not fully match")
	}
	major, err := strconv.ParseInt(match[1], 10, 32)
	if err != nil {
		return version{}, fmt.Errorf("major part of %q invalid: %w", v, err)
	}
	minor, err := strconv.ParseInt(match[2], 10, 32)
	if err != nil {
		return version{}, fmt.Errorf("minor part of %q invalid: %w", v, err)
	}
	return version{int(major), int(minor)}, nil
}

func installAutomake(dest string) error {
	log.Printf("Configuring libmpdclient...")

	config := exec.Command("./configure", "--quiet", "--enable-silent-rules", "--prefix="+dest, "--disable-documentation")
	if err := config.Run(); err != nil {
		return fmt.Errorf("failed to configure: %w", err)
	}

	makeCmd := exec.Command("make", "-j", "16", "install")
	if err := makeCmd.Run(); err != nil {
		return fmt.Errorf("failed to build/install: %w", err)
	}

	return nil
}

func installMeson(dest string) error {
	meson := exec.Command("meson", ".", "build", "--prefix="+dest)
	if err := meson.Run(); err != nil {
		return fmt.Errorf("failed to configure: %w", err)
	}

	ninja := exec.Command("ninja", "-C", "build", "install")
	if err := ninja.Run(); err != nil {
		return fmt.Errorf("failed to build/install: %w", err)
	}
	return nil
}

func latestVersion() (version, error) {
	tags, err := fetch.GitVersions(gitURL)
	if err != nil {
		return version{}, fmt.Errorf("failed to fetch git versions: %w", err)
	}

	var versions []version
	for _, tag := range tags {
		parsed, err := parseVersion(tag)
		if err != nil {
			continue
		}
		versions = append(versions, parsed)
	}

	if len(versions) < 1 {
		return version{}, errors.New("found no git versions")
	}

	sort.Slice(versions, func(i, j int) bool {
		if versions[i].major == versions[j].major {
			return versions[i].minor < versions[j].minor
		}
		return versions[i].major < versions[j].major
	})

	return versions[len(versions)-1], nil
}

func install(ctx *cli.Context) error {
	ws, err := workspace.New()
	if err != nil {
		return fmt.Errorf("workspace: %v", err)
	}
	defer ws.Cleanup()

	var v version
	if sv := ctx.String("version"); sv == "latest" {
		log.Printf("version == latest, searching for latest version")
		vLatest, err := latestVersion()
		if err != nil {
			return err
		}
		v = vLatest
	} else {
		vParsed, err := parseVersion(sv)
		if err != nil {
			return err
		}
		v = vParsed
	}
	log.Printf("Using libmpdclient version %s", v)

	if v.major != 2 {
		return fmt.Errorf("unexpected major version in %s, only 2.x is supported", v)
	}

	if v.minor == 12 {
		return fmt.Errorf("version %s not supported", v)
	}

	if err := fetch.URL(v.ReleaseURL(), "libmpdclient.tar.xz"); err != nil {
		return err
	}

	tar := exec.Command("tar", "-xJ", "--strip-components=1", "-f", "libmpdclient.tar.xz")
	if err := tar.Run(); err != nil {
		return errors.New("failed to unpack")
	}

	if err := os.Chdir(ws.Root); err != nil {
		return fmt.Errorf("cd to workspace root: %w", err)
	}

	if v.minor < 12 {
		return installAutomake(ctx.String("prefix"))
	}
	return installMeson(ctx.String("prefix"))
}

var Command = &cli.Command{
	Name:  "libmpdclient",
	Usage: "Install libmpdclient.",
	Flags: []cli.Flag{
		&cli.StringFlag{
			Name:  "version",
			Value: "latest",
			Usage: strings.Join([]string{
				"version of libmpdclient to install, or 'latest' to",
				"automatically query for the latest released version, and",
				"install that.",
			}, " "),
		},
		&cli.StringFlag{
			Name:     "prefix",
			Value:    "",
			Usage:    "The root of the target installation path.",
			Required: true,
		},
	},
	Action: install,
}
