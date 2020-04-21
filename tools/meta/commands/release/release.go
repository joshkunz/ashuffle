package release

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/urfave/cli/v2"

	"meta/fileutil"
	"meta/project"
)

func releasex86(out string) error {
	cwd, err := os.Getwd()
	if err != nil {
		return err
	}
	p, err := project.NewMeson(cwd, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: "build",
	})
	if err != nil {
		return err
	}

	if err := p.Configure(""); err != nil {
		return err
	}

	if err := p.Build("ashuffle"); err != nil {
		return err
	}

	return fileutil.Copy("build/ashuffle", out)
}

func release(ctx *cli.Context) error {
	if !ctx.Args().Present() {
		return errors.New("an architecture (`ARCH`) must be provided")
	}

	out := ctx.String("output")
	if out == "" {
		o, err := filepath.Abs("./ashuffle")
		if err != nil {
			return err
		}
		out = o
	}

	switch ctx.Args().First() {
	case "x86_64":
		return releasex86(out)
	}

	return fmt.Errorf("architecture %q not supported", ctx.Args().First())
}

var Command = &cli.Command{
	Name:  "release",
	Usage: "Build release binaries for ashuffle for `ARCH`.",
	Flags: []cli.Flag{
		&cli.StringFlag{
			Name:  "libmpdclient_version",
			Value: "",
			Usage: strings.Join([]string{
				"Version of libmpdclient to build against, or 'latest' to",
				"automatically query for the latest released version, and",
				"build against that. If unset, the system version (whether",
				"present or not) will be used.",
			}, " "),
		},
		&cli.StringFlag{
			Name:    "output",
			Aliases: []string{"o"},
			Value:   "",
			Usage:   "If set, the built binary will be written to this location.",
		},
	},
	Action: release,
}
