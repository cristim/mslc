// EXPECT: error reference to "discard_fragment" is ambiguous
#include <metal_stdlib>
using namespace metal;
void discard_fragment() { }
fragment metal::float4 discard_fragment_using_ambiguous_rejected()
{
    discard_fragment();
    return metal::float4(1.0);
}
