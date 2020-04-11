#include "load.h"
#include "args.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

#include "t/mpd_fake.h"

#include <tap.h>

using namespace ashuffle;

void test_MPDLoader_basic() {
    fake::MPD mpd;
    mpd.db.emplace_back("song_a");
    mpd.db.emplace_back("song_b");

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    MPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset);
    loader.Load(&chain);

    cmp_ok(chain.Len(), "==", 2,
           "MPDLoader_basic: 2 songs added to shuffle chain");
}

void test_MPDLoader_filter() {
    fake::MPD mpd;

    mpd.db.push_back(fake::Song("song_a", {{MPD_TAG_ARTIST, "__artist__"}}));
    mpd.db.push_back(
        fake::Song("song_b", {{MPD_TAG_ARTIST, "__not_artist__"}}));
    mpd.db.push_back(fake::Song("song_c", {{MPD_TAG_ARTIST, "__artist__"}}));

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    Rule rule;
    // Exclude all songs with the artist "__not_artist__".
    rule.AddPattern(MPD_TAG_ARTIST, "__not_artist__");
    ruleset.push_back(rule);

    MPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset);
    loader.Load(&chain);
    cmp_ok(chain.Len(), "==", 2,
           "MPDLoader_filter: 2 songs added to shuffle chain");
}

void xfwrite(FILE *f, const std::string &msg) {
    if (!fwrite(msg.data(), msg.size(), 1, f)) {
        perror("couldn't write to file");
        abort();
    }
}

void xfwriteln(FILE *f, std::string msg) {
    msg.push_back('\n');
    xfwrite(f, msg);
}

void test_FileLoader() {
    const unsigned window_size = 3;
    ShuffleChain chain(window_size);

    fake::Song song_a("song_a"), song_b("song_b"), song_c("song_c");

    FILE *f = tmpfile();
    if (f == nullptr) {
        perror("couldn't open tmpfile");
        abort();
    }

    xfwriteln(f, song_a.URI());
    xfwriteln(f, song_b.URI());
    xfwriteln(f, song_c.URI());

    // rewind, so FileLoader can see the URIs we've written.
    rewind(f);

    FileLoader loader(f);
    loader.Load(&chain);

    cmp_ok(chain.Len(), "==", 3, "FileLoader: 3 songs added to shuffle chain");

    // To make sure we parsed the file correctly, pick three songs out of the
    // shuffle chain, and make sure they match the three URIs we wrote. This
    // should be stable because we set a window size equal to the number of
    // song URIs, and sort the URIs we receive from shuffle_pick.
    std::vector<std::string> want = {song_a.URI(), song_b.URI(), song_c.URI()};
    std::vector<std::string> got = {chain.Pick(), chain.Pick(), chain.Pick()};

    std::sort(want.begin(), want.end());
    std::sort(got.begin(), got.end());

    assert(want.size() == window_size &&
           "number of wanted URIs should match the window size");

    ok(want == got, "FileLoader, want == got");

    // tmpfile is automatically cleaned up by the FileLoader, since it calls
    // fclose() on the file for us.
}

void test_CheckFileLoader() {
    // step 1. Initialize the MPD connection.
    fake::MPD mpd;

    // step 2. Build the ruleset, and add an exclusions for __not_artist__
    std::vector<Rule> ruleset;

    Rule artist_match;
    // Exclude all songs with the artist "__not_artist__".
    artist_match.AddPattern(MPD_TAG_ARTIST, "__not_artist__");
    ruleset.push_back(artist_match);

    // step 3. Prepare the shuffle_chain.
    const unsigned window_size = 2;
    ShuffleChain chain(window_size);

    // step 4. Prepare our songs/song list. The song_list will be used for
    // subsequent calls to `mpd_recv_song`.
    fake::Song song_a("song_a", {{MPD_TAG_ARTIST, "__artist__"}});
    fake::Song song_b("song_b", {{MPD_TAG_ARTIST, "__not_artist__"}});
    fake::Song song_c("song_c", {{MPD_TAG_ARTIST, "__artist__"}});
    // This song will not be present in the MPD library, so it doesn't need
    // any tags.
    fake::Song song_d("song_d");

    // When matching songs, ashuffle will first query for a list of songs,
    // and then match against that static list. Only if a song is in the library
    // will it be matched against the ruleset (since matching requires
    // expensive MPD queries to resolve the URI).
    mpd.db.push_back(song_a);
    mpd.db.push_back(song_b);
    mpd.db.push_back(song_c);
    // Don't push song_d, so we can validate that only songs in the MPD
    // library are allowed.
    // mpd.db.push_back(song_d)

    // step 5. Set up our test input file, but writing the URIs of our songs.
    FILE *f = tmpfile();
    if (f == nullptr) {
        perror("couldn't open tmpfile");
        abort();
    }

    xfwriteln(f, song_a.URI());
    xfwriteln(f, song_b.URI());
    xfwriteln(f, song_c.URI());
    // But we do want to write song_d here, so that ashuffle has to check it.
    xfwriteln(f, song_d.URI());

    // rewind, so build_songs_file can see the URIs we've written.
    rewind(f);

    // step 6. Run! (and validate)
    CheckFileLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset, f);
    loader.Load(&chain);

    cmp_ok(chain.Len(), "==", 2,
           "build_songs_file_check: 2 songs added to shuffle chain");

    // This check works like the nocheck case, but instead of expecting us
    // to pick all 3 songs that were written into the input file, we only want
    // to pick song_a and song_c which are not excluded by the ruleset
    std::vector<std::string> want = {song_a.URI(), song_c.URI()};
    std::vector<std::string> got = {chain.Pick(), chain.Pick()};

    std::sort(want.begin(), want.end());
    std::sort(got.begin(), got.end());

    assert(want.size() == window_size &&
           "number of wanted URIs should match the window size");

    ok(want == got, "build_songs_file_nocheck, want == got");

    // cleanup.
    fclose(f);
}

int main() {
    plan(NO_PLAN);

    test_MPDLoader_basic();
    test_MPDLoader_filter();
    test_FileLoader();
    test_CheckFileLoader();

    done_testing();
}
