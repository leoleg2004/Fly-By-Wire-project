#ifndef TRACE_MARKER_H
#define TRACE_MARKER_H

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int ftrace_fd = -1;
static int ftrace_live_fd = -1;

static inline void init_tracing() {
  ftrace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
  if (ftrace_fd < 0) {
    printf("Notice: Could not open trace_marker. Tracing markers will be "
           "disabled.\n");
  }

  // Apri parallelamente il trace_marker dell'istanza per il Live Streaming
  ftrace_live_fd =
      open("/sys/kernel/debug/tracing/instances/live_trace_inst/trace_marker",
           O_WRONLY);
  if (ftrace_live_fd < 0) {
    // Nessun avviso critico, l'istanza live potrebbe essere spenta
  }
}

static inline void write_trace_marker(const char *msg) {
  if (ftrace_fd >= 0) {
    write(ftrace_fd, msg, strlen(msg));
  }
  if (ftrace_live_fd >= 0) {
    write(ftrace_live_fd, msg, strlen(msg));
  }
}

static inline void close_tracing() {
  if (ftrace_fd >= 0) {
    close(ftrace_fd);
    ftrace_fd = -1;
  }
  if (ftrace_live_fd >= 0) {
    close(ftrace_live_fd);
    ftrace_live_fd = -1;
  }
}

#endif // TRACE_MARKER_H
