// EXPECT: error a struct converts to another form of itself only
// Apple accepts this (C99-style elision under a designator: p.a=1, p.b=0); mslc refuses it, which is not a miscompile.
struct Pair { float a; float b; }; struct Outer { Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Outer o{.p=1.0f,.c=3.0f}; out[0]=o.c; }
