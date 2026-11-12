// EXPECT: valid
// DISASM: OpKill
#include <metal_stdlib>
namespace M = ::metal;
fragment metal::float4 f() { M::discard_fragment(); return metal::float4(1.0f); }
