#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/eventfd.h>

#include "shm-a.h"

#define SYSMON_SHM_PATH                 "/tmp/.sysmon_shm/"
#define SYSMON_SHM_PROJ_ID_QUERY_INFO   1U

sysmon_query_info_t *g_sysmon_query_info;

static int sysmon_shmget(const int shmflag)
{
    key_t key;

    key = ftok(SYSMON_SHM_PATH, SYSMON_SHM_PROJ_ID_QUERY_INFO);
    if (key < 0) {
        return -1;
    }

    return shmget(key, sizeof(struct sysmon_query_info_s), shmflag);
}

sysmon_query_info_t *sysmon_query_buildup(void)
{
    int ret;
    int flag;
    sysmon_query_info_t *info;

    ret = system("mkdir -p "SYSMON_SHM_PATH);
    if (ret != 0) {
        return NULL;
    }

    flag = 0;
    ret = sysmon_shmget(IPC_CREAT | IPC_EXCL | 0644);
    if (ret < 0) {
        flag = 1;
        ret = sysmon_shmget(IPC_CREAT | 0644);
        if (ret < 0) {
            return NULL;
        }
    }

    info = (sysmon_query_info_t *)shmat(ret, NULL, 0);
    if (info == (sysmon_query_info_t *)-1) {
        return NULL;
    }

    if (flag == 0) {
        memset(info, 0, sizeof(*info));
    }

    return info;
}

sysmon_query_info_t *sysmon_query_init(const char *module)
{
    int ret;
    sysmon_query_info_t *info;

    if (module == NULL) {
        return NULL;
    }

    ret = sysmon_shmget(IPC_CREAT | 0644);
    if (ret < 0) {
        return NULL;
    }

    info = (sysmon_query_info_t *)shmat(ret, NULL, SHM_RDONLY);
    if (info == (sysmon_query_info_t *)-1) {
        return NULL;
    }

    return info;
}

