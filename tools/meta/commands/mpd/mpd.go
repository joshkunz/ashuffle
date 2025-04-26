package mpd

import (
	"context"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"github.com/urfave/cli/v3"

	"meta/exec"
	"meta/fetch"
	"meta/project"
	"meta/versions/mpdver"
	"meta/workspace"
)

func applyPatches(dir string) error {
	patches, err := filepath.Glob(filepath.Join(dir, "*.patch"))
	if err != nil {
		return err
	}

	slices.Sort(patches)

	for _, patch := range patches {
		log.Printf("Applying patch %q", patch)
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

func install(ctx context.Context, cmd *cli.Command) error {
	// Note: We do this first, so we can get the path before workspace.New()
	// moves us.
	var patchRoot string
	patchRoot, err := filepath.Abs(cmd.String("patch_root"))
	if err != nil {
		return fmt.Errorf("failed to find patch root %q: %w", cmd.String("patch_root"), err)
	}
	// Make sure we actually have a patch root.
	if patchRoot == "" {
		return fmt.Errorf("patch root is empty")
	}

	ws, err := workspace.New()
	if err != nil {
		return err
	}
	defer ws.Cleanup()

	v, err := mpdver.Resolve(cmd.String("version"))
	if err != nil {
		return err
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

	versionPatchDir := filepath.Join(patchRoot, "mpd", v.String())
	if stat, err := os.Stat(versionPatchDir); err == nil {
		// If the patch dir exists, then try to apply all the patches
		if !stat.IsDir() {
			return fmt.Errorf("patch dir path %q is not a directory", versionPatchDir)
		}
		if err := applyPatches(versionPatchDir); err != nil {
			return fmt.Errorf("failed to apply patches: %w", err)
		}
	} else if os.IsNotExist(err) {
		// If it doesn't exist, then just log that we're not applying anything
		log.Printf("Patch dir %q not found, no patches to apply", versionPatchDir)
	} else {
		return fmt.Errorf("failed to locate patch dir: %w", err)
	}

	proj, err := project.NewMeson(ws.Root, project.MesonOptions{
		BuildType:      project.BuildDebugOptimized,
		BuildDirectory: "build/release",
		Extra:          []string{"-Db_ndebug=true"},
	})
	if err != nil {
		return err
	}
	return project.Install(proj, cmd.String("prefix"))
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
