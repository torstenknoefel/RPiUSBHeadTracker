#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

// A unique temporary directory that is removed again when the object goes out of scope.
class TempDir
{
public:
  TempDir()
  {
    static std::atomic<int> counter{ 0 };
    path_ = std::filesystem::temp_directory_path() /
      ("rpiht_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }
  ~TempDir()
  {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  std::filesystem::path file(const std::string& name) const { return path_ / name; }

  std::filesystem::path write(const std::string& name, const std::string& content) const
  {
    auto p = file(name);
    std::ofstream(p) << content;
    return p;
  }

private:
  std::filesystem::path path_;
};

inline std::string read_file(const std::filesystem::path& p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
