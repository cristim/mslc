// EXPECT: error has [[invariant]], which is only valid on a vertex output's [[position]] field

struct FragmentOut {
	float d [[ depth(any) ]][[ invariant ]];
	float4 c [[ color(0) ]];
};

fragment FragmentOut fragment_output_invariant_rejected() {
	FragmentOut out;
	out.d = 0.5f;
	out.c = float4(1.0f);
	return out;
}
