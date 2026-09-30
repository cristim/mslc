// EXPECT: error has whitespace in the header name
//
// The header name is recovered by concatenating the directive line's tokens,
// because an angled include arrives as a run of them ("<simd/simd.h>" is Less,
// simd, Slash, simd, Dot, h, Greater) and a quoted one as a single identifier
// that keeps its quotes and its path. Concatenation discards whitespace, so a
// spaced name comes back as the same string as an unspaced one: "<metal_std
// lib>" becomes "<metal_stdlib>", and that is the one header mslc honours, so
// the spaced form would be accepted as a source that includes nothing at all.
//
// Apple's compiler treats the whitespace as part of the name it looks for, and
// reports "' metal_stdlib ' file not found, did you mean 'metal_stdlib'?", so
// rejecting is the reading that agrees with it rather than the strict one. The
// probe uses the split form because it is the more surprising of the two; the
// spacing at either end of an angled name, "< metal_stdlib >", takes the same
// path and gives the same answer.
//
// The message names the line rather than a header name, because the text the
// concatenation would have quoted is not the name that was written.
#include <metal_std lib>
kernel void include_with_whitespace_in_the_name(device float *out [[buffer(0)]],
                                                uint index [[thread_position_in_grid]])
{ out[index] = 1.0; }
