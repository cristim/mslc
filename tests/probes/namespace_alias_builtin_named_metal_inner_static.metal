// EXPECT: valid
// DISASM: OpFunctionCall %void
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace E { struct metal { static void discard_fragment() { } }; }
namespace N {
namespace metal = ::metal;
fragment ::metal::float4 f() {
    using E::metal;
    metal::discard_fragment();
    return ::metal::float4(1.0f);
}
}
