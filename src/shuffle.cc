#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

#include "shuffle.h"

namespace ashuffle {

void ShuffleChain::Clear() {
    _window.clear();
    _pool.clear();
}

void ShuffleChain::Add(std::string val) { _pool.push_back(val); }

unsigned ShuffleChain::Len() { return _window.size() + _pool.size(); }

/* ensure that our window is as full as it can possibly be. */
void ShuffleChain::FillWindow() {
    while (_window.size() <= _max_window && _pool.size() > 0) {
        /* push a random song from the pool onto the end of the window */
        unsigned idx = rand() % _pool.size();
        _window.push_back(_pool[idx]);
        _pool.erase(_pool.begin() + idx);
    }
}

std::string ShuffleChain::Pick() {
    assert(Len() != 0 && "cannot pick from empty chain");
    FillWindow();
    std::string picked = _window[0];
    _window.pop_front();
    _pool.push_back(picked);
    return picked;
}

std::vector<std::string> ShuffleChain::Items() {
    std::vector<std::string> items;
    items.insert(items.end(), _window.begin(), _window.end());
    items.insert(items.end(), _pool.begin(), _pool.end());
    return items;
}

}  // namespace ashuffle
