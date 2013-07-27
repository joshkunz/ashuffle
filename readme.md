ashuffle
========

ashuffle is a C re-implementation of my [ashuffle python script][1],
an application for automatically shuffling your mpd library in a similar
way to a more standard music player's "shuffle library" feature.

ashuffle has two modes. The first one (and the simpler of the two) is
to simply queue some number of songs randomly selected from your mpd
library. To do this you simply run:

    $ ashuffle 10   # ashuffle <number of songs to add>

which will add 10 random songs to your queue.

In the second (more interesting) mode, ashuffle will wait
until the last song in the queue is playing, at which point it will
add another song to the queue. In practice this creates a 'stream of music'
experience where songs will be continuously played, at random, to infinity.
Also, since ashuffle only adds one song at a time, and only adds that song 
once the last song in the playlist has started playing, you still retain
control over your queue. This way, you can add some song you want to hear
to the queue, and the random songs will simply continue afterwards.

Additionally, ashuffle uses mpd's idle functionality so it won't
drain cpu polling to check if the current song has advanced.

To use the second mode, run ashuffle without any arguments:

    $ ashuffle

## dependencies  

The only dependency is 'libmpdclient' which, you can probably
install via your package manager 

## building

First clone the repository into a directory, and then run:

    make

this should generate the 'ashuffle' binary. Drop it anywhere in your
path and you should be good to go.

Oh, and in the case you're wondering why it's called 'ashuffle' it's
because it implements 'automatic shuffle' mode for mpd.

  [1]: https://github.com/Joshkunz/binfiles/blob/4a4e9b7c845b59ba1c0b68edc84e6cf1972dbc73/ashuffle
