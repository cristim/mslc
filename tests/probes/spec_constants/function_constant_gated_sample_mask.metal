// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM-MATCH: OpSelect %uint %[0-9a-z_]+ %[0-9a-z_]+ %uint_4294967295
// A [[sample_mask]] output gated on a function constant, here one derived from another as ANGLE's fragment
// shaders write it. The mask is declared either way; when the gate is off the store writes all ones, which
// leaves the coverage unchanged, where a mask left unwritten would be undefined in Vulkan.
#include <metal_stdlib>
using namespace metal;
constant bool multisampled [[function_constant(2)]];
constant bool writeMask = multisampled;
struct Out {
    float4 color [[color(0)]];
    uint mask [[sample_mask, function_constant(writeMask)]];
};
fragment Out function_constant_gated_sample_mask(constant uint& coverage [[buffer(0)]]) {
    Out out;
    out.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    if (writeMask) {
        out.mask = coverage;
    }
    return out;
}
