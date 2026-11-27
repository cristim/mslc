// EXPECT: error parameter "p" needs a device or constant address space to be a buffer binding, not threadgroup
//
// A threadgroup pointer is an argument of its own; with [[buffer]] Apple rejects it.
kernel void buffer_threadgroup_pointer_with_buffer_index_rejected(threadgroup float* p [[buffer(0)]]) { float x = p[0]; }
