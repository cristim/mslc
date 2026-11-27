// EXPECT: valid
// REFLECT: "metal_index": 0, "spec_id": 3, "element_size": 4, "param_index": 1, "name": "a"
// REFLECT: "metal_index": 0, "spec_id": 4, "element_size": 16, "param_index": 1, "name": "a"
// REFLECT: "metal_index": 1, "spec_id": 5, "element_size": 4, "param_index": 2, "name": "b"
// DISASM-NOT: SpecId 6
//
// Two kernels in one module that both take threadgroup(0) with different element types. SpecIds
// are per module, so each argument gets its own and each entry's reflection names its own; a
// shared id would have to mean the same count for a float and a float4 element.
kernel void threadgroup_argument_two_kernels_a(device float* out [[buffer(0)]],
                                               threadgroup float* a [[threadgroup(0)]],
                                               uint l [[thread_position_in_threadgroup]])
{ a[l] = 1.0; out[l] = a[l]; }

kernel void threadgroup_argument_two_kernels_b(device float* out [[buffer(0)]],
                                               threadgroup float4* a [[threadgroup(0)]],
                                               threadgroup uint* b [[threadgroup(1)]],
                                               uint l [[thread_position_in_threadgroup]])
{ a[l] = float4(1.0); b[l] = 1u; out[l] = a[l].x + float(b[l]); }
