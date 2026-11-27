// EXPECT: error expression is not assignable
struct S { float v; float operator[](uint i) const { return v + float(i); } };
kernel void struct_operator_subscript_assign_rejected(device float* out [[buffer(0)]]) {
    S s = { 1.0f };
    s[0] = 1.0f;
}
