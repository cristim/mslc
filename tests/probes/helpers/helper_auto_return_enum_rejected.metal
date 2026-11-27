// EXPECT: error does not deduce its return type from an enum value
#include <metal_stdlib>
enum E { A, B }; auto f(int x) { return A; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = float(f(int(o[1]))); }
