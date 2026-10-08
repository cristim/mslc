// EXPECT: error has a field whose type is itself a struct
struct Inner { float a; }; struct Outer { Inner v; };
kernel void k(device float* out [[buffer(0)]], constant Outer* in [[buffer(1)]]) { out[0] = in[0].v.a; }
