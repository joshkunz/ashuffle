package mpd

import (
	"bytes"
	"context"
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
)

const (
	mpdConnectBackoff = 500 * time.Millisecond
	mpdConnectMax     = 10 * time.Second
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

	// Passwords is the list of passwords to configure on this instance. See
	// `Password' for per-password options.
	Passwords []Password
}

type mpdTemplateInput struct {
	Options
	MPDRoot string
}

var mpdConfTemplate = template.Must(template.New("mpd.conf").Parse(`
music_directory     "{{ .LibraryRoot }}"
playlist_directory  "{{ .MPDRoot }}/playlists"
db_file             "{{ .MPDRoot }}/database"
pid_file            "{{ .MPDRoot }}/pid"
state_file          "{{ .MPDRoot }}/state"
sticker_file        "{{ .MPDRoot }}/sticker.sql"
bind_to_address     "{{ .MPDRoot }}/socket"
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

// Instance is the type of an MPD instance. It can be constructed with mpd.New,
// and controlled with the various member methods.
type Instance struct {
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
func (i Instance) Address() (string, string) {
	return i.Addr, ""
}

// Shutdown shuts down this MPD instance, and cleans up associated data.
func (i *Instance) Shutdown() error {
	defer i.root.cleanup()
	i.cli.Close()
	i.cancelFunc()
	return i.cmd.Wait()
}

// IsOk returns true if there have been no errors on this instance. You can
// use Instance.Errors to see any errors that have occured.
func (i *Instance) IsOk() bool {
	return len(i.Errors) == 0
}

func (i *Instance) maybeErr(err error) {
	if err != nil {
		i.Errors = append(i.Errors, err)
	}
}

// Play plays the song in the current position in the MPD queue.
func (i *Instance) Play() {
	i.maybeErr(i.cli.Pause(false))
}

// Pause pauses the currently playing song.
func (i *Instance) Pause() {
	i.maybeErr(i.cli.Pause(true))
}

// Next skips the current song.
func (i *Instance) Next() {
	i.maybeErr(i.cli.Next())
}

// Prev goes back to the previous song.
func (i *Instance) Prev() {
	i.maybeErr(i.cli.Previous())
}

// Db returns a list of all URIs in this MPD instance's database.
func (i *Instance) Db() []string {
	res, err := i.cli.GetFiles()
	if err != nil {
		i.Errors = append(i.Errors, err)
		return nil
	}
	return res
}

// Queue returns an array of the songs currently in the queue.
func (i *Instance) Queue() []string {
	attrs, err := i.cli.PlaylistInfo(-1, -1)
	if err != nil {
		i.Errors = append(i.Errors, err)
		return nil
	}
	var result []string
	for _, attr := range attrs {
		result = append(result, attr["file"])
	}
	return result
}

func (i *Instance) QueuePos() int64 {
	attrs, err := i.cli.Status()
	if err != nil {
		i.Errors = append(i.Errors, err)
		return -1
	}
	res, err := strconv.ParseInt(attrs["song"], 10, 64)
	if err != nil {
		i.Errors = append(i.Errors, err)
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

func (i *Instance) PlayState() State {
	attr, err := i.cli.Status()
	if err != nil {
		i.Errors = append(i.Errors, err)
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
func New(ctx context.Context, opts *Options) (*Instance, error) {
	root, err := buildRoot(opts)
	if err != nil {
		return nil, err
	}

	mpdCtx, cancel := context.WithCancel(ctx)
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
		cancel()
		root.cleanup()
		return nil, err
	}

	// Keep re-trying to connect to mpd every mpdConnectBackoff, aborting
	// if mpdConnectMax time units have gone by.
	connectCtx, _ := context.WithTimeout(ctx, mpdConnectMax)
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
		return nil, fmt.Errorf("failed to connect to mpd at %s: %v", root.socket, err)
	}
	if cli == nil {
		panic("backoff did not return an error. This should not happen.")
	}

	if err != nil {
		cancel()
		root.cleanup()
		return nil, err
	}

	return &Instance{
		Addr:   root.socket,
		Stdout: &stdout,
		Stderr: &stderr,

		root:       *root,
		cmd:        cmd,
		cli:        cli,
		cancelFunc: cancel,
	}, nil
}
