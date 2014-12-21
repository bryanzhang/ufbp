/**
 * gcc -Wall -O3 -o mmap_example mmap_example.cc
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PERMS 0600

int main(int argc, char* argv[]) {
  int src, dst;
  void *sm, *dm;
  struct stat statbuf;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <source> <target>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if ((src = open(argv[1], O_RDONLY)) < 0) {
    perror("open source");
    exit(EXIT_FAILURE);
  }

  // 为了完成复制,必须包含读打开,否则mmap()失败
  if ((dst = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, PERMS)) < 0) {
    perror("open target");
    exit(EXIT_FAILURE);
  }
  if (fstat(src, &statbuf) < 0) {
    perror("fstat source");
    exit(EXIT_FAILURE);
  }

  // 参看前面man手册中的说明,mmap()不能用于扩展文件长度。所以这里必须实现扩大
  // 目标文件长度,准备一个空架子等待复制。
  if (lseek(dst, statbuf.st_size - 1, SEEK_SET) < 0) {
    perror("lseek target");
    exit(EXIT_FAILURE);
  }
  if (write(dst, &statbuf, 1) != 1) {
    perror("write target");
    exit(EXIT_FAILURE);
  }

  // 读的时候制定MAP_PRIVATE即可
  sm = mmap(0, (size_t)statbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, src, 0);
  if (MAP_FAILED == sm) {
    perror("mmap source");
    exit(EXIT_FAILURE);
  }

  // 这里必须制定MAP_SHARED才可能真正改变静态文件
  dm = mmap(0, (size_t)statbuf.st_size, PROT_WRITE, MAP_SHARED, dst, 0);
  if (MAP_FAILED == dm) {
    perror("mmap target");
    exit(EXIT_FAILURE);
  }
  memcpy(dm, sm, (size_t)statbuf.st_size);
  // 可以不要这行代码
  msync(dm, (size_t)statbuf.st_size, MS_SYNC);
  return EXIT_SUCCESS;
}
