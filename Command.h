#pragma once
#include <string>
#include <vector>

// Runs a program with its arguments and waits for it. No shell is involved, so
// network names and passwords from the web page are passed on as they are and
// can never run anything else.
// Returns the exit code, or -1 if the program could not be started or ran
// longer than 'timeout_s' seconds (it is then killed). Its output (stdout and
// stderr) goes to 'output'.
int run_command(const std::vector<std::string>& argv, std::string& output, int timeout_s = 10);
