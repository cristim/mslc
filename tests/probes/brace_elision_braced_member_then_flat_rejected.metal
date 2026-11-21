// EXPECT: error excess elements in Outer initializer
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Outer o{{1.0f},2.0f,3.0f}; out[0]=o.c; }
