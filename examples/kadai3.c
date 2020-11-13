static int recn(int n) {
  if (n == 1) return 1;
  n -= 1;
  return recn(n) + 1;
}

int main() {
  for (int k = 0; k < 10000; k++) {
    recn(10000);
  }
  return 0;
}