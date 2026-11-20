// EXPECT: valid
// DISASM: OpFDiv
// DISASM-NOT: OpSDiv
// Refs #341: a default of a member template that names the class template's T means the instance's T (float), not the file-scope typedef of that name (int).
#include <metal_stdlib>
using namespace metal;
typedef int T;
template <typename T> struct Box { T v; template <typename U = T> U half_of() const { return U(v) / U(2); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Box<float> b{3.0f}; out[i] = b.half_of(); }
