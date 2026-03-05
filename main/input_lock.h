#ifndef INPUT_LOCK_H
#define INPUT_LOCK_H

#include <stdbool.h>

typedef enum {
    INPUT_NONE   = 0,
    INPUT_ESPNOW = 1,
    INPUT_UDP    = 2,
} input_source_t;

// Attempt to claim the input lock for the given source.
// Returns true if this source owns the lock (claimed now or already owned).
// Returns false if a different source already owns the lock.
bool input_lock_try_claim(input_source_t source);

// Return the currently locked input source (INPUT_NONE if unlocked).
input_source_t input_lock_get(void);

#endif
