// EXPECT: error function "discard_fragment" is not a builtin mslc recognises
#include <metal_stdlib>
fragment metal::float4 discard_fragment_without_using_rejected()
{
    discard_fragment();
    return metal::float4(1.0);
}
