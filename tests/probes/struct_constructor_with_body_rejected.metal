// EXPECT: error a constructor with a body is not lowered yet
// A body that assigns a field has an effect mslc would drop: rejected, not ignored.
struct S { float x; S() { x = 2.0; } };
kernel void struct_constructor_with_body_rejected(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
