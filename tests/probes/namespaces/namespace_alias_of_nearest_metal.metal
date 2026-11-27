// EXPECT: valid
// DISASM: OpReturnValue %float_7
// DISASM-NO-MATCH: NClamp
// "namespace M = metal;" inside N names N's own namespace metal, the nearest one, so
// M::clamp is N::metal::clamp (Apple returns 7), not the library's.
#include <metal_stdlib>
namespace N {
    namespace metal { float clamp(float a, float b, float c) { return 7.0f; } }
    namespace M = metal;
    float g() { return M::clamp(5.0f, 0.0f, 1.0f); }
}
kernel void namespace_alias_of_nearest_metal(device float* out [[buffer(0)]])
{ out[0] = N::g(); }
