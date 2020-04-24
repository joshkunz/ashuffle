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
	"meta/fetch"
	"meta/fileutil"
	"meta/project"
	"meta/workspace"
)

type remoteFile struct {
	URL    string
	SHA256 string
}

// Obtained from the offical ARM crosstool release:
//  https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads
// Current Version: 9.2-2019.12
var aarch64Crosstool = remoteFile{
	URL:    "https://storage.googleapis.com/ashuffle-data/gcc-arm.tar.xz",
	SHA256: "8dfe681531f0bd04fb9c53cf3c0a3368c616aa85d48938eebe2b516376e06a66",
}

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

	crosstool, err := workspace.New()
	if err != nil {
		return err
	}
	defer crosstool.Cleanup()

	if err := fetch.URL(aarch64Crosstool.URL, crosstool.Path("archive.tar.xz")); err != nil {
		return err
	}

	if err := fileutil.Verify(crosstool.Path("archive.tar.xz"), aarch64Crosstool.SHA256); err != nil {
		return fmt.Errorf("fetched crosstool failed to verify: %w", err)
	}

	untar := exec.Command("tar", "--strip-components=1", "-xJf", crosstool.Path("archive.tar.xz"))
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

	build, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer build.Cleanup()

	p, err := project.NewMeson(src, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: build.Root,
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

	return fileutil.Copy(build.Path("ashuffle"), out)
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
