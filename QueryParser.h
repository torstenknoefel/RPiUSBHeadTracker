#pragma once

#include <map>
#include <string>
#include <vector>
#include <sstream>

// QueryParser an HTTP query string into a map.
// Based on https://github.com/ipkn/crow/issues/170
class QueryParser {
public:
  std::map<std::string, std::string> jar;

public:
  QueryParser(std::string s) { jar = parse(s); }

  // Returns the value for a given key. If the key does not exist
  // an empty string is returned.
  std::string get(const std::string key);

  // Returns the value for a given key. If the key does not exist,
  // the given default value is returned.
  std::string get(const std::string key, const std::string def);

  // Returns the list of all parsed keys.
  std::vector<std::string> keys();

private:
  // Splits a string given a character delimiter.
  void split(const std::string& s, char delim,
    std::vector<std::string>& elems);

  // "Unescapes" an HTTP urlencoded value. This should be applied
  // on values after query parsing.
  void urlDecode(const std::string& in, std::string& out);

  // Parses an HTTP query string.
  std::map<std::string, std::string> parse(const std::string& req_body);
};
