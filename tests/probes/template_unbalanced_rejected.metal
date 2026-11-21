// EXPECT: error after the template function
// Refs #341: a template whose body never ends is a diagnostic, not a hang.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x)
