#include "pyMainThreadInterface.hpp"

#include "py.h"
#include "subscaleMain.hpp"

#include <sys/wait.h>

#include <errno.h>

pid_t lastForkedPID;
bool lastForkedPIDValid = false;
std::mutex lastForkedPIDM;

bool runCommandWithFork(const char* commandWithArgs[] /* array with NULL as the last element */) {
  const char* command = commandWithArgs[0];
  printf("Forking to run command \"%s\"\n", command);
  pid_t pid = fork(); // create a new child process
  if (pid > 0) { // Parent process
    lastForkedPIDM.lock();
    lastForkedPID=pid;
    lastForkedPIDValid=true;
    lastForkedPIDM.unlock();
    int status = 0;
    int wpid;
    // https://stackoverflow.com/questions/10160583/fork-multiple-processes-and-system-calls , https://stackoverflow.com/questions/39329540/wait-returns-0-and-errno-interrupted-system-call
    do
      {
        wpid = wait(&status);
      }
    while (wpid == -1 && errno == EINTR);
    if (wpid != -1) {
      if (WIFEXITED(status)) {
       // now check to see what its exit status was
       printf("The exit status for command \"%s\" was: %d\n", command, WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
       // it was killed by a signal
       printf("The signal that killed command \"%s\" was %d\n", command, WTERMSIG(status));
      }
    } else {
      printf("Error waiting for command \"%s\"!\n", command);
      lastForkedPIDM.lock();
      lastForkedPIDValid=false;
      lastForkedPIDM.unlock();
      return false;
    }
    lastForkedPIDM.lock();
    lastForkedPIDValid=false;
    lastForkedPIDM.unlock();
  } else if (pid == 0) { // Child process
    execvp((char*)command, (char**)commandWithArgs); // one variant of exec
    printf("Failed to run execvp to run command \"%s\": %s\n", command, strerror(errno)); // Will only print if error with execvp.
    return false;
  } else {
    printf("Error with fork for command \"%s\": %s\n", command, strerror(errno));
    return false;
  }
  return true;
}

bool pyRunFile(const char *path, int argc, char **argv) {
  mainDispatchQueue.enqueue([=](){
    //PyGILState_STATE state = PyGILState_Ensure(); // Only run this if no other python code is running now, otherwise wait for a lock // TODO: implement properly

    reportStatus(Status::RunningPython);
    S_RunFile(path, argc, argv);

    //PyGILState_Release(state); // TODO: implement properly
  },path,QueuedFunctionType::Python);

  return true;
}
