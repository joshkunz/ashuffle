package mpd

import (
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/urfave/cli/v2"

	"meta/exec"
	"meta/fetch"
	"meta/semver"
	"meta/workspace"
)

type version semver.Version

func (v version) String() string {
	if v.Patch == 0 {
		return fmt.Sprintf("%d.%d", v.Major, v.Minor)
	}
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

func (v version) ReleaseURL() string {
	return fmt.Sprintf("http://www.musicpd.org/download/mpd/%d.%d/mpd-%s.tar.xz", v.Major, v.Minor, v)
}

func applyPatches(dir string) error {
	patches, err := filepath.Glob(filepath.Join(dir, "*.patch"))
	if err != nil {
		return err
	}

	sort.Strings(patches)

	for _, patch := range patches {
		f, err := os.Open(patch)
		if err != nil {
			return fmt.Errorf("failed to open patch %q: %w", patch, err)
		}
		defer f.Close()
		cmd := exec.Command("patch", "-p1")
		cmd.Stdin = f
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("failed to apply %q: %w", patch, err)
		}
	}
	return nil
}

func parseVersion(s string) (version, error) {
	parsed, err := semver.Parse(s)
	return version(parsed), err
}

func latestVersion() (version, error) {
	tags, err := fetch.GitVersions("https://github.com/MusicPlayerDaemon/MPD.git")
	if err != nil {
		return version{}, err
	}

	var versions []version
	for _, tag := range tags {
		parsed, err := parseVersion(tag)
		if err != nil {
			continue
		}
		versions = append(versions, parsed)
	}

	if len(versions) < 1 {
		return version{}, errors.New("no valid versions found")
	}

	sort.Slice(versions, func(i, j int) bool {
		return semver.Less(semver.Version(versions[i]), semver.Version(versions[j]))
	})

	return versions[len(versions)-1], nil
}

func installAutomake(target string) error {
	log.Printf("Configuring MPD...")
	configure := exec.Command(
		"./configure", "--quiet", "--enable-silent-rules", "--prefix="+target,
	)
	if err := configure.Run(); err != nil {
		return err
	}

	makeCmd := exec.Command("make", "-j", "16", "install")
	return makeCmd.Run()
}

func installMeson(target string) error {
	meson := exec.Command(
		"meson", ".", "build/release",
		"--prefix="+target,
		"--buildtype=debugoptimized",
		"-Db_ndebug=true",
	)
	if err := meson.Run(); err != nil {
		return err
	}

	ninja := exec.Command("ninja", "-C", "build/release", "install")
	return ninja.Run()
}

func install(ctx *cli.Context) error {
	// Not being able to fined a patch root is only a problem for older
	// releases, and we'll only do those once in a while. The majority of
	// the time, we don't care if it exists. Save the error here, and
	// then print it later only if we actually need the patch root.
	// Note: We do this first, so we can get the path before workspace.New()
	// moves us.
	var patchRoot string
	var patchRootErr error
	patchRoot, patchRootErr = filepath.Abs(ctx.String("patch_root"))
	if patchRootErr != nil {
		patchRoot = ""
	}

	ws, err := workspace.New()
	if err != nil {
		return err
	}
	defer ws.Cleanup()

	var v version
	if ver := ctx.String("version"); ver == "latest" {
		log.Printf("version == latest, looking up latest version")
		latest, err := latestVersion()
		if err != nil {
			return err
		}
		v = latest
	} else {
		version, err := parseVersion(ver)
		if err != nil {
			return err
		}
		v = version
	}

	if v.Major != 0 {
		return fmt.Errorf("unsupported MPD Major version > 0: %s", v)
	}

	log.Printf("Picked MPD version %s", v)

	if err := fetch.URL(v.ReleaseURL(), "mpd.tar.xz"); err != nil {
		return err
	}

	if err := exec.Command("tar", "--strip-components=1", "-xJf", "mpd.tar.xz").Run(); err != nil {
		return err
	}

	if v.Minor == 19 && v.Patch < 15 {
		if patchRoot == "" {
			return fmt.Errorf("failed to find patch root %q: %w", ctx.String("patch_root"), err)
		}
		if err := applyPatches(filepath.Join(patchRoot, "0.19", "pre-15")); err != nil {
			return err
		}
	}

	if v.Minor < 21 {
		return installAutomake(ctx.String("prefix"))
	}
	return installMeson(ctx.String("prefix"))
}

var Command = &cli.Command{
	Name:  "mpd",
	Usage: "Install mpd.",
	Flags: []cli.Flag{
		&cli.StringFlag{
			Name:  "version",
			Value: "latest",
			Usage: strings.Join([]string{
				"version of mpd to install, or 'latest' to",
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
			Name:  "patch_root",
			Value: "",
			Usage: "The root directory for MPD patches.",
		},
	},
	Action: install,
}
