#include <sstream>

#include "log.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::AllOf;
using ::testing::HasSubstr;

auto location_matchers = std::vector({
    HasSubstr("log_test.cc"),
    HasSubstr("TestBody"),
    HasSubstr("test message"),
});

TEST(LogTest, Info) {
    std::stringstream out;
    log::SetOutput(out);
    Log().Info("test message");

    // 22 is the line number we expect to be logged.
    EXPECT_THAT(out.str(), AllOf(HasSubstr("INFO"), HasSubstr("22"),
                                 AllOfArray(location_matchers)));
}

TEST(LogTest, Error) {
    std::stringstream out;
    log::SetOutput(out);
    Log().Error("test message");

    // 32 is the line number we expect to be logged.
    EXPECT_THAT(out.str(), AllOf(HasSubstr("ERROR"), HasSubstr("32"),
                                 AllOfArray(location_matchers)));
}
