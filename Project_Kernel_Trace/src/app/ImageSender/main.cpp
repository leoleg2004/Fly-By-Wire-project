#include "ImageSender.hpp"

int main(int argc, char *argv[]) {
  init_tracing();

  ImageSender component;
  component.start();

  close_tracing();
  return 0;
}
