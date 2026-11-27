// EXPECT: valid
// WAPaint.metal declares four struct locals in one statement; a const list needs a value for each.
struct S { float x; float y; };
kernel void several_declarators_struct_and_const(device float *o [[buffer(0)]]) {
  S p, q;
  const float a = 1.0, b = 2.0;
  p.x = a;
  q.x = b;
  o[0] = p.x + q.x;
}
