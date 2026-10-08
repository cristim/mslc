struct S { uint a; uchar b; ushort c; short d; ushort e; ulong f; ulong g; ulong h; long i; long j; char k; };
kernel void k(device S* o [[buffer(0)]])
{
    o[0].a = 1; o[0].b = 2; o[0].c = 3; o[0].d = 4; o[0].e = 5; o[0].f = 6; o[0].g = 7; o[0].h = 8; o[0].i = 9; o[0].j = 10; o[0].k = 11;
}
