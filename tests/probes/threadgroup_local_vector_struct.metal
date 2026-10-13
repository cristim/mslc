// EXPECT: valid
// DISASM-MATCH: OpVariable %_ptr_Workgroup_v3float Workgroup
// DISASM-MATCH: OpVariable %_ptr_Workgroup__struct_[0-9]+ Workgroup
struct S { float a; float3 b; };
float twice(float v) { return v * 2.0; }
kernel void k(device float* data [[buffer(0)]], uint t [[thread_position_in_threadgroup]]) {
	threadgroup float3 v;
	threadgroup S st;
	if (t == 0) { v = float3(1.0, 2.0, 3.0); st.a = 4.0; st.b.y = 5.0; v.y += 1.0; }
	threadgroup_barrier(mem_flags::mem_threadgroup);
	data[t] = twice(v.y) + st.a + st.b.y;
}
