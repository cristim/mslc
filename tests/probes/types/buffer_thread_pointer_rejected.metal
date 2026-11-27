// EXPECT: error parameter "p" needs a device or constant address space to be a buffer binding, not thread
kernel void buffer_thread_pointer_rejected(thread float* p [[buffer(0)]]) { float x = p[0]; }
