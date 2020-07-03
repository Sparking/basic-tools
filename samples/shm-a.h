#ifndef __SYSM_SHM_QUERY_SERVICE_H_
#define __SYSM_SHM_QUERY_SERVICE_H_

#include <stdint.h>

struct sysmon_query_info_s {
    float last5s_cpu_rate;
    float last1min_cpu_rate;
    float last5min_cpu_rate;
    float last_mem_rate;

    uint64_t uptime_stamp;
};
typedef struct sysmon_query_info_s sysmon_query_info_t;

extern int sysmon_query_buildup(void);

extern int sysmon_query_init(const char *module);

extern int sysmon_query(sysmon_query_info_t *info);

#endif /* End of __SYSM_SHM_QUERY_SERVICE_H_ */
