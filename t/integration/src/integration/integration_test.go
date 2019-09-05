package integration_test

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"testing"
	"time"

	"ashuffle"
	"mpd"
)

const ashuffleBin = "/ashuffle/build/ashuffle"

func panicf(format string, params ...interface{}) {
	panic(fmt.Sprintf(format, params...))
}

func TestMain(m *testing.M) {
	// compile ashuffle
	origDir, err := os.Getwd()
	if err != nil {
		panicf("failed to getcwd: %v", err)
	}

	if err := os.Chdir("/ashuffle"); err != nil {
		panicf("failed to chdir to /ashuffle: %v", err)
	}

	fmt.Println("===> Running MESON")
	mesonCmd := exec.Command("meson", "build")
	mesonCmd.Stdout = os.Stdout
	mesonCmd.Stderr = os.Stderr
	if err := mesonCmd.Run(); err != nil {
		panicf("failed to run meson for ashuffle: %v", err)
	}

	fmt.Println("===> Building ashuffle")
	ninjaCmd := exec.Command("ninja", "-C", "build", "ashuffle")
	ninjaCmd.Stdout = os.Stdout
	ninjaCmd.Stderr = os.Stderr
	if err := ninjaCmd.Run(); err != nil {
		panicf("failed to build ashuffle: %v", err)
	}

	if err := os.Chdir(origDir); err != nil {
		panicf("failed to rest workdir: %v", err)
	}

	os.Exit(m.Run())
}

// Basic test, just to make sure we can start MPD and ashuffle.
func TestStartup(t *testing.T) {
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("Failed to create new MPD instance: %v", err)
	}
	ashuffle, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdi,
	})
	if err != nil {
		t.Fatalf("Failed to create new ashuffle instance")
	}

	if err := ashuffle.Shutdown(); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}
	mpdi.Shutdown()
}

func TestShuffleOnce(t *testing.T) {
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("failed to create new MPD instance: %v", err)
	}
	ashuffle, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdi,
		Args:       []string{"-o", "3"},
	})
	if err != nil {
		t.Fatalf("failed to create new ashuffle instance")
	}

	time.Sleep(time.Second)

	if state := mpdi.PlayState(); state != mpd.StateStop {
		t.Errorf("want mpd state play, got: %v", state)
	}

	if queueLen := len(mpdi.Queue()); queueLen != 3 {
		t.Errorf("want mpd queue len 3, got %d", queueLen)
	}

	if !mpdi.IsOk() {
		t.Errorf("mpd communication error: %v", mpdi.Errors)
	}

	if err := ashuffle.Shutdown(); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}
	mpdi.Shutdown()
}
