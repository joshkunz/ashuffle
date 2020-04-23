package release

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"text/template"

	"github.com/urfave/cli/v2"

	"meta/exec"
	"meta/fileutil"
	"meta/project"
	"meta/workspace"
)

const aarch64Crosstool = "scripts/cross/gcc-arm.tar.xz"

var aarch64Crossfile = template.Must(
	template.New("aarch64-crossfile").
		Parse(strings.Join([]string{
			"[binaries]",
			"c = '{{ .Sysroot }}/bin/aarch64-none-linux-gnu-gcc'",
			// We have to set /usr/bin/pkg-config explicitly here, or meson won't
			// use pkg-config to find libmpdclient.
			"pkgconfig = '/usr/bin/pkg-config'",
			"",
			"[properties]",
			"sys_root = '{{ .Sysroot }}'",
			"c_args = '-I{{ .Sysroot }}/include'",
			"c_link_args = '-L{{ .Sysroot }}/lib'",
			"",
			"[host_machine]",
			"system = 'linux'",
			"cpu_family = 'aarch64'",
			"cpu = 'cortex-a53'",
			"endian = 'little'",
		}, "\n")),
)

func releaseAArch64(ctx *cli.Context, out string) error {
	src, err := os.Getwd()
	if err != nil {
		return err
	}
	crosstoolAr := filepath.Join(src, "scripts/cross/gcc-arm.tar.xz")

	crosstool, err := workspace.New()
	if err != nil {
		return err
	}
	defer crosstool.Cleanup()

	untar := exec.Command("tar", "--strip-components=1", "-xJf", crosstoolAr)
	if err := untar.Run(); err != nil {
		return fmt.Errorf("failed to unpack crosstool: %w", err)
	}

	if err := os.Chdir(src); err != nil {
		return err
	}

	crossF, err := ioutil.TempFile("", "cross-aarch64-*.txt")
	if err != nil {
		return err
	}
	defer os.Remove(crossF.Name())

	sysroot := struct {
		Sysroot string
	}{
		Sysroot: crosstool.Root,
	}

	if err := aarch64Crossfile.Execute(crossF, sysroot); err != nil {
		fmt.Errorf("failed to write crossfile: %w", err)
	}

	libmpdclientArgs := []string{
		"meta", "install", "libmpdclient",
		fmt.Sprintf("--cross_file=%s", crossF.Name()),
		fmt.Sprintf("--prefix=%s", crosstool.Root),
	}
	if ver := ctx.String("libmpdclient_version"); ver != "" {
		flag := fmt.Sprintf("--version=%s", ver)
		libmpdclientArgs = append(libmpdclientArgs, flag)
	}
	if err := ctx.App.Run(libmpdclientArgs); err != nil {
		fmt.Errorf("failed to build libmpdclient: %w", err)
	}

	p, err := project.NewMeson(src, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: "build",
		Extra:          []string{"--cross-file", crossF.Name()},
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

	return fileutil.Copy("build/ashuffle", out)
}

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
	case "aarch64":
		return releaseAArch64(ctx, out)
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
				"present or not) will be used. Currently only supported by",
				"AArch64 release.",
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
