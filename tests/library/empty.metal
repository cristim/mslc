// A genuinely empty translation unit: what the untouched Xcode Metal File and
// Metal Library Base templates contain. Comments, an include that expands to
// nothing an empty library needs, and a using directive.
//
// This is not a Vulkan executable. It compiles to a Universal SPIR-V 1.5
// Linkage object with no entry points, which is what --compile-library emits
// and what the default mode refuses.

//___FILEHEADER___

#include <metal_stdlib>
using namespace metal;
