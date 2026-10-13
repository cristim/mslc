// EXPECT: error "sqrt" conflicts with the using declaration of "sqrt" in the same scope
// A block's using declaration of a stdlib name occupies the name in that block, exactly
// as a using declaration of a user name does, so a local of the same name conflicts.
#include <metal_stdlib>
using namespace metal;
kernel void using_declaration_of_stdlib_name_then_local_same_block_rejected(device float* out [[buffer(0)]])
{
    using metal::sqrt;
    float sqrt = 4.0f;
    out[0] = sqrt;
}
