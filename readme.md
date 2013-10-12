ashuffle
========

Table of Contents:
* features
    * usage
    * help text
    * inclusions and exclusions
* dependencies
* building

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

## help text

```
usage: ashuffle -h [-e PATTERN ...] [-o NUMBER] [-f FILENAME]

Optional Arguments:
   -e,--exclude  Specify things to remove from shuffle (think blacklist).
   -o,--only     Instead of continuously adding songs, just add 'NUMBER'
                 songs and then exit.
   -h,-?,--help  Display this help message.
   -f,--file     Use MPD URI's found in 'file' instead of using the entire MPD
                 library. You can supply `-` instead of a filename to retrive
                 URI's from standard in. This can be used to shuffle over songs
                 piped in from an external matching program.
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

First clone the repository into a directory, and then run:

    make

This should generate the 'ashuffle' binary. Drop it anywhere in your
path and you should be good to go.

Oh, and in the case you're wondering why it's called 'ashuffle' it's
because it implements 'automatic shuffle' mode for mpd.

  [1]: https://github.com/Joshkunz/binfiles/blob/4a4e9b7c845b59ba1c0b68edc84e6cf1972dbc73/ashuffle
