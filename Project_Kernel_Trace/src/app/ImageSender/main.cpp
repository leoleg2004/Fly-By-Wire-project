#include "ImageSender.hpp"
#include <unistd.h>
#include <vector>

int main(int argc, char *argv[]) {
  if (geteuid() != 0) {
    std::vector<char*> new_argv;
    new_argv.push_back((char*)"sudo");
    new_argv.push_back((char*)"LD_LIBRARY_PATH=/usr/local/lib");
    for (int i = 0; i < argc; ++i) {
      new_argv.push_back(argv[i]);
    }
    new_argv.push_back(nullptr);
    execvp("sudo", new_argv.data());
    return 1;
  }
  init_tracing();

  ImageSender component;
  component.start();

  close_tracing();
  return 0;
}
