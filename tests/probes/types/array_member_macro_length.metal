// EXPECT: valid
// DISASM-MATCH: OpDecorate %_arr_mat4v4float_uint_80[_0-9]* ArrayStride 64
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 5120
// DISASM-MATCH: OpDecorate %_arr_float_uint_4[_0-9]* ArrayStride 4
//
// The shapes real shaders write: a typedef-named matrix with a literal length,
// and a length that is a macro.
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

#define MAX_LIGHTS 4

struct Uniforms
{
    matrix_float4x4 boneMatrices[80];
    float lights[MAX_LIGHTS];
};

kernel void array_member_macro_length(device float *out [[buffer(0)]],
    constant Uniforms &u [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = u.boneMatrices[79][3][3] + u.lights[MAX_LIGHTS - 1];
}
