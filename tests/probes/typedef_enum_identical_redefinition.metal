// EXPECT: valid
//
// Accept guard for the #257 fix: redeclaring an existing typedef of an enum
// with the SAME enum identity must still compile, covering three generic-path
// shapes the enumName comparison must not over-reject: an enum redeclared as a
// typedef of itself, an enum alias repeated, and an anonymous-enum typedef
// repeated. enum_typedef_forms.metal covers only the `isEnum` branch forms
// (`typedef enum E ...`), not these generic-path ones.
#include <metal_stdlib>
using namespace metal;
enum Mode { A };
typedef Mode Mode;
typedef Mode Alias;
typedef Mode Alias;
typedef enum { X } Anon;
typedef Anon Anon;
kernel void typedef_enum_identical_redefinition(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Alias m = A; out[i] = m == A ? 1 : 0; }
