// EXPECT: error typedef redefinition with different types ('int' vs 'Foo')
//
// The issue (#257): a plain-type typedef redefining an enum's own tag name is
// silently accepted today, dropping the enum's identity. Apple's diagnostic is
// quoted by the issue as "typedef redefinition with different types" (unverified
// here: no Apple toolchain reachable on this machine, quoted from the issue text
// only); the operand spelling is this project's own.
#include <metal_stdlib>
using namespace metal;
enum Foo { X };
typedef int Foo;
kernel void typedef_int_over_enum_tag_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
