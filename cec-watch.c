#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define fatal(msg...) { \
        fprintf(stderr, msg); \
        fprintf(stderr, " [%s(), %s:%u]\n", __FUNCTION__, __FILE__, __LINE__); \
        exit(1); \
        }

int create_input_fd()
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fatal("Cannot open /dev/uinput");
    }
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        fatal("UI_SET_EVBIT");
    }
    for (int i = 0 ; i < 256 ; i++) {
        if (ioctl(fd, UI_SET_KEYBIT, i) < 0) {
            fatal("UI_SET_KEYBIT");
        }
    }
    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "xakcop kbd");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0xdead;
    uidev.id.product = 0xbeef;
    uidev.id.version = 1;
    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        fatal("cannot write to /dev/uinput");
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        fatal("UI_DEV_CREATE");
    }
    return fd;
}

void emit(int fd, int type, int code, int val)
{
    struct input_event ie;

    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    write(fd, &ie, sizeof(ie));
}

int main(int argc, char *argv[])
{
    int fd = create_input_fd();
    printf("Created input fd=%d\n", fd);
    sleep(1);

    emit(fd, EV_KEY, KEY_F11, 1);
    emit(fd, EV_SYN, SYN_REPORT, 0);
    emit(fd, EV_KEY, KEY_F11, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);

    sleep(1);
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);

    return 0;
}
