#include "plusaes.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

int main(int argc, char *argv[]) {
    // AES-CBC 128-bit

    // parameters
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    const std::string raw_data = buffer.str();
    std::cout << raw_data.size();
    const std::vector<unsigned char> key = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; // 16-char = 128-bit
    const unsigned char iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    };

    // encrypt
    const unsigned long encrypted_size = plusaes::get_padded_encrypted_size(raw_data.size());
    std::vector<unsigned char> encrypted(encrypted_size);

    plusaes::encrypt_cbc((unsigned char*)raw_data.data(), raw_data.size(), &key[0], key.size(), &iv, &encrypted[0], encrypted.size(), true);
    // fb 7b ae 95 d5 0f c5 6f 43 7d 14 6b 6a 29 15 70

  return 0;
}