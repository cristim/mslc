// EXPECT: error variables in the threadgroup address space can only be declared in a kernel function
void h() { threadgroup float s; }
kernel void k(device float* d [[buffer(0)]]) { h(); }
