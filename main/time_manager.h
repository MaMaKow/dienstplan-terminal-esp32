#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void ntp_init(void);
void obtain_time(void);
void time_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
