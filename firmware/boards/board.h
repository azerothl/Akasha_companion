#pragma once

#if !defined(BOARD_FNK0104B) && !defined(BOARD_FNK0104N) && !defined(BOARD_FNK0104S) && \
    !defined(BOARD_FNK0104A)
#define BOARD_FNK0104B 1
#endif

#if defined(BOARD_FNK0104B)
#include "fnk0104b.h"
#elif defined(BOARD_FNK0104N)
#include "fnk0104n.h"
#elif defined(BOARD_FNK0104S)
#include "fnk0104s.h"
#elif defined(BOARD_FNK0104A)
#include "fnk0104a.h"
#else
#error "Define BOARD_FNK0104B (or N/S/A)"
#endif
