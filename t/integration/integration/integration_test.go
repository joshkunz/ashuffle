package integration_test

import (
	"context"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"sort"
	"strings"
	"sync"
	"testing"
	"time"

	"ashuffle/ashuffle"
	"ashuffle/mpd"

	"github.com/google/go-cmp/cmp"
	"github.com/montanaflynn/stats"
	"golang.org/x/sync/semaphore"
)

const ashuffleBin = "/ashuffle/build/ashuffle"

const (
	// must be less than waitMax
	waitBackoff = 20 * time.Millisecond
	// 100ms, because we want ashuffle operations to be imperceptible.
	waitMax = 100 * time.Millisecond
)

func panicf(format string, params ...interface{}) {
	panic(fmt.Sprintf(format, params...))
}

// Optimistically wait for some condition to be true. Sometimes, we need to
// wait for ashuffle to perform some action, and since this is a test, it
// may or may not successfully perform that action. To avoid putting in
// load-bearing sleeps that slow down the test, and make it more fragile, we
// can use this function instead. Ideally, it completes instantly, but it
// may take a few hundred millis before ashuffle actually gets around to
// doing what it is supposed to do.
func tryWaitFor(cond func() bool) {
	maxWaitC := time.After(waitMax)
	ticker := time.Tick(waitBackoff)
	for {
		select {
		case <-maxWaitC:
			log.Printf("giving up after waiting %s", waitMax)
			return
		case <-ticker:
			if cond() {
				return
			}
		}
	}
}

type linesFile struct {
	f *os.File
}

func (l linesFile) Path() string {
	return l.f.Name()
}

func (l linesFile) Cleanup() error {
	p := l.Path()
	l.f.Close()
	return os.Remove(p)
}

func writeLines(lines []string) (linesFile, error) {
	inputF, err := ioutil.TempFile(os.TempDir(), "ashuffle-input")
	if err != nil {
		return linesFile{}, fmt.Errorf("couldn't open tempfile: %w", err)
	}

	if _, err := io.WriteString(inputF, strings.Join(lines, "\n")); err != nil {
		return linesFile{}, fmt.Errorf("couldn't write lines into tempfile: %w", err)
	}

	return linesFile{inputF}, nil
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
	t.Parallel()
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
	t.Parallel()
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("failed to create new MPD instance: %v", err)
	}
	as, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdi,
		Args:       []string{"-o", "3"},
	})
	if err != nil {
		t.Fatalf("failed to create new ashuffle instance")
	}

	// Wait for ashuffle to exit.
	if err := as.Shutdown(ashuffle.ShutdownSoft); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}

	if state := mpdi.PlayState(); state != mpd.StateStop {
		t.Errorf("want mpd state stop, got: %v", state)
	}

	if queueLen := len(mpdi.Queue()); queueLen != 3 {
		t.Errorf("want mpd queue len 3, got %d", queueLen)
	}

	if !mpdi.IsOk() {
		t.Errorf("mpd communication error: %v", mpdi.Errors)
	}

	mpdi.Shutdown()
}

// Starting up ashuffle in a clean MPD instance. The "default" workflow. Then
// we skip a song, and make sure ashuffle enqueues another song.
func TestBasic(t *testing.T) {
	t.Parallel()
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("failed to create mpd instance: %v", err)
	}
	ashuffle, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdi,
	})

	// Wait for ashuffle to startup, and start playing a song.
	tryWaitFor(func() bool { return mpdi.PlayState() == mpd.StatePlay })

	if state := mpdi.PlayState(); state != mpd.StatePlay {
		t.Errorf("[before skip] want mpd state play, got %v", state)
	}
	if queueLen := len(mpdi.Queue()); queueLen != 1 {
		t.Errorf("[before skip] want mpd queue len == 1, got len %d", queueLen)
	}
	if pos := mpdi.QueuePos(); pos != 0 {
		t.Errorf("[before skip] want mpd queue pos == 0, got %d", pos)
	}

	// Skip a track, ashuffle should enqueue another song, and keep playing.
	mpdi.Next()
	// Give ashuffle some time to try and react, otherwise the test always
	// fails.
	tryWaitFor(func() bool { return mpdi.PlayState() == mpd.StatePlay })

	if state := mpdi.PlayState(); state != mpd.StatePlay {
		t.Errorf("[after skip] want mpd state play, got %v", state)
	}
	if queueLen := len(mpdi.Queue()); queueLen != 2 {
		t.Errorf("[after skip] want mpd queue len == 2, got len %d", queueLen)
	}
	if pos := mpdi.QueuePos(); pos != 1 {
		t.Errorf("[after skip] want mpd queue pos == 1, got %d", pos)
	}

	if !mpdi.IsOk() {
		t.Errorf("mpd communication error: %v", mpdi.Errors)
	}
	if err := ashuffle.Shutdown(); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}
	mpdi.Shutdown()
}

func TestFromFile(t *testing.T) {
	t.Parallel()
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("failed to create mpd instance: %v", err)
	}

	// These are the songs we'll ask ashuffle to use. They should all be
	// in the integration root container.
	db := []string{
		"BoxCat_Games_-_10_-_Epic_Song.mp3",
		"Broke_For_Free_-_01_-_Night_Owl.mp3",
		"Jahzzar_-_05_-_Siesta.mp3",
		"Monk_Turner__Fascinoma_-_01_-_Its_Your_Birthday.mp3",
		"Tours_-_01_-_Enthusiast.mp3",
	}

	// The same as "db", but without the songs by Jahzzar and Tours
	want := []string{
		"BoxCat_Games_-_10_-_Epic_Song.mp3",
		"Broke_For_Free_-_01_-_Night_Owl.mp3",
		"Monk_Turner__Fascinoma_-_01_-_Its_Your_Birthday.mp3",
	}

	linesF, err := writeLines(db)
	if err != nil {
		t.Fatalf("couldn't write db lines to file: %v", err)
	}
	defer linesF.Cleanup()

	as, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdi,
		Args: []string{
			"--exclude", "artist", "tours",
			// The real album name is "Traveller's Guide", partial match should
			// work.
			"--exclude", "artist", "jahzzar", "album", "traveller",
			// Pass in our list of songs.
			"-f", linesF.Path(),
			// Then, we make ashuffle just print the list of songs and quit
			"--test_enable_option_do_not_use", "print_all_songs_and_exit",
		},
	})
	if err != nil {
		t.Fatalf("failed to start ashuffle: %v", err)
	}

	// Wait for ashuffle to exit.
	if err := as.Shutdown(ashuffle.ShutdownSoft); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}

	got := strings.Split(strings.TrimSpace(as.Stdout.String()), "\n")

	sort.Strings(want)
	sort.Strings(got)

	if diff := cmp.Diff(want, got); diff != "" {
		t.Errorf("shuffle songs differ, diff want got:\n%s", diff)
	}

	mpdi.Shutdown()
}

// Implements MPDAddress, wrapping the given MPDAddress with the appropriate
// password.
type mpdPasswordAddressWrapper struct {
	wrap     ashuffle.MPDAddress
	password string
}

func (m mpdPasswordAddressWrapper) Address() (string, string) {
	wrap_host, wrap_port := m.wrap.Address()
	host := m.password + "@" + wrap_host
	return host, wrap_port
}

func TestPassword(t *testing.T) {
	t.Parallel()
	ctx := context.Background()
	mpdi, err := mpd.New(ctx, &mpd.Options{
		LibraryRoot:        "/music",
		DefaultPermissions: []string{"read"},
		Passwords: []mpd.Password{
			{
				Password:    "super_secret_mpd_password",
				Permissions: []string{"read", "add", "control", "admin"},
			},
			{
				Password:    "anybody_can_see",
				Permissions: []string{"read"},
			},
		},
	})
	if err != nil {
		t.Fatalf("failed to create mpd instance: %v", err)
	}

	// Step 1. Create an ashuffle client with the wrong password, it should
	// fail gracefully.
	as, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdPasswordAddressWrapper{
			wrap:     mpdi,
			password: "anybody_can_see",
		},
	})
	if err != nil {
		t.Fatalf("[step 1] failed to create ashuffle: %v", err)
	}

	err = as.Shutdown(ashuffle.ShutdownSoft)
	if err == nil {
		t.Errorf("[step 1] ashuffle shutdown cleanly, wanted error")
	} else if eErr, ok := err.(*exec.ExitError); ok {
		if eErr.Success() {
			t.Errorf("[step 1] ashuffle exited successfully, wanted error")
		}
	} else {
		t.Errorf("[step 1] unexpected error: %v", err)
	}

	// Step 2. Create an ashuffle client with the correct password. It should
	// work like the Basic test case.
	as, err = ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
		MPDAddress: mpdPasswordAddressWrapper{
			wrap:     mpdi,
			password: "super_secret_mpd_password",
		},
	})
	if err != nil {
		t.Fatalf("failed to create ashuffle instance: %v", err)
	}

	tryWaitFor(func() bool { return mpdi.PlayState() == mpd.StatePlay })

	if state := mpdi.PlayState(); state != mpd.StatePlay {
		t.Errorf("[step 2] want mpd state play, got %v", state)
	}

	if err := as.Shutdown(); err != nil {
		t.Errorf("failed to shutdown ashuffle cleanly")
	}
	mpdi.Shutdown()
}

// TestFastStartup verifies that ashuffle can load the "huge" music
// library (see the ashuffle root container for details of how it is created)
// and startup within a set threshold. This test is designed to detect
// performance regresssions in ashuffle startup.
// This test closely mirrors the "ShuffleOnce" test.
func TestFastStartup(t *testing.T) {
	// No t.Parallel(), since this benchmark is performance sensitive. We want
	// to avoid CPU or I/O starvation from other tests.
	ctx := context.Background()

	// Parallelism controls the number of parallel startup tests that are
	// allowed to run at once.
	parallelism := 10
	// Trials controls the number of startup time samples that will be
	// collected.
	trials := 50
	// Threshold is the level at which the 95th percentile startup time is
	// considered a "failure" for the purposes of this test. Right now it
	// is pegged to 400ms. Note: making this threshold less than 1ms may not
	// work, since the 95th percentile is calculated in milliseconds.
	threshold := 400 * time.Millisecond

	// Start up MPD and read out the DB, so we can build a file list for the
	// "from file" test. We will immediately shut down this instance, because
	// we only need it to fetch the DB.
	mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music.huge"})
	if err != nil {
		t.Fatalf("failed to create new MPD instance: %v", err)
	}
	dbF, err := writeLines(mpdi.Db())
	if err != nil {
		t.Fatalf("failed to build db file: %v", err)
	}
	defer dbF.Cleanup()
	mpdi.Shutdown()

	tests := []struct {
		name string
		args []string
		once func(*testing.T) time.Duration
	}{
		{
			name: "from mpd",
			args: []string{"-o", "1"},
		},
		{
			name: "from file",
			args: []string{"-f", dbF.Path(), "-o", "1"},
		},
		{
			name: "from file, with filter",
			args: []string{"-f", dbF.Path(), "-o", "1", "-e", "artist", "AA"},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			sem := semaphore.NewWeighted(int64(parallelism))
			wg := new(sync.WaitGroup)
			ch := make(chan time.Duration)

			runOnce := func() {
				sem.Acquire(ctx, 1)
				defer wg.Done()
				defer sem.Release(1)

				mpdi, err := mpd.New(ctx, &mpd.Options{LibraryRoot: "/music.huge"})
				if err != nil {
					t.Fatalf("failed to create new MPD instance: %v", err)
				}
				defer mpdi.Shutdown()

				start := time.Now()
				as, err := ashuffle.New(ctx, ashuffleBin, &ashuffle.Options{
					MPDAddress: mpdi,
					Args:       test.args,
				})
				if err != nil {
					t.Fatalf("failed to create new ashuffle instance")
				}

				if err := as.Shutdown(ashuffle.ShutdownSoft); err != nil {
					t.Fatalf("ashuffle did not shut down cleanly: %v", err)
				}
				ch <- time.Since(start)

				if !mpdi.IsOk() {
					t.Fatalf("mpd communication error: %v", mpdi.Errors)
				}
			}

			for i := 0; i < trials; i++ {
				wg.Add(1)
				go runOnce()
			}

			go func() {
				wg.Wait()
				close(ch)
			}()

			var runtimesMs []float64
			for result := range ch {
				runtimesMs = append(runtimesMs, float64(result.Milliseconds()))
			}

			pct95, err := stats.Percentile(runtimesMs, 95)
			if err != nil {
				t.Fatalf("failed to calculate 95th percentile: %v", err)
			}

			if d95 := time.Duration(pct95) * time.Millisecond; d95 > threshold {
				t.Errorf("ashuffle took %v to startup, want %v or less.", d95, threshold)
			}
		})
	}
}
