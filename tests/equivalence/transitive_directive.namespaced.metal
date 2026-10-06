#include <metal_stdlib>
using namespace metal;
namespace Z { constant float v = 4.0; }
namespace X { using namespace Z; }
namespace A { using namespace X; float g() { return v; } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::g(); }
