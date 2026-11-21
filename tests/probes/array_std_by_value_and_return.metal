// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// DISASM-MATCH: = OpFunctionParameter %_arr_float_uint_3
// A by-value array parameter is a copy the callee may change; a reference parameter is the
// caller's array; a returned array is independent of the callee's local; a braced return
// builds the array.
#include <metal_stdlib>
using namespace metal;
array<float, 3> bump(array<float, 3> a) { a[0] += 1.0f; return a; }
void bumpRef(thread array<float, 3>& a) { a[1] += 10.0f; }
array<float, 3> make(float x) { array<float, 3> r = {x, x * 2.0f, x * 3.0f}; return r; }
array<float, 3> braced(float x) { return {x, 1.0f, 2.0f}; }
float sum(const array<float, 3> a) { return a[0] + a[1] + a[2]; }
array<array<float, 2>, 2> grid2(float x) { return {{{x, 1.0f}, {2.0f, 3.0f}}}; }
kernel void array_std_by_value_and_return(device float* out [[buffer(0)]], device const float* in [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    array<float, 3> a = make(in[i]);
    array<float, 3> b = bump(a);
    bumpRef(a);
    array<float, 3> c = braced(in[i + 1]);
    c = bump(c);
    out[i * 12 + 0] = a[0];
    out[i * 12 + 1] = a[1];
    out[i * 12 + 2] = a[2];
    out[i * 12 + 3] = b[0];
    out[i * 12 + 4] = b[1];
    out[i * 12 + 5] = c[0];
    out[i * 12 + 6] = sum(c);
    out[i * 12 + 7] = make(in[i + 2])[uint(in[i + 3]) % 3u];
    out[i * 12 + 8] = make(in[i + 2])[2];
    out[i * 12 + 9] = grid2(in[i])[0][0] + grid2(in[i])[1][uint(in[i + 4]) % 2u];
    out[i * 12 + 10] = sum(bump(make(1.0f)));
    out[i * 12 + 11] = sum(a);
}
