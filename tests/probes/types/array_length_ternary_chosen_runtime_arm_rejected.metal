// EXPECT: error an array length needs a constant integer
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], constant uint* un [[buffer(2)]]) { int y = in[0]; float a[false ? 1 : y]; out[0] = a[0]; out[0] = 1.0f; }
