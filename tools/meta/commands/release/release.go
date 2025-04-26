package release

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/urfave/cli/v3"

	"meta/crosstool"
	"meta/fileutil"
	"meta/project"
	"meta/workspace"
)

func crossFile(crosstool *crosstool.Crosstool) (string, error) {
	cf, err := os.CreateTemp("", "cross-"+crosstool.CPU.Triple().Architecture+"-*.txt")
	if err != nil {
		return "", err
	}

	if err := crosstool.WriteCrossFile(cf); err != nil {
		cf.Close()
		os.Remove(cf.Name())
		return "", err
	}

	cf.Close()
	return cf.Name(), nil
}

func releaseCross(ctx context.Context, cmd *cli.Command, out string, cpu crosstool.CPU) error {
	src, err := os.Getwd()
	if err != nil {
		return err
	}

	crosstool, err := crosstool.For(cpu, crosstool.Options{
		CC:  cmd.String("cross_cc"),
		CXX: cmd.String("cross_cxx"),
	})
	if err != nil {
		return err
	}
	defer crosstool.Cleanup()

	crossF, err := crossFile(crosstool)
	if err != nil {
		return err
	}
	defer os.Remove(crossF)

	libmpdclientArgs := []string{
		"meta", "install", "libmpdclient",
		fmt.Sprintf("--cross_file=%s", crossF),
		// Install into the crosstool root, so that our `--sysroot` works
		// when building ashuffle.
		fmt.Sprintf("--prefix=%s", crosstool.Root),
	}
	if ver := cmd.String("libmpdclient_version"); ver != "" {
		flag := fmt.Sprintf("--version=%s", ver)
		libmpdclientArgs = append(libmpdclientArgs, flag)
	}
	// XXX: In v3, this results in an infinite loop, likely because "release"
	// becomes its own parent. Not exactly sure what context is _supposed_
	// to be used here.
	if err := cmd.Root().Run(context.TODO(), libmpdclientArgs); err != nil {
		return fmt.Errorf("failed to build libmpdclient: %w", err)
	}

	build, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer build.Cleanup()

	p, err := project.NewMeson(src, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: build.Root,
		Extra:          []string{"--cross-file", crossF},
	})
	if err != nil {
		return err
	}

	if err := p.Configure(""); err != nil {
		return err
	}

	if err := p.Build("ashuffle"); err != nil {
		return fmt.Errorf("failed to build ashuffle: %w", err)
	}

	if err := fileutil.RemoveRPath(build.Path("ashuffle")); err != nil {
		return fmt.Errorf("failed to remove rpath: %w", err)
	}

	return fileutil.Copy(build.Path("ashuffle"), out)
}

func releasex86(out string) error {
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

	if err := fileutil.RemoveRPath(build.Path("ashuffle")); err != nil {
		return fmt.Errorf("failed to remove rpath: %w", err)
	}

	return fileutil.Copy(build.Path("ashuffle"), out)
}

func release(ctx context.Context, cmd *cli.Command) error {
	if !cmd.Args().Present() {
		return errors.New("an architecture (`ARCH`) must be provided")
	}

	out := cmd.String("output")
	if out == "" {
		o, err := filepath.Abs("./ashuffle")
		if err != nil {
			return err
		}
		out = o
	}

	arch := cmd.Args().First()
	switch arch {
	case "x86_64":
		return releasex86(out)
	case "aarch64":
		// Processors used on 3B+ support this arch, but RPi OS does not.
		// These are probably OK defaults for aarch64 though.
		return releaseCross(ctx, cmd, out, crosstool.CortexA53)
	case "armv7h":
		// Used on Raspberry Pi 2B+. Should also work for newer
		// chips running 32-bit RPi OS.
		return releaseCross(ctx, cmd, out, crosstool.CortexA7)
	case "armv6h":
		// Used on Raspberry Pi 0/1.
		return releaseCross(ctx, cmd, out, crosstool.ARM1176JZF_S)
	}

	return fmt.Errorf("architecture %q not supported", cmd.Args().First())
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
				"present or not) will be used. Currently only supported by",
				"AArch64 release.",
			}, " "),
		},
		&cli.StringFlag{
			Name:  "cross_cc",
			Value: "clang",
			Usage: strings.Join([]string{
				"Name of the C compiler driver to use during cross compilation.",
				"Defaults to 'clang'. The driver must support the `--target`",
				"option.",
			}, " "),
		},
		&cli.StringFlag{
			Name:  "cross_cxx",
			Value: "clang++",
			Usage: strings.Join([]string{
				"Name of the C++ compiler driver to use during cross",
				"compilation. Defaults to 'clang'. The driver must support the",
				"`--target` option.",
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
