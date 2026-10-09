// EXPECT: valid
// Values that fit stay constants: unsigned wrap is defined, float overflow to infinity
// is accepted by Apple, 1 << 31 fits, and a non-constexpr constant may overflow.
constant constexpr uint a = 4294967295u + 1u;
constant constexpr uint b = 0u - 1u;
constant constexpr float c = 3.4e38f * 10.0f;
constant constexpr int d = 1 << 31;
constant constexpr float e = 1.0f / 3.0f;
constant int f = 2147483647 + 1;
constant float g = 1.0f / 0.0f;
kernel void file_scope_constexpr_representable_values_accepted(device float *out [[buffer(0)]]) {
  out[0] = float(a) + float(b) + c + float(d) + e + float(f) + g;
}
