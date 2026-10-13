// EXPECT: error the call to "A::clamp" also matches the standard library's "clamp"
// Namespace B's own "using metal::clamp;" brings the library's clamp to file scope
// only transitively, through "using namespace B;" (no direct using-declaration or
// using-namespace-metal at file scope, and B itself declares nothing named clamp).
// The library's names are still visible beside A::clamp there (Apple calls the
// library's), which collectMembers's besideStdlib has to check for a scope's own
// stdlibDeclarations, not just "scope == kStdlibNamespace", to catch.
#include <metal_stdlib>
namespace B { using metal::clamp; }
namespace A { float clamp(float x, int lo, int hi) { return 7.0f; } }
using namespace A;
using namespace B;
kernel void stdlib_name_transitive_stdlib_declaration_rejected(device float* in [[buffer(0)]], device float* out [[buffer(1)]])
{ out[0] = clamp(in[0], 0.0f, 2.0f); }
