// EXPECT: error has [[invariant]], which is only valid on a vertex output's [[position]] field

struct FragmentIn {
	float4 p [[ position ]][[ invariant ]];
};

fragment float4 fragment_input_position_invariant_rejected(FragmentIn in [[ stage_in ]]) {
	return in.p;
}
