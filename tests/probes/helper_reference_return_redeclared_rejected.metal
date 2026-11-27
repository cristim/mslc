// EXPECT: error is declared again with another return type
// A prototype that returns a reference and a definition that returns a value differ only in their return type,
// which C++ does not allow ([over.load]); Apple's compiler refuses it. Taken as two declarations of one function,
// the definition's value would let "self(s) = S{9}" assign a temporary and drop the store.
#include <metal_stdlib>
using namespace metal;
struct S { int v; };
thread S& self(thread S& s);
S self(thread S& s) { return s; }
kernel void helper_reference_return_redeclared_rejected(device int* out [[buffer(0)]]) { S s{1}; self(s) = S{9}; out[0] = s.v; }
