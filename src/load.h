#ifndef __ASHUFFLE_LOAD_H__
#define __ASHUFFLE_LOAD_H__

#include <istream>
#include <string>
#include <string_view>
#include <vector>

#include "mpd.h"
#include "rule.h"
#include "shuffle.h"
#include "util.h"

namespace ashuffle {

// Loader is the abstract interface for objects that are capable of loading
// songs into a shuffle chain from various sources.
class Loader {
   public:
    virtual ~Loader(){};
    virtual void Load(ShuffleChain* into) = 0;
};

class MPDLoader : public Loader {
   public:
    ~MPDLoader() override = default;
    MPDLoader(mpd::MPD* mpd, const std::vector<Rule>& ruleset)
        : mpd_(mpd), rules_(ruleset){};

    void Load(ShuffleChain* into) override;

   protected:
    virtual bool Verify(const mpd::Song&);

   private:
    mpd::MPD* mpd_;
    const std::vector<Rule>& rules_;
};

class FileMPDLoader : public MPDLoader {
   public:
    ~FileMPDLoader() override = default;
    FileMPDLoader(mpd::MPD* mpd, const std::vector<Rule>& ruleset,
                  std::istream* file);

   protected:
    bool Verify(const mpd::Song&) override;

   private:
    std::istream* file_;
    std::vector<std::string> valid_uris_;
};

class FileLoader : public Loader {
   public:
    ~FileLoader() override = default;
    FileLoader(std::istream* file) : file_(file){};

    void Load(ShuffleChain* into) override;

   private:
    std::istream* file_;
};

}  // namespace ashuffle

#endif  // __ASHUFFLE_LOAD_H__
