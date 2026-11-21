// EXPECT: valid
// DISASM-NO-MATCH: OpVariable %_ptr_Function_mat2v2float
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) {
	S s{.a = float2x2(in[1])[i].x, .a = in[3]};
	out[0] = s.a;
}
