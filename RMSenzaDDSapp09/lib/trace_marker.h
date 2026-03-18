#ifndef TRACE_MARKER_H
#define TRACE_MARKER_H

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int ftrace_fd = -1;

static inline void init_tracing() {
  ftrace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
  if (ftrace_fd < 0) {
    printf("Notice: Could not open trace_marker. Tracing markers will be "
           "disabled.\n");
  }
}

static inline void write_trace_marker(const char *msg) {
  if (ftrace_fd >= 0) {
    // Unbuffered direct string write into the ftrace ring-buffer
    write(ftrace_fd, msg, strlen(msg));
  }
}

static inline void close_tracing() {
  if (ftrace_fd >= 0) {
    close(ftrace_fd);
    ftrace_fd = -1;
  }
}

#endif // TRACE_MARKER_H
