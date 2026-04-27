#ifndef TRACE_MARKER_H
#define TRACE_MARKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_tracing();
void close_tracing();
void write_trace_marker(const char *msg);

#ifdef __cplusplus
}
#endif

#endif
