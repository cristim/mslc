// EXPECT: error excess elements in S initializer
// Apple: "excess elements in struct initializer".
struct S { float a; float b; };
kernel void struct_brace_temporary_too_many_rejected(device float *o [[buffer(0)]]) {
  S s = S{1.0, 2.0, 3.0};
  o[0] = s.a;
}
