// EXPECT: valid
// DISASM: Flat
//
// Deliberate scope boundary (#255): the issue names only a vertex function's
// returned struct, so an enum field of a fragment-only [[stage_in]] struct is left
// accepted here. Apple's behaviour for this position is not stated by the issue and
// not independently measured on this machine (no Apple toolchain reachable); a
// follow-up tracker for this and the other unmeasured positions (color(n),
// attribute(n), bare enum return) is linked from #235. A later change that rejects
// this position should fail this probe rather than pass silently.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct FIn {
    float4 p [[position]];
    Mode m [[flat]];
};

fragment float4 enum_fragment_input_field_accepted(FIn in [[stage_in]])
{
    return in.m == B ? float4(1.0) : float4(0.0);
}
