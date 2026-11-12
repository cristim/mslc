// EXPECT: valid
// DISASM: OpKill
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
fragment metal::float4 f() {
    N::M::discard_fragment(); return metal::float4(1.0f);
}
