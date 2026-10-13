// EXPECT: valid
// A struct element: its fields and a vector field component are read and written through the index.
struct S { float x; float4 v; };
kernel void local_array_struct_elements(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  S s[2] = { {1.0, float4(2.0)}, {3.0, float4(4.0)} };
  int idx = int(in[0]);
  s[idx].v.y = 9.0;
  s[idx].x += 5.0;
  o[0] = s[1].x;
  o[1] = s[1].v.y;
}
