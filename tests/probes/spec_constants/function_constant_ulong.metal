// EXPECT: valid
// DISASM-CAPTURE: OpDecorate (%[0-9]+) SpecId 65536
// DISASM-MATCH-CAPTURED: $1 = OpSpecConstant %ulong 0
// DISASM-CAPTURE: OpDecorate (%[0-9]+) SpecId 131072
// DISASM-MATCH-CAPTURED: $2 = OpSpecConstantFalse %bool
// REFLECT: "function_constants":
// REFLECT: { "name": "reloc_word", "type": "ulong", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// [[function_constant(n)]] declares a value the host gives when it makes a function from the library
// (MTLFunctionConstantValues). It is a specialization constant at SpecId 65536 + n, with a bool at
// SpecId 131072 + n that the host sets when it gives a value, and the reflection lists both. The
// kernel is Julia Metal.jl's test of function constants.
#include <metal_stdlib>
using namespace metal;
constant ulong reloc_word [[function_constant(0)]];
kernel void fc_test(device ulong* out [[buffer(0)]]) {
    out[0] = reloc_word;
}
