char *itoau(unsigned int num, char *buf, int base) {
  if (base < 0 || base > 36 || buf == 0) {
    return 0;
  }
  
  unsigned int i = 0;
  unsigned int l=i;  

  if (num == 0) {
    buf[i++] = '0';
  }  

  while (num > 0) {
    unsigned int r = num % base;

    num = num / base;
    buf[i++] =  r < 10 ? '0' + r : 'a' + (r - 10);
  }
  unsigned int r = i - 1;
  while (l < r) {
    char tmp = buf[l];
    buf[l++] = buf[r];
    buf[r--] = tmp;

  }
  buf[i] = '\0';
  return buf;
}

char *itoa(int num, char *buf, int base) {
  if (base < 0 || base > 36 || buf == 0) {
    return 0;
  }
  
  int i = 0;
  // Only do signed if base 10
  if (num < 0 && base == 10)  {
    buf[i++] = '-';
    num = -num;
  }
  int l=i;  

  if (num == 0) {
    buf[i++] = '0';
  }  

  while (num > 0) {
    int r = num % base;

    if (r < 0) {
      buf[i++] = '-';
    }
    num = num / base;
    buf[i++] =  r < 10 ? '0' + r : 'a' + (r - 10);
  }
  int r = i - 1;
  while (l < r) {
    char tmp = buf[l];
    buf[l++] = buf[r];
    buf[r--] = tmp;

  }
  buf[i] = '\0';
  return buf;
}
