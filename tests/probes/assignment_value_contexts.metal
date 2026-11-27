// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// An assignment as a value in an auto initialiser, a loop and an if condition, and of a struct (checked against
// host clang: 500544, 510635, 510726 for i = 0, 1, 2).
#include <metal_stdlib>
using namespace metal;
struct P { float a; int b; };
int bump(thread int& n) { n += 1; return n; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int y = 0;
  auto q = (y = int(i) + 4);
  int n = 5, steps = 0;
  while ((n = n - 1) > int(i)) { steps++; }
  int m = 0;
  if ((m = bump(y)) > 5) { m += 100; }
  P p, r; p.a = 1.5f; p.b = 2;
  P s = (r = p);
  out[i] = float(q) + float(steps) * 10.0f + float(m) * 100.0f + (s.a + float(s.b) + r.a) * 100000.0f;
}
