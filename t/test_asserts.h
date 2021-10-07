#ifndef __ASHUFFLE_T_TEST_ASSERTS_H__
#define __ASHUFFLE_T_TEST_ASSERTS_H__

#include <iostream>

// Assert that the given status is OK, or print the bad status.
#define ASSERT_OK(x) ASSERT_TRUE((x).ok()) << "Bad status: " << (x)

#endif  // __ASHUFFLE_T_TEST_ASSERTS_H__
