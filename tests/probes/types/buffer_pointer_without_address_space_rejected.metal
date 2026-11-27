// EXPECT: error pointer parameter needs an explicit address space (device or constant)
kernel void buffer_pointer_without_address_space_rejected(float* p)
{ float x = p[0]; }
