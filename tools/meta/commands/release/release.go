package release

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	osexec "os/exec"
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

var (
	// Both crosstools obtained from the offical ARM crosstool release:
	//	https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads
	// Current Version: 9.2-2019.12
	aarch64CrosstoolArchive = remoteArchive{
		URL:    "https://storage.googleapis.com/ashuffle-data/gcc-arm.tar.xz",
		SHA256: "8dfe681531f0bd04fb9c53cf3c0a3368c616aa85d48938eebe2b516376e06a66",
	}
	armCrosstoolArchive = remoteArchive{
		URL:    "https://storage.googleapis.com/ashuffle-data/gcc-arm32.tar.xz",
		SHA256: "51bbaf22a4d3e7a393264c4ef1e45566701c516274dde19c4892c911caa85617",
	}

	aarch64Triple = triple{
		Architecture: "aarch64",
		Vendor:       "none",
		System:       "linux",
		ABI:          "gnu",
	}

	armTriple = triple{
		Architecture: "arm",
		Vendor:       "none",
		System:       "linux",
		ABI:          "gnueabihf",
	}
)

type remoteArchive struct {
	URL    string
	SHA256 string
}

func (r remoteArchive) FetchTo(dest string) error {
	d, err := filepath.Abs(dest)
	if err != nil {
		return err
	}

	ws, err := workspace.New()
	if err != nil {
		return err
	}
	defer ws.Cleanup()

	if err := fetch.URL(r.URL, ws.Path("archive.tar.xz")); err != nil {
		return err
	}

	if err := fileutil.Verify(ws.Path("archive.tar.xz"), r.SHA256); err != nil {
		return fmt.Errorf("failed to verify: %w", err)
	}

	untar := exec.Command("tar", "--strip-components=1", "-C", d, "-xJf", ws.Path("archive.tar.xz"))
	if err := untar.Run(); err != nil {
		return fmt.Errorf("failed to unpack: %w", err)
	}

	return nil
}

type triple struct {
	Architecture string
	Vendor       string
	System       string
	ABI          string
}

func (t triple) String() string {
	return strings.Join([]string{
		t.Architecture,
		t.Vendor,
		t.System,
		t.ABI,
	}, "-")
}

type armCrosstool struct {
	PkgConfig string
	CMake     string
	Triple    triple

	*workspace.Workspace
}

func newARMCrosstool(triple triple) (*armCrosstool, error) {
	pkgConfig, err := osexec.LookPath("pkg-config")
	if err != nil {
		return nil, fmt.Errorf("unable to find pkg-config: %w", err)
	}
	cmake, err := osexec.LookPath("cmake")
	if err != nil {
		return nil, fmt.Errorf("unable to find cmake: %w", err)
	}

	var archive remoteArchive
	switch triple.Architecture {
	case "aarch64":
		archive = aarch64CrosstoolArchive
	case "arm":
		archive = armCrosstoolArchive
	default:
		return nil, fmt.Errorf("unrecognized architecture %q in triple: %s", triple.Architecture, triple)
	}

	ws, err := workspace.New(workspace.NoCD)
	if err != nil {
		return nil, err
	}

	if err := archive.FetchTo(ws.Root); err != nil {
		ws.Cleanup()
		return nil, err
	}

	return &armCrosstool{
		PkgConfig: pkgConfig,
		CMake:     cmake,
		Triple:    triple,
		Workspace: ws,
	}, nil
}

var armCrossfileTmpl = template.Must(
	template.New("arm-crossfile").
		Funcs(map[string]interface{}{
			"joinBy": func(delim string, strs []string) string {
				return strings.Join(strs, delim)
			},
		}).
		Parse(strings.Join([]string{
			"[binaries]",
			"c = '{{ .Crosstool.Root }}/bin/{{ .Crosstool.Triple }}-gcc'",
			"cpp = '{{ .Crosstool.Root }}/bin/{{ .Crosstool.Triple }}-g++'",
			"c_ld = 'gold'",
			"cpp_ld = 'gold'",
			// We have to set pkgconfig and cmake explicitly here, otherwise
			// meson will not be able to find them. We just re-use the
			// system versions, since we don't need any special arch-specific
			// handing.
			"pkgconfig = '{{ .Crosstool.PkgConfig }}'",
			"cmake = '{{ .Crosstool.CMake }}'",
			"",
			"[properties]",
			"sys_root = '{{ .Crosstool.Root }}'",
			"c_args = '-mcpu={{ .CPU }} -I{{ .Crosstool.Root }}/include {{ .CFlags | joinBy \" \" }}'",
			"c_link_args = '-L{{ .Crosstool.Root }}/lib'",
			"cpp_args = '-mcpu={{ .CPU }} -I{{ .Crosstool.Root }}/include {{ .CFlags | joinBy \" \" }}'",
			"cpp_link_args = '-L{{ .Crosstool.Root }}/lib'",
			"",
			"[host_machine]",
			"system = '{{ .Crosstool.Triple.System }}'",
			"cpu_family = '{{ .Crosstool.Triple.Architecture }}'",
			"cpu = '{{ .CPU }}'",
			"endian = 'little'",
		}, "\n")),
)

func crossFile(crosstool *armCrosstool, cpu string) (string, error) {

	cf, err := ioutil.TempFile("", "cross-"+crosstool.Triple.Architecture+"-*.txt")
	if err != nil {
		return "", err
	}

	info := struct {
		Crosstool *armCrosstool
		CPU       string
		// Flags added to all compiler invocations (C or CXX).
		CFlags []string
	}{
		Crosstool: crosstool,
		CPU:       cpu,
	}

	if crosstool.Triple.Architecture == "arm" {
		// When building for arm32, GCC defaults to THUMB output for
		// some reason. Force it to output ARM instead.
		info.CFlags = append(info.CFlags, "-marm")
	}

	if err := armCrossfileTmpl.Execute(cf, info); err != nil {
		cf.Close()
		os.Remove(cf.Name())
		return "", err
	}

	cf.Close()
	return cf.Name(), nil
}

func releaseARM(ctx *cli.Context, out string, triple triple, cpu string) error {
	src, err := os.Getwd()
	if err != nil {
		return err
	}

	crosstool, err := newARMCrosstool(triple)
	if err != nil {
		return err
	}
	defer crosstool.Cleanup()

	crossF, err := crossFile(crosstool, cpu)
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

	arch := ctx.Args().First()
	switch arch {
	case "x86_64":
		return releasex86(out)
	case "aarch64":
		// Processors used on 3B+ support this arch, but RPi OS does not.
		// These are probably OK defaults for aarch64 though.
		return releaseARM(ctx, out, aarch64Triple, "cortex-a53")
	case "armv7h":
		// Used on Raspberry Pi 2B+. Should also work for newer
		// chips running 32-bit RPi OS.
		return releaseARM(ctx, out, armTriple, "cortex-a7")
	case "armv6h":
		// Used on Raspberry Pi 0/1.
		return releaseARM(ctx, out, armTriple, "arm1176jzf-s")
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
