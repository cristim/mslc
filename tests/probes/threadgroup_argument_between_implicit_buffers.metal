// EXPECT: valid
// REFLECT: { "kind": "Buffer", "metal_index": 0,
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 1, "param_index": 2, "name": "in2"
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 3, "element_size": 4, "param_index": 1, "name": "a" }
//
// Threadgroup arguments are numbered apart from the buffers: the second implicit device buffer is
// buffer 1 whatever sits between the two, and the threadgroup argument is index 0.
kernel void threadgroup_argument_between_implicit_buffers(device float* in, threadgroup float* a, device float* in2,
                                                          uint l [[thread_position_in_threadgroup]])
{ a[l] = in[l]; in2[l] = a[l]; }
