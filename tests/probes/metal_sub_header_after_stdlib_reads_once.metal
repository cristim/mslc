// EXPECT: valid
// DISASM: OpCapability Shader
//
// A sub-header after #import <metal_stdlib> is the same built-in header, already read: it does not
// define METAL_ARG_UNIFORM again once the shader has removed it.
#import <metal_stdlib>
#undef METAL_ARG_UNIFORM
#include <metal_math>
#ifdef METAL_ARG_UNIFORM
#error the sub-header was read a second time
#endif
kernel void metal_sub_header_after_stdlib_reads_once(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
