// EXPECT: valid
// DISASM: OpFAdd
//
// A struct, enum, typedef, constant and helper declared in a namespace are named N::x.
#include <metal_stdlib>
using namespace metal;
namespace N {
  struct S { float a; };
  enum E { X, Y = 5 };
  typedef float4 F4;
  constant float k = 2.0;
  S make(float v) { S s; s.a = v * k; return s; }
}
kernel void kern(device float* out [[buffer(0)]])
{
  N::S s = N::make(1.0);
  N::F4 v = N::F4(s.a, N::k, float(N::Y), float(N::X));
  N::E e = N::Y;
  out[0] = v.x + v.y + v.z + float(e);
}
