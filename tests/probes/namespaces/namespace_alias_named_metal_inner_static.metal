// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_2[_0-9]*
// DISASM-NOT: OpKill
// DISASM-ORDER: %14 = OpFunction %void
// DISASM-ORDER: OpFunctionCall %int %20
// DISASM-ORDER: %20 = OpFunction %int
// DISASM-ORDER: OpReturnValue %int_2
#include <metal_stdlib>
namespace A { int discard_fragment() { return 11; } }
namespace E { struct metal { static int discard_fragment() { return 2; } }; }
namespace Host { namespace metal = A;
fragment ::metal::float4 f() { using E::metal; return ::metal::float4(float(metal::discard_fragment())); }
}
