/* Bootstrap mbed TLS on a machine without an operating-system CSPRNG. */
#include "jitterentropy.h"

int xbox_entropy_seed(unsigned char seed[32])
{
    if (jent_entropy_init())
        return -1;
    struct rand_data *collector = jent_entropy_collector_alloc(0, 0);
    if (!collector)
        return -1;
    int ok = jent_read_entropy(collector, (char *)seed, 32) == 32;
    jent_entropy_collector_free(collector);
    return ok ? 0 : -1;
}
