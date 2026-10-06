// EXPECT: error an inline namespace is not supported
// Apple accepts this; mslc rejects it rather than lower it.
#include <metal_stdlib>
using namespace metal;
inline namespace Q { constant float k = 1.0; }
