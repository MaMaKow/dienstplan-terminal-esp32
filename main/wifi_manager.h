#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
void wifi_init(void);
bool wifi_is_connected(void);
#ifdef __cplusplus
}
#endif
