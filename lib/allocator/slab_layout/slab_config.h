#ifndef _SLAB_CONFIG_
#define _SLAB_CONFIG_

#define SAFER_FREE
//#define CONTINUE_RSEQ

enum reclaim_policy {
    PERCPU = 0,  // This will result in faster freeing but slower reclaiming
    SHARED = 1   // this will result in slower freeing but faster reclaiming
};


#endif
