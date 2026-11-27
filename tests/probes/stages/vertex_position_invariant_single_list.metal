// EXPECT: valid
// DISASM-CAPTURE: OpDecorate (%[A-Za-z0-9_]+) BuiltIn Position
// DISASM-MATCH-CAPTURED: OpDecorate $1 Invariant

struct VertexOut {
	float4 o0 [[ position, invariant ]];
};

vertex VertexOut vertex_position_invariant_single_list(uint vid [[ vertex_id ]]) {
	VertexOut out;
	out.o0 = float4(float(vid), 0.0f, 0.0f, 1.0f);
	return out;
}
