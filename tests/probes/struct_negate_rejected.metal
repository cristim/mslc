// EXPECT: error a unary operator on the struct P is not lowered
//
// Apple rejects this source.
struct P { float a; float b; };
kernel void struct_negate_rejected(device float* o [[buffer(0)]]) { P s; s.a = 1.0; s.b = 2.0; P t = -s; o[0] = t.a; }
