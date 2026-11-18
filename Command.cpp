#include "Command.h"

#include <cerrno>
#include <chrono>
#include <csignal>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

int run_command(const std::vector<std::string>& argv, std::string& output, int timeout_s)
{
  output.clear();
  if (argv.empty())
    return -1;

  std::vector<char*> args;
  for (const auto& a : argv)
    args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);

  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0)
    return -1;

  const pid_t pid = fork();
  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return -1;
  }
  if (pid == 0) {
    // Child: only async-signal-safe calls until exec
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    const int null = open("/dev/null", O_RDONLY);
    if (null >= 0)
      dup2(null, STDIN_FILENO);
    execvp(args[0], args.data());
    _exit(127);
  }

  close(fds[1]);
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  bool timed_out = false;
  char buffer[4096];
  while (true) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - std::chrono::steady_clock::now()).count();
    if (left <= 0) {
      timed_out = true;
      break;
    }
    pollfd p{ fds[0], POLLIN, 0 };
    const int r = poll(&p, 1, int(left));
    if (r < 0 && errno == EINTR)
      continue;
    if (r <= 0) {
      timed_out = (r == 0);
      break;
    }
    const ssize_t n = read(fds[0], buffer, sizeof(buffer));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      break;  // end of output: the program has finished (or closed it)
    output.append(buffer, size_t(n));
  }
  close(fds[0]);

  if (timed_out)
    kill(pid, SIGKILL);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  if (timed_out || !WIFEXITED(status))
    return -1;
  return WEXITSTATUS(status);
}
