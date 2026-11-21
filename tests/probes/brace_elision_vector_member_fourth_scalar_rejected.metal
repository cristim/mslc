// EXPECT: error excess elements in V initializer
struct V { float x; float2 v; float y; };
kernel void k(device float* out [[buffer(0)]]) { V o{1.0f,2.0f,3.0f,4.0f}; out[0]=o.y; }
