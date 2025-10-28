#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/wireless.h>

#define BUF_SIZE 4096

#define RTW_IOCTL_MP 	SIOCDEVPRIVATE + 1

static void rtw_help(void)
{
    printf("##########\n");
    printf("input parameters should like this: rtwpriv wlan0 XXX XXX\n");
    printf("##########\n");
}

int main(int argc, char **argv)
{
    int ret = 0;
    int sock;
    char ibuf[BUF_SIZE];
    char* ifname;
    struct ifreq ifr;
    union iwreq_data u;
    int i = 0;

    if (argc < 3) {
        printf("rtwpriv no enough parameters!\n");
        rtw_help();
        return -EINVAL;
    }

    ifname = argv[1];

    sprintf(ibuf, "%s", argv[2]);
    for (i = 3; i < argc; i++) {
        strcat(ibuf, " ");
        strcat(ibuf, argv[i]);
        //sprintf(ibuf, "%s %s", ibuf, argv[i]);
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("create socket error\n");
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, ifname);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
        printf("could not read interface %s flags: %s", ifname, strerror(errno));
        ret = -1;
        goto err;
    }

    if (!(ifr.ifr_flags & IFF_UP)) {
        printf("%s is not up!\n", ifname);
        ret = -1;
        goto err;
    }

    memset(&u, 0, sizeof(union iwreq_data));
    u.data.pointer = ibuf;
    u.data.length = strlen(ibuf) + 1;

    ifr.ifr_data = (void*)&u;

    if (ioctl(sock, RTW_IOCTL_MP, &ifr) < 0) {
        printf("ioctl error\n");
        ret = -1;
        goto err;
    }
    printf("\nPrivate Message: %s\n", ibuf);

err:
    close(sock);
    return ret;
}
