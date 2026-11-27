// EXPECT: error a struct converts to another form of itself only
//
// Apple rejects this source.
struct P { float a; float b; };
kernel void struct_from_int_literal_rejected(device float* o [[buffer(0)]]) { P u = 0; o[0] = u.a; }
