// EXPECT: error a pointer field in value struct
struct PP { device float* p; float c; }; struct OP { PP q; float c; };
kernel void k(device float* out [[buffer(0)]]) { OP o{out,out,1.0f}; out[0]=o.c; }
