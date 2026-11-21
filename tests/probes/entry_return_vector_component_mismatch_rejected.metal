// EXPECT: error a vector is not implicitly converted to a vector of another component type
//
// Apple rejects an int4 returned from a float4 function, as it does in an
// initialiser, so the entry-point return takes the implicit conversion too.
#include <metal_stdlib>
using namespace metal;

fragment float4 entry_return_vector_component_mismatch_rejected()
{
    return int4(1);
}
