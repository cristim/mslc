// EXPECT: valid
// DISASM-MATCH: OpKill
#include <metal_stdlib>
using namespace metal;
fragment float4 discard_fragment_global_qualified()
{
    ::metal::discard_fragment();
    return float4(1.0);
}
