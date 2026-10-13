// EXPECT: error the using declaration of "A::x" conflicts with "x" declared earlier in the same scope
// Apple: "target of using declaration conflicts with declaration already in scope".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void using_declaration_after_local_same_block_rejected(device int* out [[buffer(0)]])
{
    int x = 5;
    using A::x;
    out[0] = x;
}
