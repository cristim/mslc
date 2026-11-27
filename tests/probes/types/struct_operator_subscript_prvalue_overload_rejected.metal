// EXPECT: error no matching function for call to "mut"
struct S { float v; float operator[](uint i) const { return v + float(i); } };
void mut(thread float& x) { x += 1.0f; }
kernel void struct_operator_subscript_prvalue_overload_rejected(device float* out [[buffer(0)]]) {
    S s = { 2.0f };
    mut(s[0]);
}
