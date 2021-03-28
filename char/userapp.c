#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>  

int main(void)
{
    int fd;
    int res;
    char buf[] = "Houmin says hello to scull!";
    int buflen = sizeof(buf);
    char buf2[100];
    fd = open("/dev/scull0", O_RDWR);
    if (fd < 0) {
        printf("Open scull device failed\n");
    }

    res = write(fd, buf, buflen);
    printf("Write to scull device, content: %s\n", buf);

    lseek(fd, 0, SEEK_SET);
    res = read(fd, buf2, buflen);
    buf2[buflen] = '\0';
    printf("Read from scull device: %s\n", buf2);

    close(fd);
    return 0;
}
