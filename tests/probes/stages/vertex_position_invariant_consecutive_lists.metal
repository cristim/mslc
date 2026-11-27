// EXPECT: valid
// DISASM-CAPTURE: OpDecorate (%[A-Za-z0-9_]+) BuiltIn Position
// DISASM-MATCH-CAPTURED: OpDecorate $1 Invariant
// DISASM-CAPTURE: OpDecorate (%[A-Za-z0-9_]+) Location 0
// DISASM-MATCH-CAPTURED: OpDecorate $2 NoPerspective
// DISASM: Location 2

struct VertexOut {
	float4 o0 [[ position ]][[ invariant ]];
	float2 uv [[ user(locn0) ]][[ center_no_perspective ]];
	float4 c [[]][[ user(locn2) ]];
};

vertex VertexOut vertex_position_invariant_consecutive_lists(uint vid [[ vertex_id ]][[]]) {
	VertexOut out;
	out.o0 = float4(float(vid), 0.0f, 0.0f, 1.0f);
	out.uv = float2(0.5f, 0.25f);
	out.c = float4(1.0f);
	return out;
}
