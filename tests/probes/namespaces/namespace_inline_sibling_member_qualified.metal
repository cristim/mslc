// EXPECT: valid
// DISASM: OpFAdd
// A::J::u and A::I::v name members of two inline namespaces by their own names.
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { constant float v = 1; } inline namespace J { constant float u = 2; } } kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::J::u + A::I::v; }
