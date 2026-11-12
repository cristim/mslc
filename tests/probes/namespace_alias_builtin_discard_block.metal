// EXPECT: valid
// DISASM: OpKill
#include <metal_stdlib>
fragment metal::float4 f() {
    namespace M = ::metal;
    M::discard_fragment(); return metal::float4(1.0f);
}
