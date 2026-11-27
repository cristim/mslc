// EXPECT: error cannot be narrowed to an integer
struct PI { int a; int b; }; struct OI { PI p; float c; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { OI o{in[0],in[1],in[2]}; out[0]=o.c; }
