// EXPECT: valid
// A guard, not a reproduction: a backslash-newline splice joins two physical
// lines into one logical line, so it must not advance the line count, or the
// directive's end-of-line bound stops before the spliced token and the kernel
// below is left unconsumed at top level. This passes before the change too;
// it is here because the same code path was nearly changed to do the opposite.
#define A "x\
y"
kernel void splice_string_then_kernel(device uint* out [[buffer(0)]],
                                      uint i [[thread_position_in_grid]])
{ out[i] = i; }
