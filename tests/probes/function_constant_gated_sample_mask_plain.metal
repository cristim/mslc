// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: OpSpecConstantFalse %bool
// DISASM-MATCH: OpSelect %uint %[0-9a-z_]+ %[0-9a-z_]+ %uint_4294967295
// REFLECT: { "name": "useMask", "type": "bool", "index": 0,
// A [[sample_mask]] gated directly on a bool function constant, in a fragment function that returns the struct.
// The function lists the constant it is gated on, so the runtime sets it, though nothing else reads it.
#include <metal_stdlib>
using namespace metal;
constant bool useMask [[function_constant(0)]];
struct Out {
    float4 color [[color(0)]];
    uint mask [[sample_mask, function_constant(useMask)]];
};
fragment Out function_constant_gated_sample_mask_plain() {
    return Out{ float4(0.0f), 0x5u };
}
