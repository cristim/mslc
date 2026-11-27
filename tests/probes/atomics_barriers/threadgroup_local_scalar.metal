// EXPECT: valid
// DISASM-MATCH: OpVariable %_ptr_Workgroup_float Workgroup
// DISASM-NOT: OpVariable %_ptr_Function_float Function
kernel void k(device float* data [[buffer(0)]], uint t [[thread_position_in_threadgroup]], uint n [[threads_per_threadgroup]]) {
	threadgroup float s;
	if (t == n - 1) { s = 42.0; }
	threadgroup_barrier(mem_flags::mem_threadgroup);
	data[t] = s;
}
