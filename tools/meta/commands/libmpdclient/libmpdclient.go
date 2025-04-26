// Package libmpdclient provides a subcommand that is capable of installing
// and configuring libmpdclient in a target system.
package libmpdclient

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/urfave/cli/v3"

	"meta/exec"
	"meta/fetch"
	"meta/project"
	"meta/versions/libmpdclientver"
	"meta/workspace"
)

func install(ctx context.Context, cmd *cli.Command) error {
	ws, err := workspace.New()
	if err != nil {
		return fmt.Errorf("workspace: %v", err)
	}
	defer ws.Cleanup()

	v, err := libmpdclientver.Resolve(cmd.String("version"))
	if err != nil {
		return err
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
		if cmd.String("cross_file") != "" {
			return errors.New("cross compilation via --cross_file not supported with this version of libmpdclient")
		}
		p, err := project.NewAutomake(ws.Root)
		if err != nil {
			return err
		}
		proj = p
	} else {
		var opts project.MesonOptions
		if cf := cmd.String("cross_file"); cf != "" {
			opts.Extra = append(opts.Extra, "--cross-file", cf)
		}
		p, err := project.NewMeson(ws.Root, opts)
		if err != nil {
			return err
		}
		proj = p
	}
	return project.Install(proj, cmd.String("prefix"))
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
