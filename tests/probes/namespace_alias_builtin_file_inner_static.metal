// EXPECT: valid
// DISASM: OpFunctionCall %void
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace M = ::metal;
namespace E { struct M { static void discard_fragment() { } }; }
fragment metal::float4 f() {
    using E::M;
    M::discard_fragment();
    return metal::float4(1.0f);
}
