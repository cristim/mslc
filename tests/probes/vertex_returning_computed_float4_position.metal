// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ BuiltIn Position
// DISASM-MATCH: OpStore %[A-Za-z0-9_]+ %[A-Za-z0-9_]+
// The returned value is stored to the position output, not dropped.
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_returning_computed_float4_position(uint vid [[vertex_id]]) {
    float x = float((vid << 1) & 2u);
    float y = float(vid & 2u);
    return float4(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
}
