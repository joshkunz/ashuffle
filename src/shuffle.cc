#include <assert.h>
#include <stdlib.h>
#include <iostream>

#include "list.h"
#include "shuffle.h"

void ShuffleChain::Empty() {
    _window.clear();
    _pool.clear();
}

void ShuffleChain::Add(std::string val) {
    _pool.push_back(val);
}

unsigned ShuffleChain::Len() {
    return _window.size() + _pool.size();
}

/* ensure that our window is as full as it can possibly be. */
void ShuffleChain::FillWindow() {
    while (_window.size() <= _max_window && _pool.size() > 0) {
        /* push a random song from the pool onto the end of the window */
        unsigned idx = rand() % _pool.size();
        _window.push_back(_pool[idx]);
        _pool.erase(_pool.begin()+idx);
    }
}

std::string ShuffleChain::Pick() {
    if (Len() == 0) {
        std::cerr << "shuffle_pick: cannot pick from empty chain." << std::endl;
        abort();
    }
    FillWindow();
    std::string picked = _window[0];
    _window.pop_front();
    _pool.push_back(picked);
    return picked;
}

void ShuffleChain::LegacyUnsafeItems(struct list *out) {
    assert(out != NULL && "output list must not be null");
    assert(out->length == 0 && "output list must be empty");

    for (auto it = _window.begin(); it != _window.end(); it++) {
        list_push_str(out, it->data());
    }
    for (auto it = _pool.begin(); it != _pool.end(); it++) {
        list_push_str(out, it->data());
    }
}
