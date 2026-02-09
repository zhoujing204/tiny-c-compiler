/* Test function calls with different number of arguments */

int add2(int a, int b) { return a + b; }

int add4(int a, int b, int c, int d) { return a + b + c + d; }

int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

int main() {
  int x;
  int y;
  int z;

  x = add2(10, 20);
  if (x != 30)
    return 1;

  y = add4(10, 20, 30, 40);
  if (y != 100)
    return 2;

  z = add6(10, 20, 30, 40, 50, 60);
  if (z != 210)
    return 3;

  return 0;
}
