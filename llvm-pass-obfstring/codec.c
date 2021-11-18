//#define STRING_ENCRYPTION_KEY "S5zeMZ68*K7ddKwiOWTt"
#define STRING_ENCRYPTION_KEY "fl80*098f^m&j$NfdkL083*()_"

int encode(unsigned char *str) {
  int i = 0;
  for (; str[i] != 0; ++i) {
    unsigned key_idx = i % (sizeof(STRING_ENCRYPTION_KEY) - 1);
    if (str[i] == 0xff) {
      // value 0xff is banned in the string because it is used as
      // a placeholder for 0 in encrypted result
      str[i] = '.';
    }
    str[i] ^= STRING_ENCRYPTION_KEY[key_idx];
    if (str[i] == 0) {
      str[i] = STRING_ENCRYPTION_KEY[key_idx] ^ 0xff;
    }
  }
  return i;
}

int decode(unsigned char *str) {
  int i = 0;
  for (; str[i] != 0; ++i) {
    unsigned key_idx = i % (sizeof(STRING_ENCRYPTION_KEY) - 1);
    str[i] ^= STRING_ENCRYPTION_KEY[key_idx];
    if (str[i] == 0xff) {
      str[i] = STRING_ENCRYPTION_KEY[key_idx];
    }
  }
  return i;
}