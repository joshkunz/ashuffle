package integration_test

import (
	"context"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"runtime"
	"sort"
	"strings"
	"sync"
	"testing"
	"time"

	"ashuffle/testashuffle"
	"ashuffle/testmpd"

	"github.com/google/go-cmp/cmp"
	"github.com/joshkunz/fakelib/filesystem"
	"github.com/joshkunz/fakelib/library"
	"github.com/montanaflynn/stats"
	"golang.org/x/sync/semaphore"
)

const (
	ashuffleBin = "/ashuffle/build/ashuffle"

	goldMP3 = "/music.huge/gold.mp3"
)

const (
	// must be less than waitMax
	waitBackoff = 20 * time.Millisecond
	// 100ms, because we want ashuffle operations to be imperceptible.
	waitMax = 100 * time.Millisecond
)

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

type runOptions struct {
	Library      string
	AshuffleArgs []string
}

func run(ctx context.Context, t *testing.T, opts runOptions) (*testashuffle.Ashuffle, *testmpd.MPD, func()) {
	t.Helper()
	if opts.Library == "" {
		opts.Library = "/music"
	}
	mpd, err := testmpd.New(ctx, &testmpd.Options{LibraryRoot: opts.Library})
	if err != nil {
		t.Fatalf("failed to create new MPD instance: %v", err)
	}
	as, err := testashuffle.New(ctx, ashuffleBin, &testashuffle.Options{
		MPDAddress: mpd,
		Args:       opts.AshuffleArgs,
	})
	if err != nil {
		t.Fatalf("failed to start ashuffle with args %+v: %v", opts.AshuffleArgs, err)
	}
	cleanup := func() {
		if !mpd.IsOk() {
			t.Errorf("mpd communication error: %+v", mpd.Errors)
		}

		mpd.Shutdown()
	}
	return as, mpd, cleanup
}

func newLibrary() (*library.Library, error) {
	goldF, err := os.Open(goldMP3)
	if err != nil {
		return nil, err
	}

	return library.New(goldF)
}

func TestMain(m *testing.M) {
	// compile ashuffle
	origDir, err := os.Getwd()
	if err != nil {
		log.Fatalf("failed to getcwd: %v", err)
	}

	if err := os.Chdir("/ashuffle"); err != nil {
		log.Fatalf("failed to chdir to /ashuffle: %v", err)
	}

	fmt.Println("===> Running MESON")
	mesonCmd := exec.Command("meson", "build")
	mesonCmd.Stdout = os.Stdout
	mesonCmd.Stderr = os.Stderr
	if err := mesonCmd.Run(); err != nil {
		log.Fatalf("failed to run meson for ashuffle: %v", err)
	}

	fmt.Println("===> Building ashuffle")
	ninjaCmd := exec.Command("ninja", "-C", "build", "ashuffle")
	ninjaCmd.Stdout = os.Stdout
	ninjaCmd.Stderr = os.Stderr
	if err := ninjaCmd.Run(); err != nil {
		log.Fatalf("failed to build ashuffle: %v", err)
	}

	if err := os.Chdir(origDir); err != nil {
		log.Fatalf("failed to reset workdir: %v", err)
	}

	lib, err := newLibrary()
	if err != nil {
		log.Fatalf("failed to create new library: %v", err)
	}
	lib.Tracks = 20_000

	if err := os.Mkdir("/music.20k", os.ModePerm); err != nil {
		log.Fatalf("failed to create /music.20k: %v", err)
	}

	srv, err := filesystem.Mount(lib, "/music.20k", nil)
	if err != nil {
		log.Fatalf("failed to mount library: %v", err)
	}

	ret := m.Run()

	srv.Unmount()
	os.Exit(ret)
}

// Basic test, just to make sure we can start MPD and ashuffle.
func TestStartup(t *testing.T) {
	t.Parallel()
	ctx := context.Background()
	mpd, err := testmpd.New(ctx, &testmpd.Options{LibraryRoot: "/music"})
	if err != nil {
		t.Fatalf("Failed to create new MPD instance: %v", err)
	}
	ashuffle, err := testashuffle.New(ctx, ashuffleBin, &testashuffle.Options{
		MPDAddress: mpd,
	})
	if err != nil {
		t.Fatalf("Failed to create new ashuffle instance")
	}

	if err := ashuffle.Shutdown(); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}
	mpd.Shutdown()
}

func TestStartupFail(t *testing.T) {
	t.Parallel()
	ctx := context.Background()

	tests := []struct {
		desc               string
		options            *testashuffle.Options
		wantStderrContains string
	}{
		{
			desc:               "No MPD server running",
			wantStderrContains: "could not connect to mpd",
		},
		{
			desc:               "Group by and no-check",
			options:            &testashuffle.Options{Args: []string{"-g", "album", "--no-check"}},
			wantStderrContains: "group-by not supported with no-check",
		},
	}

	for _, test := range tests {
		as, err := testashuffle.New(ctx, ashuffleBin, test.options)
		if err != nil {
			t.Errorf("failed to start ashuffle: %v", err)
			continue
		}
		if err := as.Shutdown(testashuffle.ShutdownSoft); err == nil {
			t.Errorf("ashuffle shutdown cleanly, but we wanted an error: %v", err)
		}
		if stderr := as.Stderr.String(); !strings.Contains(stderr, test.wantStderrContains) {
			t.Errorf("want stderr contains %q, got stderr:\n%s", test.wantStderrContains, stderr)
		}
	}

}

func TestShuffleOnce(t *testing.T) {
	t.Parallel()
	ctx := context.Background()

	as, mpd, cleanup := run(ctx, t, runOptions{AshuffleArgs: []string{"-o", "3"}})
	defer cleanup()

	// Wait for ashuffle to exit.
	if err := as.Shutdown(testashuffle.ShutdownSoft); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}

	if state := mpd.PlayState(); state != testmpd.StateStop {
		t.Errorf("want mpd state stop, got: %v", state)
	}

	if queueLen := len(mpd.Queue()); queueLen != 3 {
		t.Errorf("len(mpd.Queue()) = %d, want 3", queueLen)
	}
}

// Starting up ashuffle in a clean MPD instance. The "default" workflow. Then
// we skip a song, and make sure ashuffle enqueues another song.
func TestBasic(t *testing.T) {
	t.Parallel()
	ctx := context.Background()

	as, mpd, cleanup := run(ctx, t, runOptions{})
	defer cleanup()

	// Wait for ashuffle to startup, and start playing a song.
	tryWaitFor(func() bool { return mpd.PlayState() == testmpd.StatePlay })

	if state := mpd.PlayState(); state != testmpd.StatePlay {
		t.Errorf("[before skip] mpd.PlayState() = %v, want play", state)
	}
	if queueLen := len(mpd.Queue()); queueLen != 1 {
		t.Errorf("[before skip] len(mpd.Queue()) = %d, want 1", queueLen)
	}
	if pos := mpd.QueuePos(); pos != 0 {
		t.Errorf("[before skip] mpd.QueuePos() = %d, want 0", pos)
	}

	// Skip a track, ashuffle should enqueue another song, and keep playing.
	mpd.Next()
	// Give ashuffle some time to try and react, otherwise the test always
	// fails.
	tryWaitFor(func() bool { return mpd.PlayState() == testmpd.StatePlay })

	if state := mpd.PlayState(); state != testmpd.StatePlay {
		t.Errorf("[after skip] mpd.PlayState() = %v, want play", state)
	}
	if queueLen := len(mpd.Queue()); queueLen != 2 {
		t.Errorf("[after skip] len(mpd.Queue()) = %d, want 2", queueLen)
	}
	if pos := mpd.QueuePos(); pos != 1 {
		t.Errorf("[after skip] mpd.QueuePos() = %d, want 1", pos)
	}

	if err := as.Shutdown(); err != nil {
		t.Errorf("ashuffle did not shut down cleanly: %v", err)
	}
}

type Groups [][]string

func (g Groups) Len() int      { return len(g) }
func (g Groups) Swap(i, j int) { g[i], g[j] = g[j], g[i] }
func (g Groups) Less(i, j int) bool {
	a, b := g[i], g[j]
	if len(a) < len(b) {
		return true
	}
	if len(b) < len(a) {
		return false
	}
	for idx := range b {
		if a[idx] < b[idx] {
			return true
		}
		if a[idx] > b[idx] {
			return false
		}
	}
	return false
}

func splitGroups(lines string) Groups {
	var groups Groups
	var cur []string
	for _, line := range strings.Split(strings.TrimSpace(lines), "\n") {
		if line == "---" {
			sort.Strings(cur)
			groups = append(groups, cur)
			cur = nil
			continue
		}
		cur = append(cur, line)
	}
	if len(cur) > 0 {
		sort.Strings(cur)
		groups = append(groups, cur)
	}
	return groups
}

func TestFromFile(t *testing.T) {
	tests := []struct {
		desc    string
		library string
		flags   []string
		input   []string
		want    Groups
	}{
		{
			desc:    "With excludes",
			library: "/music",
			flags: []string{
				"--exclude", "artist", "tours",
				// The real album name is "Traveller's Guide", partial match should
				// work.
				"--exclude", "artist", "jahzzar", "album", "traveller",
			},
			input: []string{
				"BoxCat_Games_-_10_-_Epic_Song.mp3",
				"Broke_For_Free_-_01_-_Night_Owl.mp3",
				"Jahzzar_-_05_-_Siesta.mp3",
				"Monk_Turner__Fascinoma_-_01_-_Its_Your_Birthday.mp3",
				"Tours_-_01_-_Enthusiast.mp3",
			},
			want: Groups{
				{"BoxCat_Games_-_10_-_Epic_Song.mp3"},
				{"Broke_For_Free_-_01_-_Night_Owl.mp3"},
				{"Monk_Turner__Fascinoma_-_01_-_Its_Your_Birthday.mp3"},
			},
		},
		{
			desc:    "By Album",
			library: "/music.20k",
			flags:   []string{"--by-album"},
			input: []string{
				"A/A/A.mp3",
				"A/A/B.mp3",
				"A/A/C.mp3",
				"A/A/D.mp3",
				"A/A/E.mp3",
				"A/A/F.mp3",
				"A/A/G.mp3",
				"A/A/H.mp3",
				"A/A/I.mp3",
				"A/A/J.mp3",
				"A/B/A.mp3",
				"A/B/B.mp3",
				"A/B/C.mp3",
				"A/B/D.mp3",
				"A/B/E.mp3",
				"A/B/F.mp3",
				"A/B/G.mp3",
				"A/B/H.mp3",
				"A/B/I.mp3",
				"A/B/J.mp3",
			},
			want: Groups{
				{
					"A/A/A.mp3",
					"A/A/B.mp3",
					"A/A/C.mp3",
					"A/A/D.mp3",
					"A/A/E.mp3",
					"A/A/F.mp3",
					"A/A/G.mp3",
					"A/A/H.mp3",
					"A/A/I.mp3",
					"A/A/J.mp3",
				},
				{
					"A/B/A.mp3",
					"A/B/B.mp3",
					"A/B/C.mp3",
					"A/B/D.mp3",
					"A/B/E.mp3",
					"A/B/F.mp3",
					"A/B/G.mp3",
					"A/B/H.mp3",
					"A/B/I.mp3",
					"A/B/J.mp3",
				},
			},
		},
	}

	ctx := context.Background()
	for _, test := range tests {
		t.Run(test.desc, func(t *testing.T) {
			test := test
			t.Parallel()

			linesF, err := writeLines(test.input)
			if err != nil {
				t.Fatalf("couldn't write db lines to file: %v", err)
			}
			defer linesF.Cleanup()

			as, _, cleanup := run(ctx, t, runOptions{
				Library: test.library,
				AshuffleArgs: append([]string{
					"-f", linesF.Path(),
					"--test_enable_option_do_not_use", "print_all_songs_and_exit",
				}, test.flags...),
			})
			defer cleanup()

			// Wait for ashuffle to exit.
			if err := as.Shutdown(testashuffle.ShutdownSoft); err != nil {
				t.Errorf("ashuffle did not shut down cleanly: %v", err)
			}

			got := splitGroups(as.Stdout.String())

			sort.Sort(got)
			sort.Sort(test.want)

			if diff := cmp.Diff(test.want, got); diff != "" {
				t.Errorf("shuffle songs differ (want -> got):\n%s", diff)
			}
		})
	}
}

// Implements MPDAddress, wrapping the given MPDAddress with the appropriate
// password.
type mpdPasswordAddressWrapper struct {
	wrap     testashuffle.MPDAddress
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
	mpd, err := testmpd.New(ctx, &testmpd.Options{
		LibraryRoot:        "/music",
		DefaultPermissions: []string{"read"},
		Passwords: []testmpd.Password{
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
	as, err := testashuffle.New(ctx, ashuffleBin, &testashuffle.Options{
		MPDAddress: mpdPasswordAddressWrapper{
			wrap:     mpd,
			password: "anybody_can_see",
		},
	})
	if err != nil {
		t.Fatalf("[step 1] failed to create ashuffle: %v", err)
	}

	err = as.Shutdown(testashuffle.ShutdownSoft)
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
	as, err = testashuffle.New(ctx, ashuffleBin, &testashuffle.Options{
		MPDAddress: mpdPasswordAddressWrapper{
			wrap:     mpd,
			password: "super_secret_mpd_password",
		},
	})
	if err != nil {
		t.Fatalf("failed to create ashuffle instance: %v", err)
	}

	tryWaitFor(func() bool { return mpd.PlayState() == testmpd.StatePlay })

	if state := mpd.PlayState(); state != testmpd.StatePlay {
		t.Errorf("[step 2] mpd.PlayState = %v, want play", state)
	}

	if err := as.Shutdown(); err != nil {
		t.Errorf("failed to shutdown ashuffle cleanly")
	}

	if !mpd.IsOk() {
		t.Errorf("mpd communication error: %+v", mpd.Errors)
	}

	mpd.Shutdown()
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

	max := func(a, b int) int {
		if a > b {
			return a
		}
		return b
	}

	// Parallelism controls the number of parallel startup tests that are
	// allowed to run at once.
	parallelism := max(1, runtime.NumCPU()/4)
	// Trials controls the number of startup time samples that will be
	// collected.
	trials := 50
	// Threshold is the level at which the 95th percentile startup time is
	// considered a "failure" for the purposes of this test. Note: making this
	// threshold less than 1ms may not work, since the 95th percentile is
	// calculated in milliseconds.
	threshold := 750 * time.Millisecond

	// Start up MPD and read out the DB, so we can build a file list for the
	// "from file" test. We will immediately shut down this instance, because
	// we only need it to fetch the DB.
	mpd, err := testmpd.New(ctx, &testmpd.Options{LibraryRoot: "/music.20k"})
	if err != nil {
		t.Fatalf("failed to create new MPD instance: %v", err)
	}
	dbF, err := writeLines(mpd.Db())
	if err != nil {
		t.Fatalf("failed to build db file: %v", err)
	}
	defer dbF.Cleanup()
	mpd.Shutdown()

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
		{
			name: "from mpd, group-by artist",
			args: []string{"-f", dbF.Path(), "-o", "1", "-g", "artist"},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			test := test
			sem := semaphore.NewWeighted(int64(parallelism))
			wg := new(sync.WaitGroup)
			ch := make(chan time.Duration)

			runOnce := func() {
				sem.Acquire(ctx, 1)
				defer wg.Done()
				defer sem.Release(1)

				mpd, err := testmpd.New(ctx, &testmpd.Options{LibraryRoot: "/music.20k"})
				if err != nil {
					t.Fatalf("failed to create new MPD instance: %v", err)
				}
				defer mpd.Shutdown()

				start := time.Now()
				as, err := testashuffle.New(ctx, ashuffleBin, &testashuffle.Options{
					MPDAddress: mpd,
					Args:       test.args,
				})
				if err != nil {
					t.Fatalf("failed to create new ashuffle instance")
				}

				if err := as.Shutdown(testashuffle.ShutdownSoft); err != nil {
					t.Fatalf("ashuffle did not shut down cleanly: %v", err)
				}
				ch <- time.Since(start)

				if !mpd.IsOk() {
					t.Fatalf("mpd communication error: %v", mpd.Errors)
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
