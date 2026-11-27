// EXPECT: error conflicts with the using declaration of "A::x" in the same scope
// Apple: "declaration conflicts with target of using declaration already in scope".
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
kernel void using_declaration_then_local_same_block_rejected(device int* out [[buffer(0)]])
{
    using A::x;
    int x = 5;
    out[0] = x;
}
