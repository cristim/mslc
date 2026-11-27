// EXPECT: error is marked constant, and the object is not in the constant address space
struct S { float4 data[2]; float4 operator[](uint i) const constant { return data[i]; } };
kernel void struct_operator_subscript_const_constant_on_thread_rejected(device float4* out [[buffer(0)]]) {
    S local = {};
    out[0] = local[0];
}
