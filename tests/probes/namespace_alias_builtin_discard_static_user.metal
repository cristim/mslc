// EXPECT: valid
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace E { struct M { static void discard_fragment() { } }; }
fragment metal::float4 f() {
    namespace M = ::metal;
    { using E::M; M::discard_fragment(); }
    return metal::float4(1.0f);
}
