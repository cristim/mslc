// EXPECT: valid
// REFLECT: { "name": "c", "type": "int", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// Two kernels share one helper that reads a function constant through a template instance. The helper is
// emitted once, for the first kernel; tests/library_api.c checks that the second kernel lists the
// constant as well.
#include <metal_stdlib>
using namespace metal;
constant int c [[function_constant(0)]];
template <typename T> T leaf() { return T(c); }
int shared() { return leaf<int>(); }
kernel void a(device int* out [[buffer(0)]]) { out[0] = shared(); }
kernel void b(device int* out [[buffer(0)]]) { out[0] = shared() + 1; }
