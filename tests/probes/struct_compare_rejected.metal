// EXPECT: error a struct converts to another form of itself only
//
// Apple rejects this source.
struct P { float a; float b; };
kernel void struct_compare_rejected(device float* o [[buffer(0)]]) { P s; P t; s.a = 1.0; s.b = 2.0; t = s; if (s == t) { o[0] = 1.0; } }
