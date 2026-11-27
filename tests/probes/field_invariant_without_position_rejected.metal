// EXPECT: error has [[invariant]], which is only valid on a vertex output's [[position]] field

struct VertexOut {
	float4 p [[ position ]];
	float4 uv [[ invariant ]];
};

vertex VertexOut field_invariant_without_position_rejected() {
	VertexOut out;
	out.p = float4(0.0f);
	out.uv = float4(0.0f);
	return out;
}
