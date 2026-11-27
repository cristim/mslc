// EXPECT: error attribute "position" cannot appear more than once on a declaration

struct VertexOut {
	float4 o0 [[ position ]][[ position ]];
};

vertex VertexOut field_duplicate_attribute_across_lists_rejected() {
	VertexOut out;
	out.o0 = float4(0.0f);
	return out;
}
