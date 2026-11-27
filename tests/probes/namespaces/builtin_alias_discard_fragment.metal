// EXPECT: valid
// DISASM: OpKill
// M::discard_fragment() is the library's discard_fragment.
#include <metal_stdlib>
namespace M = metal;
fragment float4 fr(float4 p [[position]]) { if (p.x < 0) M::discard_fragment(); return p; }
