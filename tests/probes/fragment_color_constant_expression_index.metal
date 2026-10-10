// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0[^0-9]
// DISASM-MATCH: OpDecorate %[0-9]+ Location 3[^0-9]
// DISASM-NO-MATCH: Location [12][^0-9]
//
// [[color(n)]]'s index is a constant expression, not only a literal, the
// same as [[attribute(n)]] already accepts (see
// enum_as_field_attribute_index_reaches_the_stage_check.metal). An
// enumerator names both colors here: First is Location 0, Second is
// Location 3. The NO-MATCH pin rules out 1 and 2; the trailing [^0-9]
// keeps "Location 3" from also matching a hypothetical "Location 30"
// (CMake's regex engine has no \b word-boundary support).
#include <metal_stdlib>
using namespace metal;
enum Idx { First, Second = 3 };
struct Out { float4 a [[color(First)]]; float4 b [[color(Second)]]; };
fragment Out fragment_color_constant_expression_index()
{ Out o; o.a = float4(1.0); o.b = float4(2.0); return o; }
