// EXPECT: error a vector of 4 components cannot be converted to one of 3
//
// Apple rejects a float4 returned where a float3 is expected; the scalar
// broadcast must not loosen that.
#include <metal_stdlib>
using namespace metal;

fragment float3 entry_return_vector_width_mismatch_rejected()
{
    return float4(1);
}
