#include "ImageSubscriber.hpp"

int main(int argc, char *argv[]) {
  init_tracing();

  ImageSubscriber component;
  component.start();

  close_tracing();
  return 0;
}
