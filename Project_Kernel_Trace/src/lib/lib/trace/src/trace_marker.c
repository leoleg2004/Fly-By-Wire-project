#include "trace_marker.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int trace_fd = -1;

void init_tracing() {
  trace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
  if (trace_fd == -1) {
    // Fallback or warning
    // perror("Error opening trace_marker");
  }
}

void close_tracing() {
  if (trace_fd != -1) {
    close(trace_fd);
    trace_fd = -1;
  }
}

void write_trace_marker(const char *msg) {
  if (trace_fd != -1) {
    ssize_t ret = write(trace_fd, msg, strlen(msg));
    (void)ret;
  }
}
