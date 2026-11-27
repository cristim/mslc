// EXPECT: valid
// DISASM: OpCapability Shader
//
// The allowlist side of the include rule. <metal_stdlib> carries the MSL
// builtins, which mslc resolves from its own table rather than from a header,
// and every corpus shader opens with it, so rejecting it would reject the whole
// corpus to no purpose. If the allowlist ever shrinks this is the probe that
// notices, and it is the reason the rule is "an include mslc cannot honour"
// rather than "every include".
//
// The kernel is deliberately ordinary. The point is that the directive is
// accepted and the code after it compiles, and the capability decoration is here
// to show a module came out the other side rather than an empty file. The spaced
// spelling of the same include is the other side of this, in
// include_with_whitespace_in_the_name.
#include <metal_stdlib>
kernel void metal_stdlib_include_accepted(device float *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{ out[i] = 1.0; }
