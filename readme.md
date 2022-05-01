ashuffle
========

[![Test](https://github.com/joshkunz/ashuffle/actions/workflows/test.yaml/badge.svg?branch=master)](https://github.com/joshkunz/ashuffle/actions/workflows/test.yaml)
[![GitHub](https://img.shields.io/github/license/joshkunz/ashuffle?color=informational)](LICENSE)

Table of Contents:
* [features](#features)
  * [usage](#usage)
  * [help text](#help-text)
  * [patterns](#patterns)
  * [shuffle algorithm](#shuffle-algorithm)
  * [mpd version support](#mpd-version-support)
* [getting ashuffle](#getting-ashuffle)
  * [pre-built binaries](#pre-built-binaries)
  * [install from source](#installing-from-source)
    * [dependencies](#dependencies)
    * [building](#building)
  * [third-party repositories](#third-party-repositories)
* [contact](#contact)
* [users](#users)

# features

ashuffle is an application for automatically shuffling your MPD library
in a similar way to a more standard music player's "shuffle library"
feature. ashuffle works like any other MPD client, and can be used alongside
your other MPD frontends.

## usage

In ashuffle's primary mode is to create a 'stream of music'. To do this run:

    $ ashuffle

ashuffle will wait until the last song in the queue has finished playing,
then randomly choose another song from your MPD library, add it to the
queue and continue playing. Songs will be continuously played, at random,
to infinity. Since ashuffle adds only one song at a time, after the last
song in the playlist has _finished_ playing, you still retain control over
your queue. When using ashuffle, more songs can be added to the queue,
and once those songs finish, the random music will resume.

ashuffle uses MPD's "idle" command to listen for MPD events so it won't
drain cpu polling to check if the current song has advanced.

If you only want to enqueue a set number of songs, use the `--only` flag like
this:

    $ ashuffle --only 10   # ashuffle --only <number of songs to add>

This particular command adds 10 random songs to the queue.

In addition to these two basic modes, ashuffle supports many other features
like:

  * Custom shuffle filter rules, using `--exclude`.
  * Shuffling based on a list of MPD URIs, like would be output from
    `mpc search` using the `--file` option.
  * MPD authentication.
  * Crossfade support using `--queue-buffer`.
  * Shuffling by album or other groupings of songs using the `--by-album` or
    `--group-by` option.

If any of these sound interesting, read on!

### running in a non-standard configuration

When running MPD on a non-standard port or on a remote machine, ashuffle
will respect the standard `MPD_HOST` and `MPD_PORT` environment variables,
used to specify the host and port respectively of the MPD server.

Also following standard MPD tools, a password can be supplied in the `MPD_HOST`
environment variable by putting an `@` between the password and the host name.

For example, one can run ashuffle as follows:

    $ env MPD_HOST="<password>@<hostname>" MPD_PORT="<port>" ashuffle ...

Or without the password by just omitting the `<password>` and `@` from the
`MPD_HOST` variable.

ashuffle also supports providing a host or port via the command line with the
`--host` and `--port` options. `--host`, and `--port`, will override `MPD_HOST`
or `MPD_PORT` if provided.

For example:

    $ ashuffle --host <password>@<hostname> --port <port>

Once again, the password can be omitted.

### shuffling from files and `--no-check`

By supplying the `-f` option and a file containing a list of song URIs to
shuffle, you can make ashuffle use an arbitrary list of songs. For
example, by passing `-f -` to ashuffle, you can have it shuffle over songs
passed to it via standard in:

    $ mpc search artist "Girl Talk" | ashuffle -f -

As explained in more detail below, if song URIs are passed to ashuffle using
this mechanism, ashuffle will still try to apply exclusion rules to these
songs. If the song URIs you want ashuffle to shuffle over do not exist
in your MPD library (for example if you are trying to shuffle URIs with the
`file://` schema), ashuffle will exclude them by default. If you pass the
`--no=check` option to ashuffle, it will not apply the filtering rules, allowing
you to shuffle over songs that are not in your library.

### crossfade support and the `--queue-buffer`

By default, ashuffle will only enqueue another song once the current queue
has ended. This gives the user a lot of control over what will be playing next.
One unfortunate side-effect of this is that it breaks MPD's built-in crossfade
support. If the next song is only added once the previous song has finished
playing, MPD doesn't know how to crossfade between the two songs. As a tradeoff
between queue control and cross-fade support, you can supply the
`--queue-buffer n` flag. This flag will have ashuffle ensure that there are
always `n` songs in the queue after the currently playing song. This way you
still retain some queue control, while making sure that MPD can crossfade
effectively. Most crossfade users will probably want to use this flag like so:

    $ ashuffle --queue-buffer 1

### shuffling by album, or other groups, with `--group-by`

If you'd rather shuffle songs in groups, instead of individually, ashuffle can
group songs by any combination of tag values using the `-g`/`--group-by`
option. For example, you could run:

    $ ashuffle --group-by album

In this mode, when loading songs from MPD or a file, ashuffle will first group
the songs by the given tag. Album in this case. Then, when a song needs to be
added to the queue, ashuffle will pick a random group (e.g., album) and
enqueue all songs from that group. Once the end of the queue is reached, a new
group is picked and the process is repeated.

Multiple tags can be provided to `--group-by`, and songs will be grouped
together as long as all the given tags match. A `--by-album` option is provided
for convenience that is probably what you want to use when shuffling by album.
It's equivalent to `--group-by album date`.

Note that `-g`/`--group-by`/`--by-album` can only be provided once.

### advanced options for specialized preferences, with `--tweak`

Tweaks are infrequently used, specialized, or complicated options that most
users probably don't want to use. These options are all set via a single
`--tweak`/`-t` flag to avoid cluttering help pages. All tweaks have the form
`<key>=<value>`. For example: `--tweak window-size=7`. Here is a table of
tweaks, and their meanings:

| Name | Values | Default | Description |
| ---- | ------ | ------- | ----------- |
| `exit-on-db-update` | Boolean | `no` | If set to a true value, then ashuffle will exit when the MPD database is updated. This can be useful when used in conjunction with the `-f -` option, as it allows you to re-start ashuffle with a new music list. |
| `play-on-startup` | Boolean | `yes` | If set to a true value, ashuffle starts playing music if MPD is paused, stopped, or the queue is empty on startup. If set to false, then ashuffle will not enqueue any music until a song is enqueued for the first time. |
| `suspend-timeout` | Duration `> 0` | `0ms` | Enables "suspend" mode, which may be useful to users that use ashuffe in a workflow where they clear their queue. In this mode, if the queue is cleared while ashuffle is running, ashuffle will wait for `suspend-timeout`. If songs were added to the queue during that period of time (i.e., the queue is no longer empty), then ashuffle suspends itself, and will not add any songs to the queue (even if the queue runs out) until the queue is cleared again, at which point normal operations resume. This was add to support use-cases like the one given in issue #13, where a music player had a "play album" mode that would clear the queue, and then play an album. See below for the duration format. |
| `window-size` | Integer `>=1` | `7` | Sets the size of the "window" used for the shuffle algorithm. See the section on the [shuffle algorithm](#shuffle-algorithm) for more details. In-short: Lower numbers mean more frequent repeats, and higher numbers mean less frequent repeats. |

Value types:

| Name | Representation |
| ---- | -------------- |
| Integer | An integral number, like `-1`, `0`, or `15`. |
| Boolean | The strings `on`, `true`, `yes` or `1` mean "true" or "enable", and the strings `off`, `false`, `no`, or `0` mean "false" or "disable". |
| Duration | A duration of time that can be parsed by [Abseil's `ParseDuration`](https://github.com/abseil/abseil-cpp/blob/c678d6c6bf70d47b4aa5bc3576a3a769775bc162/absl/time/time.h#L548). The general format is `<number><unit>`. `<number>` can be floating point, and negative. `<unit>` is whatever time units Abseil supports, but the expected ones like `ms`, `s`, and `m` should work. Example: `250ms` would represent 250 milliseconds. |

## help text

```
usage: ashuffle [-h] [-n] [-v] [[-e PATTERN ...] ...] [-o NUMBER]
    [-f FILENAME] [-q NUMBER] [-g TAG ...] [[-t TWEAK] ...]

Optional Arguments:
   -h,-?,--help      Display this help message.
   -e,--exclude      Specify things to remove from shuffle (think
                     blacklist). A PATTERN should follow the exclude
                     flag.
   --exclude-from    Read exclude rules from the given file. Rules
                     should be given in the YAML format described in
                     the included readme.md
   -f,--file         Use MPD URI's found in 'file' instead of using the
                     entire MPD library. You can supply `-` instead of a
                     filename to retrive URI's from standard in. This
                     can be used to pipe song URI's from another program
                     into ashuffle.
   --by-album        Same as '--group-by album date'.
   -g,--group-by     Shuffle songs grouped by the given tags. For
                     example 'album' could be used as the tag, and an
                     entire album's worth of songs would be queued
                     instead of one song at a time.
   --host            Specify a hostname or IP address to connect to.
                     Defaults to `localhost`.
   -n,--no-check     When reading URIs from a file, don't check to
                     ensure that the URIs match the given exclude rules.
                     This option is most helpful when shuffling songs
                     with -f, that aren't in the MPD library.
   -o,--only         Instead of continuously adding songs, just add
                     'NUMBER' songs and then exit.
   -p,--port         Specify a port number to connect to. Defaults to
                     `6600`.
   -q,--queue-buffer Specify to keep a buffer of `n` songs queued after
                     the currently playing song. This is to support MPD
                     features like crossfade that don't work if there
                     are no more songs in the queue.
   -t,--tweak        Tweak an infrequently used ashuffle option. See
                     `readme.md` for a list of available options.
   -v,--version      Print the version of ashuffle, and then exit.
See included `readme.md` file for PATTERN syntax.
```

## patterns

Patterns are a list of key-value pairs given to the `--exclude` flag. A pair is
composed of a 'field' and a 'value'. A 'field' is the name
of an MPD tag (e.g. artist, title, album) to match on (case insensitive) and
'value' is a string to match against that field. So, if I wanted to exclude
MGMT's album 'Congratulations' in ashuffle I could supply a command
line like the following:

    $ ashuffle --exclude artist MGMT album "Congratulations"

Since typing in an exact match for all songs could become quite cumbersome, the
'value' field will match on substrings. You only have to specify part of the
search string. For example, if I wanted to match Arctic Monkeys album
'Whatever People Say I Am, That's What I'm Not' I could shorten that to just:

    $ ashuffle --exclude artist arctic album whatever

Multiple `--exclude` flags can be given. If a song matches any exclude pattern,
it will be excluded. For example, if I wanted to exclude songs by MGMT and
songs by the Arctic Monkeys, I could write:

    $ ashuffle --exclude artist MGMT --exclude artist arctic

MPC and the `-f` flag can be used with `--exclude` to shuffle over more
complex matches. For example, if we wanted to listen to only songs by
Girl Talk *except* the Secret Diary album, we could use `mpc` to generate a
list of Girl Talk songs and then use an `--exclude` statement to filter out the
Secret Diary album:

    $ mpc search artist "Girl Talk" | ashuffle --exclude album "Secret Diary" --file -

### exclude patterns from a file

ashuffle allows exclude patterns to be passed via a YAML formatted file with
the following structure:

```yaml
rules:
- <tag1>: <value1>
  <tag...>: <value...>
  <tagN>: <valueN>
- ...
```

Where `<tag>` is replaced with the tag to match on (e.g. `artist`) and `<value>`
is replaced with the value to match (e.g. `arctic`). Tags are matched to values
based on the rules described in the patterns section above: a case-insensitive
substring match. All tag values must match their values for a given rule
(one item of the `rules` list) to match and exclude a track. Rules are not
updated when the underlying rule file changes. `ashuffle` must be re-started
for changes to take effect.

For example, if there was an exclusion rules file `excludes.yaml`, with the
contents:

```yaml
rules:
- artist: arctic
  album: whatever
- artist: MGMT
  album: Congratulations
```

And ashuffle was invoked like:

    $ ashuffle --exclude-from excludes.yaml

Then ashuffle would not include tracks from the album 'Whatever People Say I
Am, That's What I'm Not' by the 'Arctic Monkeys', or the album 'Congratulations'
by the artist 'MGMT' in the pool of songs to shuffle.

## shuffle algorithm

ashuffle uses a fairly unique algorithm for shuffling songs.
Most applications fall into one of two camps:

  * **true random shuffle:** With true random shuffle, no restrictions are
    placed on what songs can be selected for play. It's possible that a
    single song could be played two or even three times in a row because
    songs are just drawn out of a hat.
  * **random list shuffle:** With 'random list' shuffle, songs to be shuffled
    are organized into a list of songs behind the scenes. This list is then
    scrambled (imagine a deck of cards), and then the scrambled playlist is
    played like normal. Using this method songs won't be played twice in a row,
    but the once the playlist has been played it will either loop (playing the
    same random set again), or be re-scrambled and played again, so it can
    still get repetitive. Also, since there's no chance that a song can be
    played again, it won't *feel* very random, especially when listening for
    a long time. Every song has to be played once before any song can be
    repeated. I often start noticing song order once the random-list
    wraps around.

ashuffle's approach is an attempt at a happy medium between these two styles.
It keeps two lists of songs: a 'pool' of the songs it's shuffling,
and a 'window' which is a short, ordered, playlist of songs. When the program
starts, ashuffle builds the window by randomly taking songs out of the pool,
and adding them to the window. When a new random song needs to be added to the
MPD queue, the 'top' song of the window is removed, added to the queue, and
then put back into the pool. Then, another song is taken from the pool and
added to the window so that the next request can be fulfilled. This ensures
that no songs are repeated (every song in the window is unique), but you also
don't have to listen to every song in your library before a song comes up
again. I like this style a lot better, because I can "skip" between songs I
want to listen to.

## MPD version support

ashuffle aims to be compatible with several versions of MPD, and libmpdclient,
so users don't have to bend-over-backwards to get ashuffle to work.
Specifically, ashuffle aims to be compatible with the latest MPD/libmpdclient
release, as well as all MPD/libmpdclient versions used in active Ubuntu
releases. If you have an issue using ashuffle with any of these versions,
please open an issue.

# getting ashuffle

ashuffle is officially distributed via pre-compiled binaries, and via its
source. Linux-compatible binaries are currently available for `x86_64`,
and several ARM flavors that should support most ARM users, including
Raspberry Pi.

## pre-built binaries

First, install 'libmpdclient', the library ashuffle uses to interact with MPD.
It can be obtained from most package managers. E.g. via `sudo apt install
libmpdclient2`, or `brew install libmpdclient`. Once libmpdclient is installed
you can download the latest binary release for your platform [on the releases
page](https://github.com/joshkunz/ashuffle/releases). Binaries are currently
available for the following platforms:

| Binary | Architecture | Minimum CPU | Popular Devices |
| ------ | ------------ | ----------- | --------------- |
| `ashuffle.x86_64-linux-gnu` | `x86_64` || Most Desktops, Laptops, and Servers |
| `ashuffle.aarch64-linux-gnu` | `aarch64` | `cortex-a53` | Raspberry Pi 3B+ running 64-bit OS (not RPi OS) |
| `ashuffle.armv7h-linux-gnueabihf` | `armv7hl` | `cortex-a7` | Raspberry Pi 2B+ Running RPi OS (f.k.a. Raspbian) |
| `ashuffle.armv6h-linux-gnueabihf` | `armv6hl` | `arm1176jzf-s` | Raspberry Pi 0/1+ Running RPi OS (f.k.a. Raspbian) |

Once you've downloaded the binary, it should "just work" when run
(e.g. `$ ./ashuffle.x86_64-linux-gnu`). If they do not, please [file an
issue](https://github.com/joshkunz/ashuffle/issues) or send an email to the
ashuffle users list at `users@ashuffle.app`.

If you'd like to add binary support to another platform, pull
requests are welcome.

## installing from source

For platforms without a binary release, you'll have to build from source. 
ashuffle is designed to have a small number of dependencies, and we try to
keep the build relatively straightforward. That said, you will need a
relatively recent C++ compiler. Clang 7+, or GCC 8+ should work.

If you have any trouble building ashuffle, please file an issue on Github,
or email the ashuffle users group. Make sure to your compiler version, meson
version, the commands you tried to execute, and any errors that were produced. 

### dependencies

The only dependency is 'libmpdclient' which, you can probably
install via your package manager. For example on debian based
distributions (like ubunutu) use:

    sudo apt-get install libmpdclient-dev

or on OS X using brew:

    brew install libmpdclient

ashuffle is built using `ninja`, and the meson build system, you can obtain meson
by following the instruction's on
[meson's site](https://mesonbuild.com/Getting-meson.html). Meson version
`>=0.54.0` is required. Ninja is available on most distributions. On
debian-based distributions (including ubuntu) it can be installed like so:

    sudo apt-get install ninja-build

### building

ashuffle relies on git submodules to track libraries it depends on. These
libraries are not distributed in the source tarballs provided by Github,
so you need to use git to get ashuffle when building from source. 

Start by cloning ashuffle:

    git clone https://github.com/joshkunz/ashuffle.git
    cd ashuffle

The check what the [latest release][latest] is on the releases page, and
checkout the corresponding git tag. For example, If the latest release was
v1.22.3 you would run:

    git checkout v1.22.3

Then init and update the submodules:

    git submodule update --init --recursive

Now you have the source needed to build ashuffle. Next, you need to configure
the build, using meson. Luckily this is easy:

    meson -Dbuildtype=release build

Then run

    ninja -C build install

to build and install the binary. If you want to use a prefix other than
`/usr/local` you can supply an alternate by running `meson` like so:

    meson build -Dbuildtype=release --prefix <prefix>

You can uninstall the program later by running

    sudo ninja -C build uninstall

**Note:** See meson's [documentation][2] for more information on configuration.

Oh, and in case you're wondering why it's called 'ashuffle' it's
because it implements 'automatic shuffle' mode for mpd.

## third party repositories

These repositores are not maintained by ashuffle. I cannot vouch for any of
them, your mileage may vary.

  * AUR (Arch linux): [ashuffle-git](https://aur.archlinux.org/packages/ashuffle-git/)
  * Fedora COPR: [`tokariew/ashuffle`](https://copr.fedorainfracloud.org/coprs/tokariew/ashuffle/)
  * Homebrew (macOS): [`hamuko/mpd/ashuffle`](https://github.com/joshkunz/ashuffle/issues/35#issuecomment-1086713026)
  * MacPorts (macOS): [`ashuffle`](https://ports.macports.org/port/ashuffle/)

# contact

If you find bugs in ashuffle, or have a feature suggestion, please file a
[Github issue](https://github.com/joshkunz/ashuffle/issues). ashuffle also
has two mailing lists you can subscribe to for discussion and announcements:

  * [`announce@ashuffle.app`][announce]: For (very) infrequent announcements
    of major ashuffle releases. Posts only allowed by the project maintiner.
  * [`users@ashuffle.app`][users]: For general ashuffle questions, or
    discussion of ashuffle features. Allows posts from all users.

# users

Below are some projects that use ashuffle. If your are using ashuffle as part
of another project, feel free to open a pull request to include it in this
list.

 * [moOde Audio Player](https://github.com/moode-player/moode)

## related projects

These projects do not use ashuffle directly, but are related to ashuffle in
some way.

  * [ashuffle-rs](https://github.com/palfrey/ashuffle-rs): An experimental
    transpile of ashuffle to Rust:
    https://tech.labs.oliverwyman.com/blog/2018/11/27/experiments-in-converting-code-from-c-to-rust/

  [1]: https://github.com/Joshkunz/binfiles/blob/4a4e9b7c845b59ba1c0b68edc84e6cf1972dbc73/ashuffle
  [2]: https://mesonbuild.com/Quick-guide.html
  [latest]: https://github.com/joshkunz/ashuffle/releases/latest
  [announce]: https://groups.google.com/a/ashuffle.app/forum/#!forum/announce
  [users]: https://groups.google.com/a/ashuffle.app/forum/#!forum/users
