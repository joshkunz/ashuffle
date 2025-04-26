package testbuild

import (
	"context"
	"os"
	"path/filepath"

	"github.com/urfave/cli/v3"

	"meta/fileutil"
	"meta/project"
	"meta/workspace"
)

func testbuild(ctx context.Context, cmd *cli.Command) error {
	out := cmd.String("output")
	if out == "" {
		o, err := filepath.Abs("./ashuffle")
		if err != nil {
			return err
		}
		out = o
	}

	cwd, err := os.Getwd()
	if err != nil {
		return err
	}

	build, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer build.Cleanup()

	p, err := project.NewMeson(cwd, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: build.Root,
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

	return fileutil.Copy(build.Path("ashuffle"), out)
}

var Command = &cli.Command{
	Name:  "testbuild",
	Usage: "Build ashuffle for integration tests.",
	Flags: []cli.Flag{
		&cli.StringFlag{
			Name:    "output",
			Aliases: []string{"o"},
			Value:   "",
			Usage:   "If set, the built binary will be written to this location.",
		},
	},
	Action: testbuild,
}
