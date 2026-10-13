// EXPECT: valid
// DISASM: OpReturnValue %int_71
// DISASM: OpReturnValue %int_72
// DISASM: OpReturnValue %int_73
// DISASM: OpReturnValue %int_74
// DISASM: OpReturnValue %int_75
// DISASM: NClamp
// What Apple accepts beside "using namespace metal": a clamp of another arity and a sin of
// int called with ints (no library overload matches, so the user's is called); A::clamp
// qualified; a block using declaration and a namespace's own clamp, which hide the library's.
#include <metal_stdlib>
using namespace metal;
namespace A { int clamp(int x, int y) { return 71; } int sin(int x) { return 72; } }
using namespace A;
namespace C { int clamp(int x, int lo, int hi) { return 73; } }
namespace D { int clamp(int x, int lo, int hi) { return 74; } }
namespace N { int clamp(int x, int lo, int hi) { return 75; } int g() { return clamp(1, 0, 2); } }
kernel void stdlib_name_beside_using_namespace_metal_accepted(device int* out [[buffer(0)]])
{
    out[0] = clamp(1, 2) + sin(1) + C::clamp(1, 0, 2) + N::g();
    { using D::clamp; out[1] = clamp(1, 0, 2); }
    out[2] = int(metal::clamp(1.5f, 0.0f, 2.0f));
}
