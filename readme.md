ashuffle
========

> __Maintainer's Note:__ Even though there have been no new commits on this repo in over a year, it is not dead, it just isn't broken yet. I actively maintain this repo, and use this software daily. If you find any bugs, please open an issue.

Table of Contents:
* [features](#features)
    * [usage](#usage)
    * [help text](#help-text)
    * [patterns](#patterns)
* [dependencies](#dependencies)
* [building](#building)

# features

ashuffle is a C re-implementation of my [ashuffle python script][1],
an application for automatically shuffling your mpd library in a similar
way to a more standard music player's "shuffle library" feature.

## usage

ashuffle has two modes. The first one (and the simpler of the two) is
to simply queue some number of songs randomly selected from your mpd
library. To do this you simply run:

    $ ashuffle --only 10   # ashuffle --only <number of songs to add>

which will add 10 random songs to your queue.

In the second (more interesting) mode, ashuffle will wait
until the last song in the queue has finished playing, at which point it will
add another song to the queue. In this creates a 'stream of music'
where songs will be continuously played, at random, to infinity.
Additionally, since ashuffle only adds one song at a time, and only adds that song 
once the last song in the playlist has finished playing, you still retain
control over your queue. This way, you can add some song you want to hear
to the queue, and the random songs will simply continue afterwards.

Additionally, ashuffle uses mpd's idle functionality so it won't
drain cpu polling to check if the current song has advanced.

To use the second mode, run ashuffle without the `--only` argument.

    $ ashuffle

### running in a non-standard configuration

If you're running MPD on a non-standard port or on a different machine, ashuffle
will respect the standard `MPD_HOST` and `MPD_PORT` environment variables to
set the host and port mpd is listening on respectively.

Also following standard MPD tools, a password can be supplied in the `MPD_HOST`
environment variable by putting an `@` between the password and the hostname.

For example, one can run ashuffle as follows:

    $ env MPD_HOST="<password>@<hostname>" MPD_PORT="<port>" ashuffle ...

Or without the password by just omitting the `<password>` and `@` from the
`MPD_HOST` variable.

### shuffling from files and `--nocheck`

By supplying the `-f` option and a file containing a list of song URIs to
shuffle, you can make ashuffle shuffle and arbitrary list of songs. For
example, by passing `-f -` to ashuffle, you can have it shuffle over songs
passed to it via standard in:

    $ mpc search artist "Girl Talk" | ashuffle -f -

As explained in more detail below, if song URIs are passed to ashuffle using
this mechanism, ashuffle will still try to apply exclusion rules to these
songs. If the song URIs you want ashuffle to shuffle over do not exist
in your MPD library (for example if you are trying to shuffle URIs with the
`file://` schema), ashuffle will exclude them by default. If you pass the
`--nocheck` option to ashuffle, it will not apply the filtering rules, allowing
you to shuffle over songs that are not in your library.

## help text

```
usage: ashuffle -h -n [-e PATTERN ...] [-o NUMBER] [-f FILENAME]

Optional Arguments:
   -e,--exclude  Specify things to remove from shuffle (think blacklist).
   -o,--only     Instead of continuously adding songs, just add 'NUMBER'
                 songs and then exit.
   -h,-?,--help  Display this help message.
   -f,--file     Use MPD URI's found in 'file' instead of using the entire MPD
                 library. You can supply `-` instead of a filename to retrive
                 URI's from standard in. This can be used to pipe song URI's
                 from another program into ashuffle.
   -n,--nocheck  When reading URIs from a file, don't check to ensure that
                 the URIs match the given exclude rules. This option is most
                 helpful when shuffling songs with -f, that aren't in the
                 MPD library.
See included `readme.md` file for PATTERN syntax.
```

## patterns

Patterns are a list of key-value pairs given to the `--exclude` flag. A pair is 
composed of a 'field' and a 'value'. A field is the name 
of an MPD tag (e.g. artist, title, album) to match on (case insensitive) and
'value' is a string to match against that field. So, if I wanted to exclude
MGMT's album 'Congratulations' in  the shuffle I could supply a command
line like the following:

    $ ashuffle --exclude artist MGMT album "Congratulations"

Since typing in an exact match for all songs could become quite cumbersome, the 
'value' field will match on substrings, so you only have to specify part of the
search string. For example, if we wanted to match Arctic Monkeys album
'Whatever People Say I Am, That's What I'm Not' we could shorten that to this:

    $ ashuffle --exclude artist arctic album whatever

Multiple `--exclude` flags can be given, the AND result 
of all flags will be used to match a given song. For example, if we wanted to 
exclude songs by MGMT and songs by the Arctic Monkeys, we 
could write:

    $ ashuffle --exclude artist MGMT --exclude artist arctic

Additionally, the `-f` flag can be used to shuffle over more complex matches. For
example, if we wanted to listen to only songs by Girl Talk *except* the Secret
Diary album, we could use `mpc` to generate a list of Girl Talk songs and then
use a `--exclude` statement to filter out the Secret Diary album:

    $ mpc search artist "Girl Talk" | ashuffle --exclude album "Secret Diary" --file -

## shuffle algorithm

Currently ashuffle is using a fairly unique algorithm for shuffling songs.
Most applications fall into two camps: true random shuffle, and 'random list'
shuffle. With true random shuffle, no restrictions are placed on what songs
can be selected for play. It's possible that a single song could be played
two or even three times in a row because songs are just being draw out
of a hat. With 'random list' shuffle, songs to be shuffled are organized into
a list of songs behind the scenes. This list is then scrambled, and then played
like a normal playlist. Using this method songs won't be played twice in a row,
but the once the playlist has been played it will either loop (playing the same
random set again), or be re-scrambled and played again, so it can still
get repetitive. Also, since there's no chance that a song can be played again,
it won't *feel* very random, especially when listening for a long time. I often
start noticing song order once the random-list wraps around.

ashuffle's approach is an attempt at a happy medium between these two approaches.
Essentially, it keeps two lists of songs, a 'pool' of the songs it's shuffling,
and a 'window' which is a short, ordered, playlist of songs. When the program
starts, ashuffle builds the window randomly by taking songs out of the pool, 
and adding them to the window. When a new random song is added to the MPD
queue, the 'top' song of the window, is taken off, added to the queue, and 
then put back into the pool. Then another song is added to the window
so that the next request can be fulfilled.

# dependencies  

The only dependency is 'libmpdclient' which, you can probably
install via your package manager. For example on debian based
distributions:

    sudo apt-get install libmpdclient-dev

or on OS X using brew:

    brew install libmpdclient

# building

Download the [latest release][latest], untar/unzip it and then cd into the
directory and run:

    make

Then run

    sudo make install

to install the binary. If you want to use a prefix other than `/usr/local` you
can supply an alternate by running `make install` like so:

    sudo make prefix=<prefix> install

You can uninstall the program later by running

    sudo make uninstall

**Note:**  You'll have to supply the prefix again if you used a custom prefix.

Oh, and in the case you're wondering why it's called 'ashuffle' it's
because it implements 'automatic shuffle' mode for mpd.

  [1]: https://github.com/Joshkunz/binfiles/blob/4a4e9b7c845b59ba1c0b68edc84e6cf1972dbc73/ashuffle
  [latest]: https://github.com/Joshkunz/ashuffle/releases/tag/v1.0.1
