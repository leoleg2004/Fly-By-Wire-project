#ifndef TRACE_MARKER_H
#define TRACE_MARKER_H

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Funzioni vuote per non far crashare il main
static inline void init_tracing() {}
static inline void close_tracing() {}

static inline void write_trace_marker(const char *msg) {
  // Variabile indipendente per ogni file che include questa libreria
  static int trace_fd = -1;

  // Se il file è chiuso, lo apre!
  if (trace_fd < 0) {
    trace_fd = open("/sys/kernel/tracing/trace_marker", O_WRONLY);
    if (trace_fd < 0) {
      trace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
    }
  }

  // Se l'ha aperto con successo, scrive il marker
  if (trace_fd >= 0) {
    write(trace_fd, msg, strlen(msg));
  }
}

#endif
