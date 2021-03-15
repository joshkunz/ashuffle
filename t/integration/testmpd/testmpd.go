// Package testmpd provides helpers for starting and examining test MPD
// instances.
package testmpd

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"text/template"
	"time"

	"github.com/cenkalti/backoff"
	mpdc "github.com/fhs/gompd/mpd"
	"github.com/martinlindhe/unit"
)

const (
	mpdConnectBackoff  = 500 * time.Millisecond
	mpdConnectMax      = 30 * time.Second
	mpdUpdateDBBackoff = 100 * time.Millisecond
	mpdUpdateDBMax     = 30 * time.Second
)

// Password is the type of an MPD password. A literal password, a collection
// of permissions for users of that password.
type Password struct {
	// Password is the actual password the user must enter to get the
	// requested permissions.
	Password string

	// Permissions is the list of permissions granted to users of this
	// password.
	Permissions []string
}

// Options are the options used when creating this MPD instance.
type Options struct {
	// BinPath is the path to the MPD binary. Leave it empty to use the PATH
	// to search for MPD.
	BinPath string
	// LibraryRoot is the root directory of MPD's music library.
	LibraryRoot string
	// DefaultPermissions is the list of permissions given to unauthenticated
	// users. Leave it empty or nil to use the MPD default.
	DefaultPermissions []string

	// If non-zero, this value is set as the `max_output_buffer_size` option
	// in the MPD configuration.
	MaxOutputBufferSize unit.Datasize

	// The maximum amount of time to wait for MPD to update its database. If
	// unset, the default timeout is used.
	UpdateDBTimeout time.Duration

	// Passwords is the list of passwords to configure on this instance. See
	// `Password' for per-password options.
	Passwords []Password
}

type mpdTemplateInput struct {
	Options
	MPDRoot string
}

var mpdConfTemplate = template.Must(template.New("mpd.conf").
	Funcs(map[string]interface{}{
		"floatToInt": func(f float64) int64 {
			return int64(f)
		},
	}).
	Parse(`
music_directory     "{{ .LibraryRoot }}"
playlist_directory  "{{ .MPDRoot }}/playlists"
db_file             "{{ .MPDRoot }}/database"
pid_file            "{{ .MPDRoot }}/pid"
state_file          "{{ .MPDRoot }}/state"
sticker_file        "{{ .MPDRoot }}/sticker.sql"
bind_to_address     "{{ .MPDRoot }}/socket"
{{ if ne .MaxOutputBufferSize 0.0 }}max_output_buffer_size "{{ .MaxOutputBufferSize.Kibibytes | floatToInt }}"{{ end }}
audio_output {
	type		"null"
	name		"null"
}

{{ if .DefaultPermissions -}}
    default_permissions "
    {{- range $index, $perm := .DefaultPermissions -}}
        {{- if $index -}},{{- end -}}
        {{ $perm }}
    {{- end -}}
    "
{{- end -}}

{{ if .Passwords }}
{{ range .Passwords }}
password "{{ .Password }}@
    {{- range $index, $perm := .Permissions -}}
        {{- if $index -}},{{- end -}}
        {{ $perm }}
    {{- end -}}
"
{{- end }}
{{ end }}
`))

// build builds a conf file from the options, and returns the conf file
// text, as well as the MPD UNIX socket path (which is set in the
// configuration)
func (m Options) Build(rootDir string) (string, string) {
	var confFile bytes.Buffer

	if err := mpdConfTemplate.Execute(&confFile, mpdTemplateInput{m, rootDir}); err != nil {
		panic(err)
	}
	return confFile.String(), filepath.Join(rootDir, "socket")
}

// MPD is the type of an MPD instance. It can be constructed with New,
// and controlled with the various member methods.
type MPD struct {
	// Addr is the UNIX socket this MPD instance is listening on.
	Addr string

	// Stdout is a buffer that contains the standard output of the MPD process.
	Stdout *bytes.Buffer
	// Stderr is a buffer that contains the standard error of the MPD process.
	Stderr *bytes.Buffer

	root       root
	cmd        *exec.Cmd
	cli        *mpdc.Client
	cancelFunc func()

	// Errors is a list of all errors that have occured when trying to access
	// this MPD instance. Usually it should be empty, so it's not worth
	// checking most of the time. The `IsOk` can be used as a quick check
	// that access to this MPD instance is healthy.
	Errors []error
}

// Address returns `i.Addr` as the host, and an empty port.
func (m MPD) Address() (string, string) {
	return m.Addr, ""
}

// Shutdown shuts down this MPD instance, and cleans up associated data.
func (m *MPD) Shutdown() error {
	defer m.root.cleanup()
	m.cli.Close()
	m.cancelFunc()
	return m.cmd.Wait()
}

// IsOk returns true if there have been no errors on this instance. You can
// use MPD.Errors to see any errors that have occured.
func (m *MPD) IsOk() bool {
	return len(m.Errors) == 0
}

func (m *MPD) maybeErr(err error) {
	if err != nil {
		m.Errors = append(m.Errors, err)
	}
}

// Play plays the song in the current position in the MPD queue.
func (m *MPD) Play() {
	m.maybeErr(m.cli.Pause(false))
}

// Pause pauses the currently playing song.
func (m *MPD) Pause() {
	m.maybeErr(m.cli.Pause(true))
}

// Next skips the current song.
func (m *MPD) Next() {
	m.maybeErr(m.cli.Next())
}

// Prev goes back to the previous song.
func (m *MPD) Prev() {
	m.maybeErr(m.cli.Previous())
}

// Db returns a list of all URIs in this MPD instance's database.
func (m *MPD) Db() []string {
	res, err := m.cli.GetFiles()
	if err != nil {
		m.Errors = append(m.Errors, err)
		return nil
	}
	return res
}

// Queue returns an array of the songs currently in the queue.
func (m *MPD) Queue() []string {
	attrs, err := m.cli.PlaylistInfo(-1, -1)
	if err != nil {
		m.Errors = append(m.Errors, err)
		return nil
	}
	var result []string
	for _, attr := range attrs {
		result = append(result, attr["file"])
	}
	return result
}

func (m *MPD) QueuePos() int64 {
	attrs, err := m.cli.Status()
	if err != nil {
		m.Errors = append(m.Errors, err)
		return -1
	}
	res, err := strconv.ParseInt(attrs["song"], 10, 64)
	if err != nil {
		m.Errors = append(m.Errors, err)
		return -1
	}
	return res
}

type State string

const (
	StateUnknown = State("unknown")
	StatePlay    = State("play")
	StatePause   = State("pause")
	StateStop    = State("stop")
)

func (m *MPD) PlayState() State {
	attr, err := m.cli.Status()
	if err != nil {
		m.Errors = append(m.Errors, err)
		return StateUnknown
	}
	switch attr["state"] {
	case "play":
		return StatePlay
	case "pause":
		return StatePause
	case "stop":
		return StateStop
	}
	return StateUnknown
}

type root struct {
	path     string
	socket   string
	confPath string
}

func (r root) cleanup() {
	os.RemoveAll(r.path)
}

func buildRoot(opts *Options) (*root, error) {
	rootPath, err := ioutil.TempDir(os.TempDir(), "mpd-harness")
	if err != nil {
		return nil, err
	}
	conf, err := ioutil.TempFile(rootPath, "generated-conf")
	if err != nil {
		os.RemoveAll(rootPath)
		return nil, err
	}
	confPath := conf.Name()
	confString, mpdSocket := opts.Build(rootPath)
	if _, err := conf.Write([]byte(confString)); err != nil {
		os.RemoveAll(rootPath)
		return nil, err
	}
	conf.Close()
	return &root{
		path:     rootPath,
		socket:   mpdSocket,
		confPath: confPath,
	}, nil
}

// New creates a new MPD instance with the given options. If `opts' is nil,
// then default options will be used. If a new MPD instance cannot be created,
// an error is returned.
func New(ctx context.Context, opts *Options) (*MPD, error) {
	root, err := buildRoot(opts)
	if err != nil {
		return nil, err
	}

	mpdCtx, mpdCancel := context.WithCancel(ctx)
	stdout := bytes.Buffer{}
	stderr := bytes.Buffer{}
	mpdBin := "mpd"
	if opts != nil && opts.BinPath != "" {
		mpdBin = opts.BinPath
	}
	cmd := exec.CommandContext(mpdCtx, mpdBin, "--no-daemon", "--stderr", root.confPath)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		mpdCancel()
		root.cleanup()
		return nil, err
	}

	// Keep re-trying to connect to mpd every mpdConnectBackoff, aborting
	// if mpdConnectMax time units have gone by.
	connectCtx, cancel := context.WithTimeout(ctx, mpdConnectMax)
	defer cancel()
	connectBackoff := backoff.WithContext(backoff.NewConstantBackOff(mpdConnectBackoff), connectCtx)

	var cli *mpdc.Client
	err = backoff.Retry(func() error {
		onceCli, err := mpdc.Dial("unix", root.socket)
		if err != nil {
			return err
		}
		cli = onceCli
		return nil
	}, connectBackoff)
	if err != nil {
		mpdCancel()
		return nil, fmt.Errorf("failed to connect to mpd at %s: %v", root.socket, err)
	}
	if cli == nil {
		panic("backoff did not return an error. This should not happen.")
	}

	if err != nil {
		mpdCancel()
		root.cleanup()
		return nil, err
	}

	updateTimeout := mpdUpdateDBMax
	if opts != nil && opts.UpdateDBTimeout != 0 {
		updateTimeout = opts.UpdateDBTimeout
	}

	updateCtx, cancel := context.WithTimeout(ctx, updateTimeout)
	defer cancel()
	updateBackoff := backoff.WithContext(backoff.NewConstantBackOff(mpdUpdateDBBackoff), updateCtx)
	err = backoff.Retry(func() error {
		attr, err := cli.Status()
		if err != nil {
			return backoff.Permanent(err)
		}
		if attr["updating_db"] == "1" {
			return errors.New("db still updating")
		}
		return nil

	}, updateBackoff)
	if err != nil {
		mpdCancel()
		root.cleanup()
		return nil, fmt.Errorf("failed to wait for MPD db to update: %v", err)
	}

	return &MPD{
		Addr:   root.socket,
		Stdout: &stdout,
		Stderr: &stderr,

		root:       *root,
		cmd:        cmd,
		cli:        cli,
		cancelFunc: mpdCancel,
	}, nil
}
