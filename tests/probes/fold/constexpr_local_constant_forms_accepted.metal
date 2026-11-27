// EXPECT: valid
// The constant forms stay valid: arithmetic, vectors, brace and = struct values,
// earlier constexpr and const integer names, members of constexpr values, ?:.
struct S { float value; };
struct T { float a; float b; };
kernel void constexpr_local_constant_forms_accepted(device float *out [[buffer(0)]]) {
  constexpr int a = 2;
  constexpr int b = a * 3;
  const int n = 3;
  constexpr int c = n + 1;
  constexpr float3 v = float3(1.0, 2.0, 3.0);
  constexpr S s = {2.0};
  constexpr T t = {1.0, 2.0};
  constexpr float d = t.b;
  constexpr float4 w = float4(1.0);
  constexpr float x = w.x;
  constexpr bool flag = true;
  constexpr float e = flag ? 1.0 : 2.0;
  out[0] = float(b + c) + v.x + s.value + d + x + e;
}
