#ifndef __ASHUFFLE_GETPASS_H__
#define __ASHUFFLE_GETPASS_H__

#include <cstdio>
#include <string>
#include <string_view>

// GetPass obtains a password from the user. It writes the given prompt to
// `out_stream` and then waits for the user to type a line on `in_stream`
// which is then returned. Terminal echoing is disabled while the user is
// writing their password, to add additional privacy.
std::string GetPass(FILE *in_stream, FILE *out_stream, std::string_view prompt);

#endif
