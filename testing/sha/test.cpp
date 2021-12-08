#include <sstream>
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdio.h>

#include "sha2.hpp"

int main(int argc, char *argv[]) {
  // AES-CBC 128-bit

  std::ifstream t(argv[1]);
  std::stringstream buffer;
  buffer << t.rdbuf();
  const std::string raw_data = buffer.str();

  auto ptr = reinterpret_cast<const uint8_t*>(raw_data.data());
  auto length = raw_data.size();

  sha2::sha512(ptr, length);
  return 0;
}