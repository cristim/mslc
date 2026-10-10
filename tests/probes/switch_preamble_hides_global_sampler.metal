// EXPECT: valid
// REFLECT: "embedded_samplers": []
// REFLECT-NOT: "kind": "Sampler"
// DISASM-MATCH: OpStore %[0-9]+ %int_17
#include <metal_stdlib>
using namespace metal;
constexpr sampler s(coord::normalized, filter::linear);
kernel void f(device int* out [[buffer(0)]], uint x [[thread_position_in_grid]]) {
  switch (x) {
    int s;
    default: s = 17; out[x] = s; break;
  }
}
