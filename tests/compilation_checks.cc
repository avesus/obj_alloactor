#include <allocator/slab_layout/dynamic_slab_manager.h>
#include <allocator/slab_layout/fixed_slab_manager.h>


int
main() {
    fixed_slab_manager<uint64_t, 0, 1>       f1_1;
    fixed_slab_manager<uint64_t, 0, 4>       f1_4;
    fixed_slab_manager<uint64_t, 1, 1, 4>    f2_11;
    fixed_slab_manager<uint64_t, 1, 1, 4>    f2_14;
    fixed_slab_manager<uint64_t, 1, 4, 4>    f2_44;
    fixed_slab_manager<uint64_t, 2, 1, 1, 1> f3_111;
    fixed_slab_manager<uint64_t, 2, 1, 4, 4> f3_144;
    fixed_slab_manager<uint64_t, 2, 4, 4, 4> f3_444;

    dynamic_slab_manager<uint64_t, 0, reclaim_policy::SHARED, 1>       d1_1;
    dynamic_slab_manager<uint64_t, 0, reclaim_policy::SHARED, 4>       d1_4;
    dynamic_slab_manager<uint64_t, 1, reclaim_policy::SHARED, 1, 1>    d2_11;
    dynamic_slab_manager<uint64_t, 1, reclaim_policy::SHARED, 1, 4>    d2_14;
    dynamic_slab_manager<uint64_t, 1, reclaim_policy::SHARED, 4, 4>    d2_44;
    dynamic_slab_manager<uint64_t, 2, reclaim_policy::SHARED, 1, 4, 4> d3_111;
    dynamic_slab_manager<uint64_t, 2, reclaim_policy::SHARED, 1, 4, 4> d3_144;
    dynamic_slab_manager<uint64_t, 2, reclaim_policy::SHARED, 4, 4, 4> d3_444;
}
