// EXPECT: error field "m" of "VOut" has the enum type Mode, which Apple's compiler does not allow in a vertex function's returned struct
//
// Same rejection as enum_vertex_output_field_rejected.metal, but through the
// Apple-sample-header shape: a tag-less enum introduced by typedef. enumName holds
// the typedef's tag ("Mode"), confirmed empirically on origin/master, so the field
// keeps the direct "Mode" spelling rather than a further alias.
#include <metal_stdlib>
using namespace metal;
typedef enum { A, B } Mode;
struct VOut {
    float4 p [[position]];
    Mode m;
};

vertex VOut enum_typedef_vertex_output_field_rejected(uint vid [[vertex_id]])
{
    VOut o;
    o.p = float4(0.0);
    o.m = B;
    return o;
}
