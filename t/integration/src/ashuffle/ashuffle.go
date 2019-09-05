package ashuffle

import (
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

type Ashuffle struct {
	cmd        *exec.Cmd
	cancelFunc func()

	Stdout *bytes.Buffer
	Stderr *bytes.Buffer
}

func (a *Ashuffle) Shutdown() error {
	a.cancelFunc()
	err := a.cmd.Wait()
	if err == nil {
		return nil
	} else if err, ok := err.(*exec.ExitError); ok {
		bySig := err.ExitCode() == -1
		status := err.Sys().(syscall.WaitStatus)
		// We actually expect ashuffle to get killed by the cancel func
		// most of the time, don't fail for "real" if that happens.
		if bySig && status.Signal() == syscall.SIGKILL {
			return nil
		}
		if err.Success() {
			return nil
		}
	}
	return err
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
	MPDAddress MPDAddress
	Args       []string
}

func New(ctx context.Context, path string, opts *Options) (*Ashuffle, error) {
	runCtx, cancel := context.WithCancel(ctx)
	var args []string
	cmd := exec.CommandContext(runCtx, path, args...)
	stdout := bytes.Buffer{}
	stderr := bytes.Buffer{}
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	if opts != nil {
		cmd.Args = append([]string{path}, opts.Args...)
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
		return nil, err
	}
	return &Ashuffle{
		cmd:        cmd,
		cancelFunc: cancel,
		Stdout:     &stdout,
		Stderr:     &stderr,
	}, nil
}
