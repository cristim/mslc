// EXPECT: valid
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 3, "element_size": 4, "param_index": 1, "name": "a" }
//
// A lone attribute-less threadgroup pointer takes threadgroup index 0 (Apple: air.location_index 0).
kernel void threadgroup_argument_lone_implicit_index_zero(device float* out [[buffer(0)]], threadgroup float* a,
                                                          uint l [[thread_position_in_threadgroup]])
{ a[l] = 1.0; out[l] = a[l]; }
