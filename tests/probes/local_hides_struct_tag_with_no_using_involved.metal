// EXPECT: valid
// #288 (cross-agent review of #264): the original fix only covered the using/directive path;
// #288 itself warned a fix limited to that path would miss the plain-struct case, and it did.
// A local hides a struct tag of the same name even with no namespace or using declaration
// anywhere: looksLikeType's statement-start test called _unit.findStruct(name) without
// consulting isLocal(name), so "S = 3;" after "int S = 2;" parsed as the start of a
// declaration of type S and failed with "expected a variable name, found =". Stores 3, not
// read through the struct type at all.
#include <metal_stdlib>
using namespace metal;
struct S { int y; };
kernel void k(device int* out [[buffer(0)]]) {
	int S = 2;
	S = 3;
	out[0] = S;
}
