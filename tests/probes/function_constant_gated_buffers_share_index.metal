// EXPECT: valid
//
// Two buffers gated by function constants may share buffer(0); each has a binding of its own. Apple's compiler
// accepts this (metal 32023.883, macOS 26). On an RTX 3060, with useFloat set, outF gets 0.5, 1.5, 2.5, 3.5;
// with useInt set, outI gets 0, 3, 6, 9.
#include <metal_stdlib>
using namespace metal;
constant bool useFloat [[function_constant(0)]];
constant bool useInt [[function_constant(1)]];
kernel void k(device float* outF [[buffer(0), function_constant(useFloat)]],
              device int* outI [[buffer(0), function_constant(useInt)]],
              uint i [[thread_position_in_grid]]) {
  if (useFloat) outF[i] = float(i) + 0.5f;
  if (useInt) outI[i] = int(i) * 3;
}
