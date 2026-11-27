// EXPECT: valid
struct S { float a; int b; };
S make(float a, int b) { S s; s.a = a; s.b = b; return s; }
kernel void auto_local_struct_copy_is_a_value(device float *o [[buffer(0)]]) {
  auto st = make(1.5f, 7);
  auto cp = st;
  cp.a = 9.0f;
  o[0] = st.a;
  o[1] = cp.a;
}
