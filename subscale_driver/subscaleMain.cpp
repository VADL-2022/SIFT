#include "subscaleMain.hpp"

#include <python3.7m/Python.h>
#include "VADL2022.hpp"

Queue<QueuedFunction, 8> mainDispatchQueue;

int main(int argc, char **argv) {
  VADL2022 vadl(argc, argv);

  QueuedFunction f;
  while (true) {
    mainDispatchQueue.dequeue(&f);
    //f();
  }

  //PyRun_SimpleString("input('Done')");
  return 0;
}
