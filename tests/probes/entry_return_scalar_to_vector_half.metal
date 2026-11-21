// EXPECT: valid
//
// A float scalar returned from a half4 fragment function, with no helper call.
#include <metal_stdlib>
using namespace metal;

fragment half4 entry_return_scalar_to_vector_half()
{
    return 0.5f;
}
