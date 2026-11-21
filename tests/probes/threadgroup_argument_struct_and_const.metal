// EXPECT: valid
// REFLECT: "metal_index": 0, "spec_id": 3, "element_size": 32, "param_index": 1, "name": "s"
// REFLECT: "metal_index": 1, "spec_id": 4, "element_size": 4, "param_index": 2, "name": "k"
// REFLECT: "metal_index": 2, "spec_id": 5, "element_size": 16, "param_index": 3, "name": "v"
// REFLECT: "metal_index": 3, "spec_id": 6, "element_size": 64, "param_index": 4, "name": "m"
//
// The reflected element_size is Metal's size (a float3 element steps 16, a struct holding a
// float and a float3 is 32), so the host's byte length divides into the element count the
// module's array wants. A const pointee is read only.
struct S { float a; float3 b; };
kernel void threadgroup_argument_struct_and_const(device float* out [[buffer(0)]],
                                                  threadgroup S* s [[threadgroup(0)]],
                                                  const threadgroup float* k [[threadgroup(1)]],
                                                  threadgroup float3* v [[threadgroup(2)]],
                                                  threadgroup float4x4* m [[threadgroup(3)]],
                                                  uint l [[thread_position_in_threadgroup]])
{
	s[l].a = 1.0;
	v[l] = float3(2.0);
	m[l] = float4x4(1.0);
	out[l] = s[l].a + k[l] + v[l].x + m[l][0][0];
}
