#include <cassert>
#include <cstdlib>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "shuffle.h"

namespace ashuffle {

void ShuffleChain::Clear() {
    _window.clear();
    _pool.clear();
    _items.clear();
}

void ShuffleChain::Add(ShuffleItem item) {
    _items.emplace_back(item);
    _pool.push_back(_items.size() - 1);
}

size_t ShuffleChain::Len() { return _items.size(); }
size_t ShuffleChain::LenURIs() {
    size_t sum = 0;
    for (auto& group : _items) {
        sum += group._uris.size();
    }
    return sum;
}

/* ensure that our window is as full as it can possibly be. */
void ShuffleChain::FillWindow() {
    while (_window.size() <= _max_window && _pool.size() > 0) {
        std::uniform_int_distribution<unsigned long long> rd{0,
                                                             _pool.size() - 1};
        /* push a random song from the pool onto the end of the window */
        size_t idx = rd(_rng);
        _window.push_back(_pool[idx]);
        _pool.erase(_pool.begin() + idx);
    }
}

const std::vector<std::string>& ShuffleChain::Pick() {
    assert(Len() != 0 && "cannot pick from empty chain");
    FillWindow();
    size_t picked_idx = _window[0];
    _window.pop_front();
    _pool.push_back(picked_idx);
    return _items[picked_idx]._uris;
}

std::vector<std::vector<std::string>> ShuffleChain::Items() {
    std::vector<std::vector<std::string>> result;
    for (auto group : _items) {
        result.push_back(group._uris);
    }
    return result;
}

}  // namespace ashuffle
