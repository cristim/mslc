// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "vs_f"
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "fs_f"
// DISASM-ENTRY-BUILTINS: vs_f Position, VertexIndex
// DISASM-ENTRY-BUILTINS: fs_f FragCoord
// REFLECT: "name": "vs_f"
// REFLECT: "name": "fs_f"
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 position [[position]]; float value; };
template <typename T> vertex VOut vs(uint vid [[vertex_id]]) {
	VOut o; o.position = float4(float(vid), 0.0f, 0.0f, 1.0f); o.value = float(T(0.5f)); return o;
}
template <typename T> fragment float4 fs(VOut in [[stage_in]]) {
	return float4(in.value, float(T(0.25f)), 0.0f, 1.0f);
}
template [[host_name("vs_f")]] vertex VOut vs<float>(uint);
template [[host_name("fs_f")]] fragment float4 fs<float>(VOut);
