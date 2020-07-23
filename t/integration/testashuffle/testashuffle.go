// Package testashuffle provides helpers for creating test ashuffle runs.
package testashuffle

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"syscall"
	"time"

	"github.com/joshkunz/massif"
)

// The default amount of time to wait for ashuffle to shutdown.
const maxShutdownWait = 5 * time.Second

type Ashuffle struct {
	cmd        *exec.Cmd
	cancelFunc func()

	Stdout *bytes.Buffer
	Stderr *bytes.Buffer

	// The filename of the massif output file.
	massifOutputFile string

	// Maximum duration to wait for ashuffle to shutdown before forced shutdown.
	shutdownTimeout time.Duration
}

type ShutdownType uint

const (
	ShutdownUnknown ShutdownType = iota
	ShutdownHard
	ShutdownSoft
)

// Waits for ashuffle to terminate. Once it does, it sends an empty struct
// on the returned channel and closes it. If the underlying process never
// terminates, the returned channel may never yield a value.
func (a *Ashuffle) tryWait() <-chan error {
	c := make(chan error, 1)
	go func() {
		c <- a.cmd.Wait()
		close(c)
	}()
	return c
}

// safeWait waits for the underlying ashuffle process to abort within
// MaxShutdownWait time units, or it forcibly kills the process by cancelling
// its context.
func (a *Ashuffle) safeWait() error {
	select {
	case err := <-a.tryWait():
		return err
	case <-time.After(a.shutdownTimeout):
		a.cancelFunc()
		return errors.New("ashuffle took too long to exit. It has been killed")
	}
}

// Shutdown the ashuffle instance. If sType is not given, or is ShutdownUnknown,
// or ShutdownHardthen a "hard" shutdown will be performed (ashuffle is
// killed). If ShutdownSoft is given, then the process will wait at most
// MaxShutdownWait time units for ashuffle to terminate normally, or it will
// forcibly terminate the process.
func (a *Ashuffle) Shutdown(sType ...ShutdownType) error {
	var t ShutdownType
	if len(sType) > 0 {
		t = sType[0]
	}

	// Don't send the abort signal, if we are just doing a "soft" shutdown.
	// Wait for the process to terminate normally.
	if t != ShutdownSoft {
		a.cancelFunc()
	}
	err := a.safeWait()
	if err == nil {
		return nil
	} else if err, ok := err.(*exec.ExitError); ok {
		bySig := err.ExitCode() == -1
		status := err.Sys().(syscall.WaitStatus)
		// If the user did not specify ShutdownSoft, we actually expect
		// ashuffle to get killed by the cancel func. Don't fail for "real" if
		// that happens.
		if t != ShutdownSoft && bySig && status.Signal() == syscall.SIGKILL {
			return nil
		}
		if err.Success() {
			return nil
		}
	}
	return err
}

func (a *Ashuffle) HeapProfile() (*massif.Massif, error) {
	if a.massifOutputFile == "" {
		return nil, errors.New("heap profiling not enabled for this run")
	}

	profile, err := os.Open(a.massifOutputFile)
	if err != nil {
		return nil, err
	}
	defer func() {
		profile.Close()
		os.Remove(a.massifOutputFile)
	}()

	return massif.Parse(profile)
}

type MPDAddress interface {
	// Address returns the MPD host and port (port may be empty if no
	// port is needed.
	Address() (string, string)
}

type literalMPDAddress struct {
	host string
	port string
}

func (l literalMPDAddress) Address() (string, string) {
	return l.host, l.port
}

// LiteralMPDAddress returns a new MPD address that always returns the given
// host/port.
func LiteralMPDAddress(host, port string) MPDAddress {
	return literalMPDAddress{host, port}
}

type Options struct {
	MPDAddress        MPDAddress
	Args              []string
	EnableHeapProfile bool
	ShutdownTimeout   time.Duration
}

func New(ctx context.Context, path string, opts *Options) (*Ashuffle, error) {
	runCtx, cancel := context.WithCancel(ctx)
	var cmd *exec.Cmd
	var massifOutput string
	if opts != nil && opts.EnableHeapProfile {
		mOut, err := ioutil.TempFile("", "ashuffle.massif")
		if err != nil {
			cancel()
			return nil, err
		}
		massifOutput = mOut.Name()
		cmd = exec.CommandContext(runCtx,
			"valgrind",
			"--tool=massif",
			"--massif-out-file="+massifOutput,
			path,
		)
		mOut.Close()
	} else {
		cmd = exec.CommandContext(runCtx, path)
	}

	var stdout, stderr bytes.Buffer
	cmd.Stderr = &stderr
	cmd.Stdout = &stdout

	shutdownTimeout := maxShutdownWait
	if opts != nil && opts.ShutdownTimeout != 0 {
		shutdownTimeout = opts.ShutdownTimeout
	}

	if opts != nil {
		cmd.Args = append(cmd.Args, opts.Args...)
		env := os.Environ()
		if opts.MPDAddress != nil {
			mpdHost, mpdPort := opts.MPDAddress.Address()
			if mpdHost != "" {
				env = append(env, fmt.Sprintf("MPD_HOST=%s", mpdHost))
			}
			if mpdPort != "" {
				env = append(env, fmt.Sprintf("MPD_PORT=%s", mpdPort))
			}
		}
		cmd.Env = env
	}

	if err := cmd.Start(); err != nil {
		cancel()
		return nil, err
	}
	return &Ashuffle{
		cmd:              cmd,
		cancelFunc:       cancel,
		Stdout:           &stdout,
		Stderr:           &stderr,
		massifOutputFile: massifOutput,
		shutdownTimeout:  shutdownTimeout,
	}, nil
}
