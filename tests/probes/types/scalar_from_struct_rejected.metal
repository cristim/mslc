// EXPECT: error a struct converts to another form of itself only
//
// Apple rejects this source.
struct P { float a; float b; };
kernel void scalar_from_struct_rejected(device float* o [[buffer(0)]]) { P s; s.a = 1.0; s.b = 2.0; float v = s; o[0] = v; }
