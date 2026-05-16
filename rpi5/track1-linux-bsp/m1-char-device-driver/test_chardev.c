/*
 * test_chardev.c - RPi5 Track 1, M1: Userspace test for chardev driver
 *
 * Tests: open, write, read, llseek, ioctl (CLEAR + GET_LEN)
 *
 * Compile: gcc -o test_chardev test_chardev.c
 * Run:     sudo ./test_chardev
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE        "/dev/chardev"
#define CHARDEV_IOC_MAGIC  'k'
#define CHARDEV_CLEAR      _IO(CHARDEV_IOC_MAGIC,  0)
#define CHARDEV_GET_LEN    _IOR(CHARDEV_IOC_MAGIC, 1, int)

#define PASS "\033[32mPASS\033[0m"
#define FAIL "\033[31mFAIL\033[0m"

static int fd;

static void test_open(void)
{
    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open " DEVICE);
        fprintf(stderr, "Make sure chardev.ko is loaded and you are root.\n");
        exit(1);
    }
    printf("[%s] open /dev/chardev (fd=%d)\n", PASS, fd);
}

static void test_write(const char *msg)
{
    ssize_t n = write(fd, msg, strlen(msg));
    if (n < 0) {
        perror("write");
        printf("[%s] write\n", FAIL);
        return;
    }
    printf("[%s] write %zd bytes: \"%s\"\n", PASS, n, msg);
}

static void test_read(void)
{
    char buf[4096] = {0};
    lseek(fd, 0, SEEK_SET);   /* rewind first */
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        printf("[%s] read\n", FAIL);
        return;
    }
    printf("[%s] read %zd bytes: \"%s\"\n", PASS, n, buf);
}

static void test_ioctl_get_len(void)
{
    int len = 0;
    if (ioctl(fd, CHARDEV_GET_LEN, &len) < 0) {
        perror("ioctl GET_LEN");
        printf("[%s] ioctl GET_LEN\n", FAIL);
        return;
    }
    printf("[%s] ioctl GET_LEN = %d bytes\n", PASS, len);
}

static void test_ioctl_clear(void)
{
    if (ioctl(fd, CHARDEV_CLEAR) < 0) {
        perror("ioctl CLEAR");
        printf("[%s] ioctl CLEAR\n", FAIL);
        return;
    }
    printf("[%s] ioctl CLEAR — buffer wiped\n", PASS);
}

static void test_read_after_clear(void)
{
    char buf[64] = {0};
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n == 0)
        printf("[%s] read after CLEAR returns EOF (n=0) as expected\n", PASS);
    else
        printf("[%s] read after CLEAR returned %zd bytes (expected 0)\n", FAIL, n);
}

static void test_llseek(void)
{
    const char *msg = "SeekTest";
    write(fd, msg, strlen(msg));

    /* seek to offset 4, read remaining */
    lseek(fd, 4, SEEK_SET);
    char buf[32] = {0};
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0 && strcmp(buf, "Test") == 0)
        printf("[%s] llseek SEEK_SET(4) → read \"%s\"\n", PASS, buf);
    else
        printf("[%s] llseek got \"%s\" (expected \"Test\")\n", FAIL, buf);
}

int main(void)
{
    printf("\n=== chardev driver test ===\n\n");

    test_open();
    test_write("Hello from userspace!");
    test_read();
    test_ioctl_get_len();
    test_llseek();
    test_ioctl_clear();
    test_read_after_clear();

    close(fd);
    printf("\n=== done — check dmesg for kernel side logs ===\n\n");
    return 0;
}
