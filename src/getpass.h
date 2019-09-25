#ifndef __GETPASS_H__
#define __GETPASS_H__

#include <stdio.h>

char *as_getpass(FILE *in_stream, FILE *out_stream, const char *prompt);

#endif
