// Package crosstool contains routiles for constructing cross-compilation
// environments for various (ARM) CPUs.
package crosstool

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"strings"
	"text/template"

	"meta/fetch"
	"meta/project"
	"meta/workspace"
)

var (
	raspbianRoot = fetch.RemoteArchive{
		URL:    "http://downloads.raspberrypi.org/raspbian_lite/archive/2019-04-09-22:48/root.tar.xz",
		SHA256: "64af252aed817429e760cd3aa10f8b54713e678828f65fca8a1a76afe495ac61",
		Format: fetch.TarXz,
		ExtraOptions: []string{
			"--exclude=./dev/*",
		},
	}

	ubuntuAArch64Root = fetch.RemoteArchive{
		URL:    "https://storage.googleapis.com/ashuffle-data/ubuntu_16.04_aarch64_root.tar.xz",
		SHA256: "441d94b8e8ab42bf31bf98a04c87dd1de3e84586090d200d4bb4974960385605",
		Format: fetch.TarXz,
		ExtraOptions: []string{
			"--strip-components=1",
		},
	}

	llvmSource = fetch.RemoteArchive{
		URL:    "https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/llvm-project-13.0.0.src.tar.xz",
		SHA256: "6075ad30f1ac0e15f07c1bf062c1e1268c241d674f11bd32cdf0e040c71f2bf3",
		Format: fetch.TarXz,
		ExtraOptions: []string{
			"--strip-components=1",
		},
	}
)

// Triple represents a target triple for a particular platform.
type Triple struct {
	Architecture string
	Vendor       string
	System       string
	ABI          string
}

// String implements fmt.Stringer for Triple.
func (t Triple) String() string {
	return strings.Join([]string{
		t.Architecture,
		t.Vendor,
		t.System,
		t.ABI,
	}, "-")
}

// CPU represents a CPU for which we can build a crosstool.
type CPU string

const (
	CortexA53    CPU = "cortex-a53"
	CortexA7     CPU = "cortex-a7"
	ARM1176JZF_S CPU = "arm1176jzf-s"
)

// Triple returns the triple for the CPU.
func (c CPU) Triple() Triple {
	switch c {
	case CortexA53:
		return Triple{
			Architecture: "aarch64",
			Vendor:       "none",
			System:       "linux",
			ABI:          "gnu",
		}
	case CortexA7, ARM1176JZF_S:
		return Triple{
			Architecture: "arm",
			Vendor:       "none",
			System:       "linux",
			ABI:          "gnueabihf",
		}
	}
	panic("unreachable")
}

// String implements fmt.Stringer for this CPU.
func (c CPU) String() string {
	return string(c)
}

// Options which control how the crosstool is built.
type Options struct {
	// CC and CXX are the host-targeted compiler binaries used by the
	// crosstool. If unset, then "clang", and "clang++" are used.
	CC, CXX string
}

var crossfileTmpl = template.Must(
	template.New("arm-crossfile").
		Funcs(map[string]interface{}{
			"joinSpace": func(strs []string) string {
				return strings.Join(strs, " ")
			},
		}).
		Parse(strings.Join([]string{
			"[binaries]",
			"c = '{{ .CC }}'",
			"cpp = '{{ .CXX }}'",
			"c_ld = 'lld'",
			"cpp_ld = 'lld'",
			// We have to set pkgconfig and cmake explicitly here, otherwise
			// meson will not be able to find them. We just re-use the
			// system versions, since we don't need any special arch-specific
			// handing.
			"pkgconfig = '{{ .PkgConfig }}'",
			"cmake = '{{ .CMake }}'",
			"",
			"[properties]",
			"sys_root = '{{ .Root }}'",
			"",
			"[cmake]",
			"CMAKE_C_COMPILER = '{{ .CC }}'",
			"CMAKE_C_FLAGS = '-mcpu={{ .CPU }} {{ .CFlags | joinSpace }}'",
			"CMAKE_CXX_COMPILER = '{{ .CC }}'",
			"CMAKE_CXX_FLAGS = '-mcpu={{ .CPU }} {{ .CXXFlags | joinSpace }}'",
			"CMAKE_EXE_LINKER_FLAGS = '-fuse-ld=lld'",
			"",
			"[built-in options]",
			"c_args = '-mcpu={{ .CPU }} {{ .CFlags | joinSpace }}'",
			"c_link_args = '{{ .LDFlags | joinSpace }}'",
			"cpp_args = '-mcpu={{ .CPU }} {{ .CXXFlags | joinSpace }}'",
			"cpp_link_args = '{{ .CXXLDFlags | joinSpace }}'",
			"",
			"[host_machine]",
			"system = '{{ .CPU.Triple.System }}'",
			"cpu_family = '{{ .CPU.Triple.Architecture }}'",
			"cpu = '{{ .CPU }}'",
			"endian = 'little'",
		}, "\n")),
)

// Crosstool represents a cross-compiler environment. These can be created via
// the `For` function for a specific CPU.
type Crosstool struct {
	*workspace.Workspace
	libCXX *workspace.Workspace

	CPU        CPU
	PkgConfig  string
	CMake      string
	CC         string
	CXX        string
	CFlags     []string
	CXXFlags   []string
	LDFlags    []string
	CXXLDFlags []string
}

// WriteCrossFile writes a Meson cross file for this crosstool to the given
// io.Writer.
func (c *Crosstool) WriteCrossFile(w io.Writer) error {
	return crossfileTmpl.Execute(w, c)
}

// Cleanup cleans up this crosstool, removing any downloaded or built artifacts.
func (c *Crosstool) Cleanup() error {
	lErr := c.libCXX.Cleanup()
	if err := c.Workspace.Cleanup(); err != nil {
		if lErr != nil {
			return fmt.Errorf("multiple cleanup errors: %v, %v", lErr, err)
		}
		return err
	}
	return lErr
}

func installLibCXX(cpu CPU, sysroot string, opts Options, into *workspace.Workspace) error {
	flags := []string{
		"--target=" + cpu.Triple().String(),
		"-mcpu=" + cpu.String(),
		"-fuse-ld=lld",
		"--sysroot=" + sysroot,
	}

	if cpu == CortexA7 || cpu == ARM1176JZF_S {
		flags = append(flags, "-marm")
	}

	base := project.CMakeOptions{
		CCompiler:   opts.CC,
		CXXCompiler: opts.CXX,
		CFlags:      flags,
		CXXFlags:    flags,
		Extra: project.CMakeVariables{
			"CMAKE_CROSSCOMPILING":  "YES",
			"LLVM_TARGETS_TO_BUILD": "ARM",
		},
	}

	src, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer src.Cleanup()

	if err := llvmSource.FetchTo(src.Root); err != nil {
		return err
	}

	libBuild, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer libBuild.Cleanup()

	lib := base
	lib.BuildDirectory = libBuild.Root
	lib.Extra["LIBCXX_CXX_ABI"] = "libcxxabi"
	lib.Extra["LIBCXX_ENABLE_SHARED"] = "NO"
	lib.Extra["LIBCXX_STANDALONE_BUILD"] = "YES"
	lib.Extra["LIBCXX_CXX_ABI_INCLUDE_PATHS"] = path.Join(src.Root, "libcxxabi/include")

	libProject, err := project.NewCMake(path.Join(src.Root, "libcxx"), lib)
	if err != nil {
		return err
	}

	if err := project.Install(libProject, into.Root); err != nil {
		return err
	}

	abiBuild, err := workspace.New(workspace.NoCD)
	if err != nil {
		return err
	}
	defer abiBuild.Cleanup()

	abi := base
	abi.BuildDirectory = abiBuild.Root
	abi.Extra["LIBCXXABI_ENABLE_SHARED"] = "NO"
	// Since we're building standalone, we need to manually set-up the libcxx
	// includes to the ones we just installed.
	abi.Extra["LIBCXXABI_LIBCXX_INCLUDES"] = path.Join(into.Root, "include/c++/v1")

	abiProject, err := project.NewCMake(path.Join(src.Root, "libcxxabi"), abi)
	if err != nil {
		return err
	}

	return project.Install(abiProject, into.Root)
}

func fixupRaspbian(sys *workspace.Workspace) error {
	// The rapbian root we're using has an absolute symlink to libm.so. This
	// causes issues when we use libm, because lld will try to link against
	// libm.a instead which won't work, because we're doing a static build.
	// So, we need to fix the symlink.

	// First delete the old symlink.
	if err := os.Remove(sys.Path("usr/lib/arm-linux-gnueabihf/libm.so")); err != nil {
		return err
	}

	// Then setup the correct symlink.
	return os.Symlink(
		sys.Path("lib/arm-linux-gnueabihf/libm.so.6"),
		sys.Path("usr/lib/arm-linux-gnueabihf/libm.so"),
	)
}

func fetchSysroot(cpu CPU) (*workspace.Workspace, error) {
	sys, err := workspace.New(workspace.NoCD)
	if err != nil {
		return nil, err
	}

	var fixup func(*workspace.Workspace) error

	var root fetch.RemoteArchive
	switch cpu {
	case CortexA53:
		root = ubuntuAArch64Root
	case CortexA7, ARM1176JZF_S:
		root = raspbianRoot
		fixup = fixupRaspbian
	}

	if err := root.FetchTo(sys.Root); err != nil {
		sys.Cleanup()
		return nil, err
	}

	if fixup != nil {
		if err := fixup(sys); err != nil {
			return nil, err
		}
	}

	return sys, nil
}

// For creates a new crosstool for the given CPU, using the given options.
func For(cpu CPU, opts Options) (*Crosstool, error) {
	if opts.CC == "" {
		opts.CC = "clang"
	}
	if opts.CXX == "" {
		opts.CXX = "clang++"
	}
	wantBins := []string{
		"pkg-config",
		"cmake",
		opts.CC,
		opts.CXX,
	}

	binPaths := make(map[string]string)
	for _, bin := range wantBins {
		path, err := exec.LookPath(bin)
		if err != nil {
			return nil, err
		}
		binPaths[bin] = path
	}

	sys, err := fetchSysroot(cpu)
	if err != nil {
		return nil, err
	}

	libcxx, err := workspace.New(workspace.NoCD)
	if err != nil {
		sys.Cleanup()
		return nil, err
	}

	if err := installLibCXX(cpu, sys.Root, opts, libcxx); err != nil {
		sys.Cleanup()
		libcxx.Cleanup()
		return nil, err
	}

	commonFlags := []string{
		"--sysroot=" + sys.Root,
		"--target=" + cpu.Triple().String(),
	}

	ct := &Crosstool{
		Workspace: sys,
		libCXX:    libcxx,
		CPU:       cpu,
		PkgConfig: binPaths["pkg-config"],
		CMake:     binPaths["cmake"],
		CC:        binPaths[opts.CC],
		CXX:       binPaths[opts.CXX],
		CFlags:    commonFlags,
		CXXFlags: append(commonFlags,
			"-nostdinc++",
			"-I"+path.Join(libcxx.Root, "include/c++/v1"),
		),
		LDFlags: append(commonFlags,
			"-fuse-ld=lld",
			"-lpthread",
		),
		CXXLDFlags: append(commonFlags,
			"-fuse-ld=lld",
			"-nostdlib++",
			"-L"+path.Join(libcxx.Root, "lib"),
			// Use the -l:lib...a form to avoid accidentally linking the
			// libraries dynamically.
			"-l:libc++.a",
			"-l:libc++abi.a",
			"-lpthread",
		),
	}

	if cpu == CortexA7 || cpu == ARM1176JZF_S {
		ct.CFlags = append(ct.CFlags, "-marm")
		ct.CXXFlags = append(ct.CXXFlags, "-marm")
	}

	return ct, nil
}
