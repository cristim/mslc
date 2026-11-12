// EXPECT: valid
// DISASM-NOT: OpKill
#include <metal_stdlib>
namespace N {
    namespace metal { void discard_fragment() { } }
    namespace M = metal;
}
fragment metal::float4 f() { N::M::discard_fragment(); return metal::float4(1.0f); }
