#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define fatal(msg...) { \
        fprintf(stderr, msg); \
        fprintf(stderr, " [%s(), %s:%u]\n", __FUNCTION__, __FILE__, __LINE__); \
        exit(1); \
        }

#define PIPE_READ 0
#define PIPE_WRITE 1

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

int read_buffer(int fd, char *buffer, int size)
{
    int count = 0;
    while (size > 0) {
       int r = read(fd, buffer+count, size);
       if (r <= 0) {
           break;
       }
       count += r;
       size -= r;
    }
    return count;
}

int get_status()
{
    int result = -1;
    int stdin_pipe[2];
    int stdout_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        fatal("cannot create stdin pipe");
    }
    if (pipe(stdout_pipe) < 0) {
        fatal("cannot create stdout pipe");
    }
    printf("Starting cec-client ...\n");
    pid_t cpid = fork();
    if (cpid == 0) {
        // child
        if (dup2(stdin_pipe[PIPE_READ], STDIN_FILENO) == -1) {
            fatal("cannot dup2 stdin");
        }
        if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1) {
            fatal("cannot dup2 stdin");
        }
        close(stdin_pipe[PIPE_READ]);
        close(stdin_pipe[PIPE_WRITE]);
        close(stdout_pipe[PIPE_READ]);
        close(stdout_pipe[PIPE_WRITE]);
        char *argv[] = { "cec-client", "-s", "-d", "1", NULL };
        char *environ[] = { NULL };
        execve("/usr/bin/cec-client", argv, environ);
    } else if (cpid > 0) {
        // parent
        close(stdin_pipe[PIPE_READ]);
        close(stdout_pipe[PIPE_WRITE]);
        const char *pow_cmd = "pow 0\n";
        write(stdin_pipe[PIPE_WRITE], pow_cmd, strlen(pow_cmd));
        char buffer[1024];
        int count = read_buffer(stdout_pipe[PIPE_READ], buffer, sizeof(buffer)-1);
        buffer[count] = 0;
        printf(">>>\n%s\n<<<\n", buffer);
        if (strstr(buffer, "power status: on") != NULL) {
            result = 1;
        }
        if (strstr(buffer, "power status: standby") != NULL) {
            result = 0;
        }
        int status = 0;
        waitpid(cpid, &status, 0);
        printf("cec-client finished, status=%d\n", status);
        close(stdin_pipe[PIPE_WRITE]);
        close(stdout_pipe[PIPE_READ]);
    } else {
        fatal("fork failed");
    }
    return result;
}

int is_hdmi()
{
    char mode[64];
    FILE *f = fopen("/sys/class/display/mode", "r");
    fscanf(f, "%s\n", mode);
    fclose(f);
    return strcmp(mode, "576cvbs") != 0;
}

int main(int argc, char *argv[])
{
    int sleep_time = 300;
    if (argc > 1) {
        sleep_time = atoi(argv[1]);
    }
    int fd = create_input_fd();
    int flag = 0;
    printf("Created input fd=%d\n", fd);
    sleep(1);
    printf("Monitoring CEC status every %d seconds ...\n", sleep_time);
    while (1) {
        if (is_hdmi()) {
            int pow_status = get_status();
            if (pow_status == 0) {
                if (flag == 1) {
                    printf("+++Power status is OFF and flag is set, press F11\n");
                    emit(fd, EV_KEY, KEY_F11, 1);
                    emit(fd, EV_SYN, SYN_REPORT, 0);
                    emit(fd, EV_KEY, KEY_F11, 0);
                    emit(fd, EV_SYN, SYN_REPORT, 0);
                    flag = 0;
                } else {
                    printf("+++Power status is OFF, setting the flag\n");
                    flag = 1;
                }
            } else if (pow_status == 1) {
                printf("+++Power status is ON, clear the flag\n");
                flag = 0;
            }
        }
        sleep(sleep_time);
    }
    //ioctl(fd, UI_DEV_DESTROY);
    //close(fd);
    return 0;
}
