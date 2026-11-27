// EXPECT: valid
// DISASM: BuiltIn FragStencilRefEXT
// DISASM-NOT: DepthReplacing
// A fragment function whose only output is the stencil reference value.
#include <metal_stdlib>
using namespace metal;
struct Out { uint s [[stencil]]; };
fragment Out f(float4 p [[position]]) { Out o; o.s = 3u; return o; }
