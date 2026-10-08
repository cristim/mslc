// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %void
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace metal { void discard_fragment(int) { } }
fragment metal::float4 discard_fragment_metal_overload_call()
{
    metal::discard_fragment(1);
    return metal::float4(1.0);
}
