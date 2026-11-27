// EXPECT: error redefinition of "metal::discard_fragment": the standard library defines it with these parameters
// The zero-argument discard_fragment is the library's (Apple: "redefinition of
// 'discard_fragment'"); discard_fragment(int) is an overload and stays accepted.
#include <metal_stdlib>
namespace metal { void discard_fragment() {} }
fragment float4 namespace_metal_redefines_discard_fragment_rejected() { return float4(0); }
