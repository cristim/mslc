// EXPECT: valid
// DISASM-NOT: SpecId 65538
// DISASM-NOT: Float16
// REFLECT: { "name": "through_helper", "type": "int", "index": 0,
// REFLECT: { "name": "direct", "type": "int", "index": 1,
// REFLECT-NOT: "hidden"
// REFLECT-NOT: "unused"
//
// An entry point's reflection lists the function constants it reads, directly or in a function it
// calls, as Apple's functionConstantsDictionary does; one a local hides is not read. A constant
// nothing reads is not declared in the module, so its half type asks for no Float16 capability.
// Kernel "other" reads none, so its reflection lists none (one "function_constants" list in all).
#include <metal_stdlib>
using namespace metal;
constant int through_helper [[function_constant(0)]];
constant int direct [[function_constant(1)]];
constant half unused [[function_constant(2)]];
constant int hidden [[function_constant(3)]];
int helper() { return through_helper + 1; }
kernel void k(device int* out [[buffer(0)]]) {
    int hidden = 4;
    out[0] = helper() + direct + hidden;
}
kernel void other(device int* out [[buffer(0)]]) {
    out[0] = 2;
}
