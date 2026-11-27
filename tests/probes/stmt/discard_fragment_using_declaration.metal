// EXPECT: valid
// DISASM-MATCH: OpKill
#include <metal_stdlib>
using metal::discard_fragment;
fragment metal::float4 discard_fragment_using_declaration()
{
    discard_fragment();
    return metal::float4(1.0);
}
