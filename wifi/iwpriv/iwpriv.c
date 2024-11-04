#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUF_SIZE 128

static void iw_help(void)
{
    printf("##########\n");
    printf("input parameters should like this: iwpriv XXX XXX\n");
    printf("##########\n");
}

static int iw_exec_cmd(const char* cmd) {
    pid_t sta = system(cmd);
    if (sta == -1) {
        printf("system %s error\n", cmd);
        return -1;
    } else {
        if (WIFEXITED(sta)) {
            if (0 == WEXITSTATUS(sta)) {
                return 0;
            } else {
                return -1;
            }
        } else {
            printf("exit status = [%d]\n", WEXITSTATUS(sta));
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int i = 0;
    char ibuf[BUF_SIZE];
    const char* cmd = "rtwpriv";
    const char* ifname = "wlan0";

    if (argc < 2) {
        printf("iwpriv no enough parameters!\n");
        iw_help();
        return -EINVAL;
    }

    sprintf(ibuf, "%s %s", cmd, ifname);
    for (i = 1; i < argc; i++) {
        strcat(ibuf, " ");
        strcat(ibuf, argv[i]);
    }

    if (iw_exec_cmd(ibuf) < 0) {
        printf("%s failed\n", ibuf);
        return -1;
    }

    return ret;
}
