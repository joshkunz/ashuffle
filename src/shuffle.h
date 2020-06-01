#ifndef __ASHUFFLE_SHUFFLE_H__
#define __ASHUFFLE_SHUFFLE_H__

#include <deque>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace ashuffle {

class ShuffleChain;

class ShuffleItem {
   public:
    template <typename T>
    ShuffleItem(T v) : ShuffleItem(std::vector<std::string>{v}){};
    ShuffleItem(std::vector<std::string> uris) : _uris(uris){};

   private:
    std::vector<std::string> _uris;
    friend class ShuffleChain;
};

class ShuffleChain {
   public:
    // By default, create a new shuffle chain with a window-size of 1.
    ShuffleChain() : ShuffleChain(1){};

    // Create a new ShuffleChain with the given window length.
    explicit ShuffleChain(size_t window) : _max_window(window) {
        std::random_device rd;
        _rng.seed(rd());
    }

    // Create a new ShuffleChain with the given window length
    // and using the given RandomNumberEngine
    ShuffleChain(size_t window, std::mt19937 rng)
        : _max_window(window), _rng(rng) {}

    // Clear this shuffle chain, removing anypreviously added songs.
    void Clear();

    // Add a string to the pool of songs that can be picked out of this
    // chain.
    void Add(ShuffleItem i);

    // Return the total number of Items (groups) in this chain.
    size_t Len();

    // Return the total number of URIs in this chain, in all items.
    size_t LenURIs();

    // Pick a group of songs out of this chain.
    const std::vector<std::string>& Pick();

    // Items returns a vector of all items in this chain. This operation is
    // extremely heavyweight, since it copies most of the storage used by
    // the chain. Use with caution.
    std::vector<std::vector<std::string>> Items();

   private:
    void FillWindow();

    size_t _max_window;
    std::vector<ShuffleItem> _items;
    std::deque<size_t> _window;
    std::deque<size_t> _pool;
    std::mt19937 _rng;
};

}  // namespace ashuffle

#endif
