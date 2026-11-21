// EXPECT: error brace elision into PA, which has an array member
struct PA { float a[2]; }; struct OA { PA p; float c; };
kernel void k(device float* out [[buffer(0)]]) { OA o{1.0f,2.0f,3.0f}; out[0]=o.c; }
