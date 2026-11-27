// EXPECT: error scoped enum metal::component
#include <metal_stdlib>
fragment int hit() { return metal::component::x; }
