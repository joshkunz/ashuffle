package fetch

import (
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"sort"
	"strings"

	"meta/exec"
	"meta/semver"
	"meta/workspace"
)

// URL fetches the content at the given URL and stores it in the destination
// file. It returns an error if any steps fail.
func URL(url, dest string) error {
	f, err := os.Create(dest)
	if err != nil {
		return fmt.Errorf("could not open dest file %q: %w", dest, err)
	}
	defer f.Close()

	log.Printf("FETCH %q -> %q", url, dest)

	resp, err := http.Get(url)
	if err != nil {
		return fmt.Errorf("failed to fetch %q: %w", url, err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return fmt.Errorf("bad status for %q: %s", url, resp.Status)
	}
	if _, err := io.Copy(f, resp.Body); err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}
	return nil
}

// GitVersions clones the Git repo at the given URL, and returns all tag names
// in the repo. Returns an error if it's not able to clone the Git repo, or
// read the tags.
func GitVersions(url string) ([]string, error) {
	ws, err := workspace.New()
	if err != nil {
		return nil, err
	}
	defer ws.Cleanup()

	if err := exec.Silent("git", "init").Run(); err != nil {
		return nil, fmt.Errorf("failed to init: %w", err)
	}

	log.Printf("GIT FETCH %q", url)
	if err := exec.Silent("git", "fetch", "--tags", "--depth=1", url).Run(); err != nil {
		return nil, fmt.Errorf("failed to fetch: %w", err)
	}

	tagCmd := exec.Silent("git", "tag")
	stdout, err := tagCmd.StdoutPipe()
	if err != nil {
		return nil, fmt.Errorf("failed to get git tag pipe: %w", err)
	}
	if err := tagCmd.Start(); err != nil {
		return nil, err
	}

	tagsRaw, err := ioutil.ReadAll(stdout)
	if err != nil {
		return nil, err
	}

	if err := tagCmd.Wait(); err != nil {
		return nil, err
	}

	return strings.Split(string(tagsRaw), "\n"), nil
}

// GitLatest fetches the git tags from the repository at the given URL, and
// returns the latest tag following Semver versioning semantics.
func GitLatest(url string) (semver.Version, error) {
	tags, err := GitVersions(url)
	if err != nil {
		return semver.Version{}, fmt.Errorf("failed to fetch git versions: %w", err)
	}

	var versions []semver.Version
	for _, tag := range tags {
		parsed, err := semver.Parse(tag)
		if err != nil {
			continue
		}
		versions = append(versions, parsed)
	}

	if len(versions) < 1 {
		return semver.Version{}, errors.New("found no (semver-compliant) git tags")
	}

	sort.Slice(versions, func(i, j int) bool {
		return semver.Less(versions[i], versions[j])
	})

	return versions[len(versions)-1], nil
}
