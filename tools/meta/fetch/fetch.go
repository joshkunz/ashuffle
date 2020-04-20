package fetch

import (
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"strings"

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

	if err := exec.Command("git", "init").Run(); err != nil {
		return nil, fmt.Errorf("failed to init: %w", err)
	}

	if err := exec.Command("git", "fetch", "--tags", "--depth=1", url).Run(); err != nil {
		return nil, fmt.Errorf("failed to fetch: %w", err)
	}

	tagCmd := exec.Command("git", "tag")
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
