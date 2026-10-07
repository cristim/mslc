// EXPECT: error declares 1 helper function and no kernel, vertex or fragment entry point
//
// The other half of #109, and the case a Metal shader library is actually
// written in: real helper functions and no entry point. Apple's compiler builds
// a library of them (xcrun metal exits 0 on this source). The rejection counts
// the helpers, so a reader can tell this file apart from one that simply
// declares nothing, and states the Vulkan limit that decides it.
float scale(float x) { return x * 2.0f; }