#include <cstdio>
int main(){__builtin_cpu_init();printf("avx2=%d avx512f=%d avx512vl=%d avx512dq=%d\n",!!__builtin_cpu_supports("avx2"),!!__builtin_cpu_supports("avx512f"),!!__builtin_cpu_supports("avx512vl"),!!__builtin_cpu_supports("avx512dq"));}
