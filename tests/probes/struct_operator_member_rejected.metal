// EXPECT: error an operator overload is not lowered
struct S { float x; S operator+(S b) const { S r; r.x = x + b.x; return r; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1; out[0] = (s + s).x; }
