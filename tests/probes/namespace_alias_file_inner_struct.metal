// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_2[_0-9]*
// DISASM-NOT: OpKill
// DISASM-ORDER: %22 = OpFunction %void
// DISASM-ORDER: OpFunctionCall %int %42
// DISASM-ORDER: %42 = OpFunction %int
// DISASM-ORDER: OpReturnValue %int_2
#include <metal_stdlib>
namespace A { int discard_fragment() { return 11; } }
namespace M = A;
namespace E { struct M { static int discard_fragment() { return 2; } }; }
kernel void k(device int* out [[buffer(0)]]) { using E::M; out[0] = M::discard_fragment(); }
