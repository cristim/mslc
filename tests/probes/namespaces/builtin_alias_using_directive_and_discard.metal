// EXPECT: valid
// DISASM: OpKill
// DISASM: NClamp
// "using namespace Q;" of an alias chain to metal brings the library's names in, and
// Q::discard_fragment() still discards.
#include <metal_stdlib>
namespace M = ::metal;
namespace Q = M;
using namespace Q;
fragment float4 fr(float4 p [[position]]) { if (p.x < 0) Q::discard_fragment(); return float4(clamp(p.x, 0.0f, 1.0f)); }
