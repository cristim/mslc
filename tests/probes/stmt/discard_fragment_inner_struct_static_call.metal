// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %void
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace N {
struct metal { static void discard_fragment() { } };
fragment ::metal::float4 discard_fragment_inner_struct_static_call()
{
    metal::discard_fragment();
    return ::metal::float4(1.0);
}
}
