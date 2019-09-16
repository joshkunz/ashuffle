ashuffle
========

[![Build Status](https://travis-ci.org/joshkunz/ashuffle.svg?branch=master)](https://travis-ci.org/joshkunz/ashuffle)
[![GitHub](https://img.shields.io/github/license/joshkunz/ashuffle?color=informational)](LICENSE)

**Notice:** [ashuffle-v2 has just been released!](https://github.com/joshkunz/ashuffle/releases/tag/v2.0.0)
Check out the release for a full description of the changes. Since this is a
major version, it contains a couple backwards incompatible changes:

* `Makefile` support has been fully removed. All builds must now be done
   using meson. See [getting-ashuffle](#getting-ashuffle) below for
   instructions on how to install ashuffle.
* `--nocheck` has been renamed to `--no-check` to be consistent with
  the other command line flags.

If you would like to be notified before another major version change in the
future, subscribe to the [new ashuffle "Announce" list][announce] (the Google
group also has RSS feeds if you would prefer).

---

Table of Contents:
* [features](#features)
  * [usage](#usage)
  * [help text](#help-text)
  * [patterns](#patterns)
  * [shuffle algorithm](#shuffle-algorithm)
  * [mpd version support](#mpd-version-support)
* [getting ashuffle](#getting-ashuffle)
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
  * MPD authentication
  * Crossfade support using `--queue-buffer`.

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

## help text

```
usage: ashuffle [-h] [-n] [-e PATTERN ...] [-o NUMBER] [-f FILENAME] [-q NUMBER]

Optional Arguments:
   -h,-?,--help      Display this help message.
   -e,--exclude      Specify things to remove from shuffle (think blacklist).
   -f,--file         Use MPD URI's found in 'file' instead of using the entire
                     MPD library. You can supply `-` instead of a filename to
                     retrive URI's from standard in. This can be used to pipe
                     song URI's from another program into ashuffle.
   --host            Specify a hostname or IP address to connect to. Defaults
                     to `localhost`.
   -n,--no-check     When reading URIs from a file, don't check to ensure that
                     the URIs match the given exclude rules. This option is most
                     helpful when shuffling songs with -f, that aren't in the
                     MPD library.
   -o,--only         Instead of continuously adding songs, just add 'NUMBER'
                     songs and then exit.
   -p,--port         Specify a port number to connect to. Defaults to `6600`.
   -q,--queue-buffer Specify to keep a buffer of `n` songs queued after the
                     currently playing song. This is to support MPD features
                     like crossfade that don't work if there are no more
                     songs in the queue.
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

Multiple `--exclude` flags can be given. If a song matches any exlude pattern,
it will be excluded. For example, if I wanted to exclude songs by MGMT and
songs by the Arctic Monkeys, I could write:

    $ ashuffle --exclude artist MGMT --exclude artist arctic

MPC and the `-f` flag can be used with `--exclude` to shuffle over more
complex matches. For
example, if we wanted to listen to only songs by Girl Talk *except* the Secret
Diary album, we could use `mpc` to generate a list of Girl Talk songs and then
use an `--exclude` statement to filter out the Secret Diary album:

    $ mpc search artist "Girl Talk" | ashuffle --exclude album "Secret Diary" --file -

## shuffle algorithm

ashuffle uses a fairly unique algorithm for shuffling songs.
Most applications fall into one of two camps:

  * **true random shuffle:** With true random shuffle, no restrictions are
    placed on what songs can be selected for play. It's possible that a
    single song could be played two or even three times in a row because
    songs are just being draw out of a hat.
  * **random list shuffle:** With 'random list' shuffle, songs to be shuffled
    are organized into a list of songs behind the scenes. This list is then
    scrambled (imagine a deck of cards), and then the scrambled playlist is
    played like normal. Using this method songs won't be played twice in a row,
    but the once the playlist has been played it will either loop (playing the
    same random set again), or be re-scrambled and played again, so it can
    still get repetitive. Also, since there's no chance that a song can be
    played again, it won't *feel* very random, especially when listening for
    a long time. I often start noticing song order once the random-list
    wraps around.

ashuffle's approach is an attempt at a happy medium between these two approaches.
Essentially, it keeps two lists of songs, a 'pool' of the songs it's shuffling,
and a 'window' which is a short, ordered, playlist of songs. When the program
starts, ashuffle builds the window randomly by taking songs out of the pool,
and adding them to the window. When a new random song is added to the MPD
queue, the 'top' song of the window, is taken off, added to the queue, and
then put back into the pool. Then another song is added to the window
so that the next request can be fulfilled. This ensures that no songs are
repeated (every song in the window is unique), but you also don't have to
listen to every song in your library before a song comes up again. That is
great for people like me, who use the "skip" button to jump over songs
we're not interested in right now.

## MPD version support

ashuffle aims to be compatible with several versions of MPD, and libmpdclient,
so users don't have to bend-over-backwards to get ashuffle to work.
Specifically, ashuffle aims to be compatible with the latest MPD/libmpdclient
release, as well as all MPD/libmpdclient versions used in active Ubuntu
releases. If you have an issue using ashuffle with any of these versions,
please open an issue.

# getting ashuffle

ashuffle is officially distributed via its source. Users are expected to
build the software themselves. ashuffle has been designed to make building from
source as easy as possible. If you would rather install using a pre-made
package, check below for some 3rd-party packages.

## installing from source

### dependencies

The only dependency is 'libmpdclient' which, you can probably
install via your package manager. For example on debian based
distributions (like ubunutu) use:

    sudo apt-get install libmpdclient-dev

or on OS X using brew:

    brew install libmpdclient

ashuffle is built using `ninja`, and the meson build system, you can obtain meson
by following the instruction's on
[meson's site](https://mesonbuild.com/Getting-meson.html). Ninja is available
on most distributions. On debian-based distributions (including ubuntu) it
can be installed like so:

    sudo apt-get install ninja-build

### building

Download the [latest release][latest], untar/unzip it and then cd into the
source directory and run:

    meson build

Then run

    ninja -C build install

to install the binary. If you want to use a prefix other than `/usr/local` you
can supply an alternate by running `meson` like so:

    meson build --prefix <prefix>

You can uninstall the program later by running

    sudo ninja -C build uninstall

**Note:** See meson's [documentation][2] for more information.

Oh, and in case you're wondering why it's called 'ashuffle' it's
because it implements 'automatic shuffle' mode for mpd.

## third party repositories

These repositores are not maintained by ashuffle. I cannot vouch for any of
them, your mileage may vary.

  * AUR (Arch linux): [ashuffle-git](https://aur.archlinux.org/packages/ashuffle-git/)

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
  [latest]: https://github.com/Joshkunz/ashuffle/releases/tag/v2.0.0
  [announce]: https://groups.google.com/a/ashuffle.app/forum/#!forum/announce
  [users]: https://groups.google.com/a/ashuffle.app/forum/#!forum/users
