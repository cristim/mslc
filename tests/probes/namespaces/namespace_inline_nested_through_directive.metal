// EXPECT: valid
// DISASM: OpFAdd
// An inline namespace of an inline namespace is still the outer namespace's member, an
// extension of I adds to it, and using namespace A brings all of those in.
#include <metal_stdlib>
namespace A { inline namespace I { constant float v = 1.0; } }
namespace A { constant float v2 = 2.0; }
namespace A { inline namespace I { constant float w = 3.0; } }
namespace A { inline namespace J { inline namespace K { constant float q = 4.0; } } }
using namespace A;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = v + v2 + w + A::q + A::J::q; }
