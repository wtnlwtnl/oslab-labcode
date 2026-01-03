#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#include <stat.h>
#include <dirent.h>


#define MAX_ARGS            5

static inline int
syscall(uint64_t num, ...) {
    va_list ap;
    va_start(ap, num);
    uint64_t a[MAX_ARGS];
    int i;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);

    register uint64_t a0 asm("a0") = num;
    register uint64_t a1 asm("a1") = a[0];
    register uint64_t a2 asm("a2") = a[1];
    register uint64_t a3 asm("a3") = a[2];
    register uint64_t a4 asm("a4") = a[3];
    register uint64_t a5 asm("a5") = a[4];

    asm volatile (
        "ecall"
        : "+r" (a0), "+r" (a1), "+r" (a2), "+r" (a3), "+r" (a4), "+r" (a5)
        :
        : "memory"
    );
    return (int)a0;
}

int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, error_code);
}

int
sys_fork(void) {
    return syscall(SYS_fork);
}

int
sys_wait(int64_t pid, int64_t *store) {
    return syscall(SYS_wait, pid, store);
}

int
sys_yield(void) {
    return syscall(SYS_yield);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, pid);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, c);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir);
}

void
sys_lab6_set_priority(uint64_t priority)
{
    syscall(SYS_lab6_set_priority, priority);
}

int
sys_sleep(int64_t time) {
    return syscall(SYS_sleep, time);
}

int
sys_gettime(void) {
    return syscall(SYS_gettime);
}

int
sys_exec(const char *name, int64_t argc, const char **argv) {
    return syscall(SYS_exec, name, argc, argv);
}

int
sys_open(const char *path, uint64_t open_flags) {
    return syscall(SYS_open, path, open_flags);
}

int
sys_close(int64_t fd) {
    return syscall(SYS_close, fd);
}

int
sys_read(int64_t fd, void *base, size_t len) {
    return syscall(SYS_read, fd, base, len);
}

int
sys_write(int64_t fd, void *base, size_t len) {
    return syscall(SYS_write, fd, base, len);
}

int
sys_seek(int64_t fd, off_t pos, int64_t whence) {
    return syscall(SYS_seek, fd, pos, whence);
}

int
sys_fstat(int64_t fd, struct stat *stat) {
    return syscall(SYS_fstat, fd, stat);
}

int
sys_fsync(int64_t fd) {
    return syscall(SYS_fsync, fd);
}

int
sys_getcwd(char *buffer, size_t len) {
    return syscall(SYS_getcwd, buffer, len);
}

int
sys_getdirentry(int64_t fd, struct dirent *dirent) {
    return syscall(SYS_getdirentry, fd, dirent);
}

int
sys_dup(int64_t fd1, int64_t fd2) {
    return syscall(SYS_dup, fd1, fd2);
}
