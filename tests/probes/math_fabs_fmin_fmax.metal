// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[0-9]+ FAbs %
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ NMin %
// DISASM-MATCH: OpExtInst %half %[0-9]+ NMin %
// DISASM-MATCH: OpExtInst %v2half %[0-9]+ NMax %
#include <metal_stdlib>
using namespace metal;
kernel void math_fabs_fmin_fmax(
    device const float* sf [[buffer(0)]], device const float3* vf [[buffer(1)]],
    device const half* sh [[buffer(2)]], device const half2* vh [[buffer(3)]],
    device float* out_sf [[buffer(4)]], device float3* out_vf [[buffer(5)]],
    device half* out_sh [[buffer(6)]], device half2* out_vh [[buffer(7)]])
{
    out_sf[0] = fmax(fmin(fabs(sf[0]), sf[1]), sf[2]);
    out_vf[0] = fmax(fmin(fabs(vf[0]), vf[1]), vf[2]);
    out_sh[0] = fmax(fmin(fabs(sh[0]), sh[1]), sh[2]);
    out_vh[0] = fmax(fmin(fabs(vh[0]), vh[1]), vh[2]);
}
