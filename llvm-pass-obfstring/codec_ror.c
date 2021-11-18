int encode(unsigned char *str) {
  int i = 0;
  for (; str[i] != 0; ++i) {
    str[i] += 13;
  }
  return i;
}

int decode(unsigned char *str) {
  int i = 0;
  for (; str[i] != 0; ++i) {
    str[i] -= 13;
  }
  return i;
}