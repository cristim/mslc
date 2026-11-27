// EXPECT: error field "m" of "VOut" has the enum type Mode, which Apple's compiler does not allow in a vertex function's returned struct
//
// Not a miscompile today: the field crosses as its underlying int and the module is
// spirv-val-valid, but the issue (#255) reports Apple's own compiler refuses this with
// "field of illegal type" (unverified here: no Apple toolchain reachable on this
// machine, quoted from the issue text only).
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct VOut {
    float4 p [[position]];
    Mode m;
};

vertex VOut enum_vertex_output_field_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(0.0);
    o.m = B;
    return o;
}
