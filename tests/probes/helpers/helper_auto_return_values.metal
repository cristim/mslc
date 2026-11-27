// EXPECT: valid
// RUN: grid=4 local=4 cases=3
//
// "auto f(...)" returns the type of its first return's value ([dcl.spec.auto]): a scalar, a vector, a
// struct, int from an integer division, the type a template's parameters give, void with no return
// value, and the same type from two returns. A deduced function called inside another, inside an
// expression and in sizeof. Apple's compiler accepts the source; the values were read back on Vulkan.
#include <metal_stdlib>
using namespace metal;
struct P { float a; int b; };
auto twice(float x) { float t = x * 2; return t; }
__attribute__((always_inline)) auto halfOf(int x) { return x / 2; }
auto vec3of(float x) { return float3(x, x + 1, x + 2); }
auto pair(float x) { P p; p.a = x; p.b = int(x) * 3; return p; }
auto inner(float x) { return twice(x) + 1; }
auto noValue(device float* o) { o[8] = 5; }
auto early(int x) { if (x > 2) { return x * 10; } return x; }
template <typename T> auto scale(T v, T s) { T r = v * s; return r; }
kernel void helper_auto_return_values(device float* out [[buffer(0)]], device const uint* in [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  if (i != 0) {
    return;
  }
  float base = float(in[3]);
  float local = 100;
  out[0] = twice(base) + local;
  out[1] = float(halfOf(int(in[7])));
  out[2] = vec3of(base).z;
  out[3] = pair(base).a + float(pair(base).b);
  out[4] = inner(base);
  noValue(out);
  out[5] = float(early(int(in[1]))) + float(early(int(in[5])));
  out[6] = scale(base, 2.0f) + scale(float2(1, 2), float2(3, 4)).y;
  out[7] = float(sizeof(twice(1.0f)) + sizeof(halfOf(1)) * 10);
}
