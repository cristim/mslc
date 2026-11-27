// EXPECT: valid
// RUN: grid=48 local=16 cases=3 elems=48
// A scalar bool operand of an arithmetic or bitwise operator, unary or binary, is promoted to int ([conv.prom]),
// so a | b and -a are ints and a * 2.5f a float. & | ^ and ~ act componentwise on a bool vector and give one
// (MSL 3.1), a scalar beside it widened. Apple's compiler types them so (black-box static_asserts with
// xcrun metal 32023.883 on macOS 26); the overloads of which() see it.
#include <metal_stdlib>
using namespace metal;
inline float which(int) { return 1000.0f; }
inline float which(bool) { return 2000.0f; }
inline float which(bool2) { return 3000.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  bool a = (i & 1) != 0, b = (i & 2) != 0;
  bool2 va = bool2(a, b), vb = bool2(b, true);
  auto s = a | b; auto t = a + b; auto vo = va | vb; auto vx = va ^ vb; auto vn = ~va;
  float r[12] = { float(s), float(t), float(a * 2.5f), float(-a), float(~a), float(a ^ b),
                  float(vo.x) + 2 * float(vo.y), float(vx.x) + 2 * float(vx.y), float(vn.x) + 2 * float(vn.y),
                  which(a | b), which(vo), float((va & a).y) + which(a & b) };
  out[i] = r[(i / 4) % 12];
}
