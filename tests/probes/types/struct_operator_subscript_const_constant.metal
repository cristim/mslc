// EXPECT: valid
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
struct cb0_t { float4 data[4]; const float4 operator[](size_t i) const constant { return data[i]; } };
struct g_t { float4 v; const float4 operator[](size_t i) const constant { return v + float(i); } };
constant g_t g_cb0 = { .v = float4(1.0f, 2.0f, 3.0f, 4.0f) };
kernel void struct_operator_subscript_const_constant(device float4* out [[buffer(0)]], constant cb0_t* cb0 [[buffer(1)]]) {
    int g_cb0 = 0; out[0] = cb0[0][1].xxxx + float4((*cb0)[2].y + (::g_cb0)[g_cb0].z + float(sizeof((::g_cb0)[0])));
}
