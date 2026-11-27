// EXPECT: error [[invariant]] takes no argument

struct VertexOut {
	float4 p [[ position, invariant(0) ]];
};

vertex VertexOut field_invariant_argument_rejected() {
	VertexOut out;
	out.p = float4(0.0f);
	return out;
}
