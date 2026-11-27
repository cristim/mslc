// EXPECT: error conditional_t of a pointer, array, texture, sampler, atomic or address-space-qualified type is not lowered
// The chosen type's address space would be lost on the way through the trait: a threadgroup variable declared
// this way would be a private one, so the trait refuses such operands as make_scalar and the others do.
#include <metal_stdlib>
using namespace metal;
kernel void trait_conditional_address_space_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
    conditional_t<true, threadgroup uint, float> shared;
    shared = 1u;
    out[i] = shared;
}
