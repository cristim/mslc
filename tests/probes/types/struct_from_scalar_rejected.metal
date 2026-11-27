// EXPECT: error a struct converts to another form of itself only
//
// Apple rejects this source.
struct P { float a; float b; };
kernel void struct_from_scalar_rejected(device float* o [[buffer(0)]]) { P u = 1.0f; o[0] = u.a; }
