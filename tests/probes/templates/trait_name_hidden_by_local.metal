// EXPECT: valid
// A local named make_unsigned_t hides the library's trait, so "make_unsigned_t < n" is a comparison.
#include <metal_stdlib>
using namespace metal;
kernel void trait_name_hidden_by_local(device uint* out [[buffer(0)]]) {
    int make_unsigned_t = 3;
    int n = 4;
    out[0] = make_unsigned_t < n;
}
