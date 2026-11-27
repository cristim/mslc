// EXPECT: error is called on an object in device, constant or threadgroup memory
struct S { float4 data[2]; float4 operator[](uint i) const { return data[i]; } };
kernel void struct_operator_subscript_const_thread_on_constant_rejected(device float4* out [[buffer(0)]], constant S& s [[buffer(1)]]) {
    out[0] = s[0];
}
