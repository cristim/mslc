// EXPECT: valid
// RUN: grid=1
// numeric_limits<T>::min(), max(), lowest(), epsilon() and infinity() of a scalar T, with C++'s values: min() of a
// floating-point type is its least positive normal. Apple's compiler agrees on each value below, by static_assert
// (metal 32023.883, macOS 26). The result is a T: an auto local of numeric_limits<short>::max() is 2 bytes. ANGLE's
// ftoi clamps a float to the integer's range this way. On an RTX 3060 out is
// -2147483648 2147483647 -1 -32768 65535 -128 255 1 2 8388608 2139095039 -8388609 872415232 31743 2139095040
// 2147483647 -2147483648 -7 0.
#include <metal_stdlib>
using namespace metal;
template <typename X, typename Y>
X ftoi(Y y) { auto min = metal::numeric_limits<X>::min(); auto max = metal::numeric_limits<X>::max(); return X(metal::clamp(y, Y(min), Y(max))); }
kernel void numeric_limits_scalar(device int* out [[buffer(0)]]) {
    out[0] = numeric_limits<int>::min();
    out[1] = numeric_limits<int>::max();
    out[2] = int(numeric_limits<uint>::max());
    out[3] = numeric_limits<short>::min();
    out[4] = numeric_limits<ushort>::max();
    out[5] = numeric_limits<char>::min();
    out[6] = numeric_limits<uchar>::max();
    out[7] = numeric_limits<bool>::max() ? 1 : 0;
    auto s = numeric_limits<short>::max();
    out[8] = int(sizeof(s));
    out[9] = as_type<int>(numeric_limits<float>::min());
    out[10] = as_type<int>(numeric_limits<float>::max());
    out[11] = as_type<int>(numeric_limits<float>::lowest());
    out[12] = as_type<int>(numeric_limits<float>::epsilon());
    out[13] = int(as_type<ushort>(numeric_limits<half>::max()));
    out[14] = as_type<int>(metal::numeric_limits<float>::infinity());
    out[15] = ftoi<int>(3.0e10f);
    out[16] = ftoi<int>(-3.0e10f);
    out[17] = ftoi<int>(-7.5f);
    out[18] = int(ftoi<uint>(-5.0f));
}
