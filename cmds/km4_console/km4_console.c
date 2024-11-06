#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#define CONSOLE_MAX_CHAR    128

/* Data struct which should be sync to KM4. */
typedef struct {
    char  BufCount;                         //record the input cmd char number.
    char  UARTLogBuf[CONSOLE_MAX_CHAR];     //record the input command.
} UART_LOG_BUF, *PUART_LOG_BUF;

static void pabort(const char *s)
{
    perror(s);
    abort();
}

static void usage() {
    printf("Usage:\nProgramName -c \"cmd\"\n");
}

int main(int argc, char **argv)
{
    int ret = 0;
    UART_LOG_BUF instruction;
    memset(instruction.UARTLogBuf, 0, sizeof(instruction.UARTLogBuf));

    int fd = open("/dev/console-ctrl", 2); // 2: O_RDWR   1:WR only
    if (fd == -1) {
        pabort("Can't open device");
    }

    if (argc < 2) {
        do {
            memset(instruction.UARTLogBuf, 0, sizeof(instruction.UARTLogBuf));
            fgets((char *)instruction.UARTLogBuf, CONSOLE_MAX_CHAR, stdin);

	    if (instruction.UARTLogBuf[0] == '\n') {
		char a = 0xd;
		char b = 0xa;
		printf("%c%c#", a, b); 
		continue;
	    }

	    instruction.UARTLogBuf[strlen(instruction.UARTLogBuf)-1]='\0';//or here is the carrier return '\r'
	    if(strcmp((const char *)instruction.UARTLogBuf, (const char *)("exit")) == 0) {
                break;
            } else if (strlen(instruction.UARTLogBuf) != 0) {
                instruction.BufCount = strlen(instruction.UARTLogBuf);
                ret = ioctl(fd, 0, &instruction);
            }
        } while(1);
        close(fd);
        return ret;
    }

    bool cmd_flag = false;

    int i = 1;
    for (; i < argc; i++ ) {
        if (strcmp(argv[i], "-c") == 0) {
            cmd_flag = true;
            break;
        }
    }

    if (cmd_flag) {
        i++;
        int copy_size = strlen(argv[i]);
        if (copy_size > CONSOLE_MAX_CHAR - 1)
            copy_size = CONSOLE_MAX_CHAR - 1;
        memcpy(instruction.UARTLogBuf, argv[i], copy_size);
        instruction.UARTLogBuf[CONSOLE_MAX_CHAR - 1] = '\0';
        //printf("%s\n", instruction.UARTLogBuf);
    } else {
        usage();
    }

    ret = ioctl(fd, 0, &instruction);

    close(fd);
    return ret;
}
