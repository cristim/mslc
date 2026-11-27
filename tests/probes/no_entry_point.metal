// EXPECT: no module
// REFLECT: "entry_points": [
// REFLECT-NOT: "name"
//
// The two Xcode Metal templates are this source with nothing but a
// metal_stdlib include, and Apple's compiler accepts both: it builds a library
// with no functions in it. A Vulkan module needs an entry point (spirv-val
// rejects one without OpEntryPoint unless it declares the Linkage capability,
// which Vulkan does not allow), so the library has no module, and the
// reflection lists no entry point.
struct Unused { uint x; };
