#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <lkl.h>
#include <lkl_host.h>

#define FULL_DISK      0
#define MOUNT_FLAGS    0
#define MOUNT_OPTS     NULL
#define PAGE_SIZE      4096

static int blk_busy = 0;

int fi_is_blk_busy()
{
        return  blk_busy == 1;
}

static void lkl_write(int fd, char *buf, int len, char *fpath)
{
        int ret;

        ret = lkl_sys_write(fd, buf, len);
        if (ret < 0) {
                fprintf(stderr, "write file %s failed: %s\n",
                        fpath, lkl_strerror(ret));
                close(fd);
        }
        fprintf(stderr, "lkl_sys_write finished!\n");

        ret = lkl_sys_fdatasync(fd);
        if (ret < 0) {
                fprintf(stderr, "fdatasync file %s failed: %s\n",
                        fpath, lkl_strerror(ret));
        }
        fprintf(stderr, "lkl_sys_fdatasync finished!\n");
}

void *writer(void *arg)
{
        char wbuf1[PAGE_SIZE] = { '0' };
        char wbuf2[PAGE_SIZE] = { '0' };
        int i;
        char *fpath = arg;
        int fd;
        int ret;

        fd = lkl_sys_open(fpath, LKL_O_RDWR | LKL_O_APPEND | LKL_O_CREAT, 0755);
        if (fd < 0) {
                fprintf(stderr, "open file %s for writing failed: %s\n",
                        fpath, lkl_strerror(fd));
        }
        fprintf(stderr, "lkl_sys_open finished!\n");

        for (i = 0; i < PAGE_SIZE; i++) {
                wbuf1[i] = '1';
        }
        // fprintf(stderr, "wbuf1: %s\n", wbuf1);
        lkl_write(fd, wbuf1, PAGE_SIZE, fpath);

        blk_busy = 1;

        for (i = 0; i < PAGE_SIZE; i++) {
                wbuf2[i] = '2';
        }
        // fprintf(stderr, "wbuf2: %s\n", wbuf2);
        lkl_write(fd, wbuf2, PAGE_SIZE, fpath);

        return NULL;
}

int main(void)
{
        char mnt_point[] = "/mnt/placeholder";
        char fpath[200] = { '0' };
        char rbuf[PAGE_SIZE * 2] = { '0' };
        pthread_attr_t tattr;
        pthread_t tid;
        struct lkl_disk disk;
        int disk_id;
        int fd;
        int ret;

        disk.fd = open("disk.img", O_RDWR);
        if (disk.fd < 0) {
                fprintf(stderr, "open disk.img failed: %s\n", strerror(errno));
                return -1;
        }

        disk.ops = NULL;
        ret = lkl_disk_add(&disk);
        if (ret < 0) {
                fprintf(stderr, "add disk failed: %s\n", lkl_strerror(ret));
                return -1;
        }

        disk_id = ret;

        lkl_start_kernel(&lkl_host_ops, "mem=1000M");

        ret = lkl_mount_dev(disk_id, FULL_DISK, "ext4", MOUNT_FLAGS, MOUNT_OPTS,
                        mnt_point, sizeof(mnt_point));
        if (ret < 0) {
                fprintf(stderr,  "mount disk failed: %s\n", lkl_strerror(ret));
                return  -1;
        }

        lkl_sys_umask(0);

        snprintf(fpath, 200, "%s/file", mnt_point);
        fprintf(stderr, "fpath: %s\n", fpath);

        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &tattr, writer, fpath);

        sleep(10);
        // do {
        //         ret = pthread_cancel(tid);
        // } while (ret == 0);
        // fprintf(stderr, "pthread_cancel: %s\n", lkl_strerror(ret));

        // fd = lkl_sys_open(fpath, LKL_O_RDONLY, 0);
        // if (fd < 0) {
        //         fprintf(stderr, "open file %s for reading failed: %s\n",
        //                 fpath, lkl_strerror(fd));
        //         return -1;
        // }

        // fprintf(stderr, "begin to read file\n");
        // ret = lkl_sys_read(fd, rbuf, PAGE_SIZE * 2);
        // if (ret < 0) {
        //         fprintf(stderr, "read file %s failed: %s\n",
        //                 fpath, lkl_strerror(fd));
        //         return -1;
        // }

        // fprintf(stderr, "read %d, data: %s\n", ret, rbuf);

        // sleep(10);

        close(disk.fd);

        lkl_sys_halt();

        return ret;
}

