#ifndef __ASHUFFLE_SHUFFLE_H__
#define __ASHUFFLE_SHUFFLE_H__

#include <deque>
#include <string>
#include <vector>

namespace ashuffle {

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

    // Items returns a vector of all items in this chain. This operation is
    // extremely heavyweight, since it copies most of the storage used by
    // the chain. Use with caution.
    std::vector<std::string> Items();

   private:
    void FillWindow();

    unsigned _max_window;
    std::deque<std::string> _window;
    std::deque<std::string> _pool;
};

}  // namespace ashuffle

#endif
