#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static void fail(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

int main(void)
{
    char directory[] = "ex03_demo_XXXXXX";
    const char *message = "Hello, Linux file programming!\n";
    char buffer[128];
    struct stat info;
    struct dirent *entry;

    printf("실습 3: 파일 및 디렉토리 처리\n");

    /* 실행마다 별도 디렉토리를 생성한다. */
    if (mkdtemp(directory) == NULL)
        fail("mkdtemp");
    printf("[mkdtemp] 작업 디렉토리 생성: %s\n", directory);

    if (chdir(directory) == -1)
        fail("chdir");
    printf("[chdir] 작업 디렉토리로 이동\n");

    char *path = getcwd(NULL, 0);
    if (path == NULL)
        fail("getcwd");
    printf("[getcwd] 현재 경로: %s\n", path);
    free(path);

    int fd = open("sample.txt", O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd == -1)
        fail("open");
    printf("[open] sample.txt 생성 및 열기\n");

    size_t total = 0;
    size_t length = strlen(message);
    while (total < length) {
        ssize_t written = write(fd, message + total, length - total);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            fail("write");
        }
        if (written == 0) {
            fprintf(stderr, "write: 데이터를 기록하지 못했습니다.\n");
            return EXIT_FAILURE;
        }
        total += (size_t)written;
    }
    printf("[write] %zu바이트 기록\n", total);

    if (lseek(fd, 0, SEEK_SET) == -1)
        fail("lseek");
    printf("[lseek] 파일 위치를 처음으로 이동\n");

    printf("[read] 파일 내용: ");
    for (;;) {
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count == -1) {
            if (errno == EINTR)
                continue;
            fail("read");
        }
        if (count == 0)
            break;
        if (fwrite(buffer, 1, (size_t)count, stdout) != (size_t)count)
            fail("fwrite");
    }

    if (close(fd) == -1)
        fail("close");
    printf("[close] 파일 닫기\n");

    if (stat("sample.txt", &info) == -1)
        fail("stat");
    printf("[stat] 크기: %lld바이트, 권한: %03o\n",
           (long long)info.st_size, (unsigned int)(info.st_mode & 0777));

    if (chmod("sample.txt", 0600) == -1)
        fail("chmod");
    if (stat("sample.txt", &info) == -1)
        fail("stat");
    printf("[chmod] 변경된 권한: %03o\n",
           (unsigned int)(info.st_mode & 0777));

    if (mkdir("subdir", 0755) == -1)
        fail("mkdir");
    printf("[mkdir] subdir 생성\n");

    if (rename("sample.txt", "renamed.txt") == -1)
        fail("rename");
    printf("[rename] sample.txt -> renamed.txt\n");

    DIR *dir = opendir(".");
    if (dir == NULL)
        fail("opendir");
    printf("[opendir/readdir] 디렉토리 목록\n");

    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0)
                fail("readdir");
            break;
        }
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0)
            printf("  %s\n", entry->d_name);
    }

    if (closedir(dir) == -1)
        fail("closedir");
    printf("[closedir] 디렉토리 닫기\n");

    if (unlink("renamed.txt") == -1)
        fail("unlink");
    printf("[unlink] renamed.txt 삭제\n");

    if (rmdir("subdir") == -1)
        fail("rmdir");
    printf("[rmdir] subdir 삭제\n");

    if (chdir("..") == -1)
        fail("chdir");
    if (rmdir(directory) == -1)
        fail("rmdir");
    printf("[rmdir] 작업 디렉토리 삭제\n");
    printf("실습 완료\n");

    return EXIT_SUCCESS;
}
