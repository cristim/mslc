// EXPECT: valid
// DISASM-NOT: OpCapability Int64
// DISASM-MATCH: OpSpecConstantOp %int Select
// A 32-bit limit is an int literal, so it takes part in a function constant's derived initialiser and needs no
// 64-bit integers.
#include <metal_stdlib>
using namespace metal;
constant int fc [[function_constant(0)]];
constant int v = is_function_constant_defined(fc) ? fc : numeric_limits<int>::max();
kernel void numeric_limits_function_constant_initializer(device int* out [[buffer(0)]]) {
    out[0] = v + numeric_limits<short>::min() + int(numeric_limits<uint>::max());
}
