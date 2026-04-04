#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KEY_IOCTL_GET_KEY _IOR('k', 3, char)

int main(void)
{
    int fd = open("/dev/test", O_RDWR);
    char key = 0;

    if (fd < 0) {
        fprintf(stderr, "open /dev/test failed: %s\n", strerror(errno));
        return 1;
    }

    if (ioctl(fd, KEY_IOCTL_GET_KEY, &key) != 0) {
        fprintf(stderr, "KEY_IOCTL_GET_KEY failed: %s\n", strerror(errno));
        close(fd);
        return 2;
    }

    if (key == 0) {
        puts("no key");
    } else {
        printf("key=%c\n", key);
    }

    close(fd);
    return 0;
}
