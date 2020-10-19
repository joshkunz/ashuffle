// Package project provides utilities for working with different build
// systems used by different projects. Current Automake and Meson are
// supported.
package project

import (
	"fmt"
	"log"
	"os"
	"path"
	"strings"

	"meta/exec"
)

// Project is a helpful interface for structures that can be built and
// installed.
type Project interface {
	// Configure configures the project with the given install prefix.
	Configure(prefix string) error
	// Build builds the given target (but does not install). If target
	// is empty string, the default target for the project is built.
	Build(target string) error
	// Install installs the project to the prefix given in the Configure
	// stage.
	Install() error
}

// Install installs the given project to the given prefix. It executes the
// needed steps to configure and install.
func Install(p Project, prefix string) error {
	if err := p.Configure(prefix); err != nil {
		return err
	}
	return p.Install()
}

func cd(to string) func() {
	cwd, err := os.Getwd()
	if err != nil {
		log.Fatalf("failed to Getwd: %v", err)
	}
	if err := os.Chdir(to); err != nil {
		log.Fatalf("failed to chdir to %q: %v", to, err)
	}
	return func() {
		_ = os.Chdir(cwd)
	}
}

// MesonBuildType represents the various types of build that can be performed
// with meson. These can be provided as MesonOptions when a meson project
// is created.
type MesonBuildType int

const (
	BuildPlain MesonBuildType = iota
	BuildDebug
	BuildDebugOptimized
	BuildRelease
)

// String implements fmt.Stringer for MesonBuildType.
func (m MesonBuildType) String() string {
	switch m {
	case BuildPlain:
		return "plain"
	case BuildDebug:
		return "debug"
	case BuildDebugOptimized:
		return "debugoptimized"
	case BuildRelease:
		return "release"
	}
	return "<unknown>"
}

func (m MesonBuildType) flag() string {
	return "--buildtype=" + m.String()
}

type MesonOptions struct {
	// BuildType the build type (essentially optimization level) to perform.
	BuildType MesonBuildType
	// BuildDirectory is the directory where the build should take place. By
	// default, the directory "build" in the project root is used.
	BuildDirectory string
	// Extra provides additional flags that should be provided to meson as
	// part of the configure step.
	Extra []string
}

// Meson represents a meson project.
type Meson struct {
	// Root is the Meson project root (where meson.build is).
	Root string

	opts MesonOptions
}

// Make sure Meson implements the Project interface.
var _ Project = (*Meson)(nil)

// Configure implements Project.Configure for Meson.
func (m *Meson) Configure(dest string) error {
	cleanup := cd(m.Root)
	defer cleanup()
	cmd := exec.Command(
		"meson",
		".", m.opts.BuildDirectory,
		m.opts.BuildType.flag(),
	)
	if dest != "" {
		cmd.Args = append(cmd.Args, "--prefix="+dest)
	}
	cmd.Args = append(cmd.Args, m.opts.Extra...)
	return cmd.Run()
}

// Build implements Project.Build for Meson.
func (m *Meson) Build(target string) error {
	cleanup := cd(m.Root)
	defer cleanup()
	cmd := exec.Command("ninja", "-C", m.opts.BuildDirectory)
	if target != "" {
		cmd.Args = append(cmd.Args, target)
	}
	return cmd.Run()
}

// Install implements Project.Install for Meson.
func (m *Meson) Install() error {
	cleanup := cd(m.Root)
	defer cleanup()
	return exec.Command("ninja", "-C", m.opts.BuildDirectory, "install").Run()
}

// NewMeson creates a new meson project rooted in the given directory.
func NewMeson(dir string, opts ...MesonOptions) (*Meson, error) {
	var o MesonOptions
	if len(opts) > 0 {
		o = opts[0]
	}
	if o.BuildDirectory == "" {
		o.BuildDirectory = "build"
	}
	return &Meson{
		Root: dir,
		opts: o,
	}, nil
}

// Automake represents a project that uses Automake.
type Automake struct {
	Root string
}

// Make sure Automake implements the Project interface.
var _ Project = (*Automake)(nil)

// Configure implements Project.Configure for Automake.
func (a *Automake) Configure(prefix string) error {
	cleanup := cd(a.Root)
	defer cleanup()
	return exec.Command(
		"./configure",
		"--quiet", "--enable-silent-rules", "--disable-documentation",
		"--prefix="+prefix,
	).Run()
}

// Build implements Project.Build for Automake.
func (a *Automake) Build(target string) error {
	cleanup := cd(a.Root)
	defer cleanup()
	cmd := exec.Command("make", "-j", "16")
	if target != "" {
		cmd.Args = append(cmd.Args, target)
	}
	return cmd.Run()
}

// Install implements Project.Install for Automake.
func (a *Automake) Install() error {
	cleanup := cd(a.Root)
	defer cleanup()
	return exec.Command("make", "-j", "16", "install").Run()
}

// NewAutomake returns a new Automake project rooted at the given directory.
func NewAutomake(dir string) (*Automake, error) {
	return &Automake{
		Root: dir,
	}, nil
}

// CMakeVariables represents a collection of variables to be passed to
// CMake (these are typically passed via -D<VAR>=<VALUE>).
type CMakeVariables map[string]string

// CMakeOptions represents CMake options for a build.
type CMakeOptions struct {
	// BuildDirectory is the directory where the build is run from. If unset
	// then a "build" directory within the project root is used.
	BuildDirectory string
	// CCompiler and CXXCompiler are the C and C++ compiler binaries
	// respectively.
	CCompiler, CXXCompiler string
	// CFlags and CXXFlags are extra C and C++ that will be passed to the
	// C and C++ compilers respectively.
	CFlags, CXXFlags []string
	// Extra contains additional CMake variables that will be passed to CMake.
	Extra CMakeVariables
}

// CMake represents a project that uses the CMake build system.
type CMake struct {
	Root string

	opts CMakeOptions
}

// Make sure CMake implements the project interface.
var _ Project = (*CMake)(nil)

// Configure implements Project.Configure for CMake.
func (c *CMake) Configure(prefix string) error {
	cleanup := cd(c.opts.BuildDirectory)
	defer cleanup()
	cmd := exec.Command("cmake", "-GNinja")
	if prefix != "" {
		cmd.Args = append(cmd.Args, "-DCMAKE_INSTALL_PREFIX="+prefix)
	}
	if c.opts.CCompiler != "" {
		cmd.Args = append(cmd.Args, "-DCMAKE_C_COMPILER="+c.opts.CCompiler)
	}
	if c.opts.CXXCompiler != "" {
		cmd.Args = append(cmd.Args, "-DCMAKE_CXX_COMPILER="+c.opts.CXXCompiler)
	}
	if len(c.opts.CFlags) > 0 {
		cmd.Args = append(cmd.Args, "-DCMAKE_C_FLAGS="+strings.Join(c.opts.CFlags, " "))
	}
	if len(c.opts.CXXFlags) > 0 {
		cmd.Args = append(cmd.Args, "-DCMAKE_CXX_FLAGS="+strings.Join(c.opts.CXXFlags, " "))
	}
	for varname, value := range c.opts.Extra {
		cmd.Args = append(cmd.Args, fmt.Sprintf("-D%s=%s", varname, value))
	}
	cmd.Args = append(cmd.Args, c.Root)
	return cmd.Run()
}

// Build implements Project.Build for CMake.
func (c *CMake) Build(target string) error {
	cleanup := cd(c.opts.BuildDirectory)
	defer cleanup()
	cmd := exec.Command("ninja")
	if target != "" {
		cmd.Args = append(cmd.Args, target)
	}
	return cmd.Run()
}

// Install implements Project.Install for CMake.
func (c *CMake) Install() error {
	cleanup := cd(c.opts.BuildDirectory)
	defer cleanup()
	return exec.Command("ninja", "install").Run()
}

// NewCMake creates a new CMake object representing the CMake project at the
// given directory `dir`. The optional `opts` argument can be used to control
// the configuration of the project's build.
func NewCMake(dir string, opts ...CMakeOptions) (*CMake, error) {
	cmake := CMake{Root: dir}
	if len(opts) > 0 {
		cmake.opts = opts[0]
	}
	if cmake.opts.BuildDirectory == "" {
		buildDir := path.Join(dir, "build")
		if err := os.Mkdir(buildDir, os.ModePerm); err != nil {
			return nil, err
		}
		cmake.opts.BuildDirectory = buildDir
	}
	return &cmake, nil
}
