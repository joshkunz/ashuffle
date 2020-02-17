#ifndef __ASHUFFLE_SHUFFLE_H__
#define __ASHUFFLE_SHUFFLE_H__

#include <deque>
#include <string>

class ShuffleChain {
   public:
    // By default, create a new shuffle chain with a window-size of 1.
    ShuffleChain() : ShuffleChain(1){};

    // Create a new ShuffleChain with the given window length.
    ShuffleChain(unsigned window) : _max_window(window){};

    // Empty this shuffle chain, removing anypreviously added songs.
    void Empty();

    // Add a string to the pool of songs that can be picked out of this
    // chain.
    void Add(std::string);

    // Return the total length (window + pool) of this chain.
    unsigned Len();

    // Pick a random song out of this chain.
    std::string Pick();

    // Fill the given list `out` with references to all the songs in this
    // chain.
    void LegacyUnsafeItems(struct list *out);

   private:
    void FillWindow();

    unsigned _max_window;
    std::deque<std::string> _window;
    std::deque<std::string> _pool;
};

#endif
