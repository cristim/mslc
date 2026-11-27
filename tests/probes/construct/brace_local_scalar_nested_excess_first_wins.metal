// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float %[0-9]+ %uint_0(_[0-9]+)? %int_5[^_0-9]
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float %[0-9]+ %uint_0(_[0-9]+)? %int_6[^_0-9]
// DISASM-NOT: OpFAdd
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[5], in[6] } }; out[0] = x; }
