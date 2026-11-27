// EXPECT: valid
// DISASM: OpStore
// DISASM-NOT: OpFAdd
// An inline anonymous namespace's v is A's own, so it beats the v a directive of B brings in
// (a plain anonymous namespace would make A::v ambiguous: g11).
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace { constant float v = 1; } namespace B { constant float v = 2; } using namespace B; } kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v; }
