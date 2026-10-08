struct S { unsigned a; unsigned int b; signed c; signed int d; unsigned char e; signed char f; unsigned short g; short int h; signed short i; unsigned short int j; long int k; unsigned long l; unsigned long int m; signed long n; };
kernel void k(device S* o [[buffer(0)]], const unsigned int t [[thread_position_in_grid]])
{
    o[t].a = 1; o[t].b = 2; o[t].c = 3; o[t].d = 4; o[t].e = 5; o[t].f = 6; o[t].g = 7;
    o[t].h = 8; o[t].i = 9; o[t].j = 10; o[t].k = 11; o[t].l = 12; o[t].m = 13; o[t].n = 14;
}
