// Package libmpdclient provides a subcommand that is capable of installing
// and configuring libmpdclient in a target system.
package libmpdclient

import (
	"errors"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/urfave/cli/v2"

	"meta/exec"
	"meta/fetch"
	"meta/project"
	"meta/semver"
	"meta/workspace"
)

const gitURL = "https://github.com/MusicPlayerDaemon/libmpdclient.git"

type version semver.Version

func (v version) String() string {
	return fmt.Sprintf("%d.%d", v.Major, v.Minor)
}

func (v version) ReleaseURL() string {
	return fmt.Sprintf("https://www.musicpd.org/download/libmpdclient/%d/libmpdclient-%s.tar.xz", v.Major, v)
}

func parseVersion(v string) (version, error) {
	parsed, err := semver.Parse(v)
	if err != nil {
		return version(semver.Version{}), err
	}
	return version(parsed), nil
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
		latest, err := fetch.GitLatest(gitURL)
		if err != nil {
			return err
		}
		v = version(latest)
	} else {
		parsed, err := parseVersion(sv)
		if err != nil {
			return err
		}
		v = parsed
	}
	log.Printf("Using libmpdclient version %s", v)

	if v.Major != 2 {
		return fmt.Errorf("unexpected major version in %s, only 2.x is supported", v)
	}

	if v.Minor == 12 {
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

	var proj project.Project
	if v.Minor < 12 {
		if ctx.String("cross_file") != "" {
			return errors.New("cross compilation via --cross_file not supported with this version of libmpdclient")
		}
		p, err := project.NewAutomake(ws.Root)
		if err != nil {
			return err
		}
		proj = p
	} else {
		var opts project.MesonOptions
		if cf := ctx.String("cross_file"); cf != "" {
			opts.Extra = append(opts.Extra, "--cross-file", cf)
		}
		p, err := project.NewMeson(ws.Root, opts)
		if err != nil {
			return err
		}
		proj = p
	}
	return project.Install(proj, ctx.String("prefix"))
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
		&cli.StringFlag{
			Name:  "cross_file",
			Value: "",
			Usage: "The Meson 'cross-file' to use when cross-compiling",
		},
	},
	Action: install,
}
