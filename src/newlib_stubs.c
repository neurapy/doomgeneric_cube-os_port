#include <errno.h>
#include <fcntl.h>
#include <reent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static constexpr uint32_t CUBEOS_O_RDWR	      = 1u << 0;
static constexpr uint32_t CUBEOS_O_CREAT      = 1u << 1;
static constexpr uint32_t CUBEOS_O_DEL	      = 1u << 2;
static constexpr int	  CUBEOS_MAX_OPEN_FDS = 16;

int32_t		  syscall_open(const char *path, uint32_t flags);
int32_t		  syscall_read(int fd, void *buf, uint32_t size);
int32_t		  syscall_write(uint32_t fd, const void *buf, uint32_t size);
int32_t		  syscall_close(int fd);
int32_t		  syscall_mkdir(const char *path);
int32_t		  syscall_fstat(int fd);
uint32_t	  syscall_fseek(int fd, int32_t delta);
void		 *syscall_sbrk(int32_t increment);
[[noreturn]] void syscall_exit(void);

static off_t   g_fd_offsets[CUBEOS_MAX_OPEN_FDS];
static uint8_t g_fd_in_use[CUBEOS_MAX_OPEN_FDS];

static void cubeos_set_errno(struct _reent *reent, int value)
{
	errno = value;
	if (reent)
		reent->_errno = value;
}

static void cubeos_clear_errno(struct _reent *reent)
{
	errno = 0;
	if (reent)
		reent->_errno = 0;
}

static void cubeos_track_fd_open(int fd)
{
	if (fd < 0 || fd >= CUBEOS_MAX_OPEN_FDS)
		return;

	g_fd_in_use[fd]	 = 1;
	g_fd_offsets[fd] = 0;
}

static void cubeos_track_fd_close(int fd)
{
	if (fd < 0 || fd >= CUBEOS_MAX_OPEN_FDS)
		return;

	g_fd_in_use[fd]	 = 0;
	g_fd_offsets[fd] = 0;
}

static bool cubeos_fd_is_file(int fd)
{
	return fd >= 0 && fd < CUBEOS_MAX_OPEN_FDS && g_fd_in_use[fd] != 0;
}

static int cubeos_open_file(const char *path, int flags)
{
	if (!path)
		return -1;

	if ((flags & O_TRUNC) != 0)
		(void)syscall_open(path, CUBEOS_O_DEL);

	uint32_t cubeos_flags = CUBEOS_O_RDWR;
	if ((flags & O_CREAT) != 0 || (flags & O_TRUNC) != 0)
		cubeos_flags |= CUBEOS_O_CREAT;

	int fd = syscall_open(path, cubeos_flags);
	if (fd >= 0)
		cubeos_track_fd_open(fd);
	return fd;
}

static int cubeos_copy_file(const char *src_path, const char *dst_path)
{
	uint8_t buffer[512];
	int	src_fd = syscall_open(src_path, CUBEOS_O_RDWR);
	if (src_fd < 0)
		return -1;
	cubeos_track_fd_open(src_fd);

	(void)syscall_open(dst_path, CUBEOS_O_DEL);
	int dst_fd = syscall_open(dst_path, CUBEOS_O_CREAT | CUBEOS_O_RDWR);
	if (dst_fd < 0) {
		cubeos_track_fd_close(src_fd);
		(void)syscall_close(src_fd);
		return -1;
	}
	cubeos_track_fd_open(dst_fd);

	int status = 0;
	for (;;) {
		int32_t read_count = syscall_read(src_fd, buffer, sizeof(buffer));
		if (read_count < 0) {
			status = -1;
			break;
		}
		if (read_count == 0)
			break;

		uint32_t written = 0;
		while (written < (uint32_t)read_count) {
			int32_t rc = syscall_write((uint32_t)dst_fd, buffer + written,
						   (uint32_t)read_count - written);
			if (rc <= 0) {
				status = -1;
				break;
			}
			written += (uint32_t)rc;
			g_fd_offsets[dst_fd] += rc;
		}
		if (status != 0)
			break;

		g_fd_offsets[src_fd] += read_count;
	}

	cubeos_track_fd_close(src_fd);
	cubeos_track_fd_close(dst_fd);
	(void)syscall_close(src_fd);
	(void)syscall_close(dst_fd);

	if (status != 0)
		(void)syscall_open(dst_path, CUBEOS_O_DEL);

	return status;
}

static int cubeos_remove_path(const char *path)
{
	int32_t rc = syscall_open(path, CUBEOS_O_DEL);
	return rc == 0 ? 0 : -1;
}

int _open_r(struct _reent *reent, const char *path, int flags, int mode)
{
	(void)mode;
	int fd = cubeos_open_file(path, flags);
	if (fd < 0) {
		cubeos_set_errno(reent, ENOENT);
		return -1;
	}

	cubeos_clear_errno(reent);
	return fd;
}

int _open(const char *path, int flags, int mode)
{
	return _open_r(NULL, path, flags, mode);
}

int _close_r(struct _reent *reent, int fd)
{
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
		cubeos_clear_errno(reent);
		return 0;
	}

	if (syscall_close(fd) != 0) {
		cubeos_set_errno(reent, EBADF);
		return -1;
	}

	cubeos_track_fd_close(fd);
	cubeos_clear_errno(reent);
	return 0;
}

int _close(int fd)
{
	return _close_r(NULL, fd);
}

_ssize_t _read_r(struct _reent *reent, int fd, void *buf, size_t count)
{
	int32_t rc = syscall_read(fd, buf, (uint32_t)count);
	if (rc < 0) {
		cubeos_set_errno(reent, EBADF);
		return -1;
	}

	if (cubeos_fd_is_file(fd))
		g_fd_offsets[fd] += rc;

	cubeos_clear_errno(reent);
	return rc;
}

_ssize_t _read(int fd, void *buf, size_t count)
{
	return _read_r(NULL, fd, buf, count);
}

_ssize_t _write_r(struct _reent *reent, int fd, const void *buf, size_t count)
{
	int32_t rc = syscall_write((uint32_t)fd, buf, (uint32_t)count);
	if (rc < 0) {
		cubeos_set_errno(reent, EBADF);
		return -1;
	}

	if (cubeos_fd_is_file(fd))
		g_fd_offsets[fd] += rc;

	cubeos_clear_errno(reent);
	return rc;
}

_ssize_t _write(int fd, const void *buf, size_t count)
{
	return _write_r(NULL, fd, buf, count);
}

_off_t _lseek_r(struct _reent *reent, int fd, _off_t offset, int whence)
{
	if (!cubeos_fd_is_file(fd)) {
		cubeos_set_errno(reent, EBADF);
		return -1;
	}

	_off_t target = offset;
	switch (whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		target = g_fd_offsets[fd] + offset;
		break;
	case SEEK_END: {
		int32_t size = syscall_fstat(fd);
		if (size < 0) {
			cubeos_set_errno(reent, EBADF);
			return -1;
		}
		target = size + offset;
		break;
	}
	default:
		cubeos_set_errno(reent, EINVAL);
		return -1;
	}

	if (target < 0)
		target = 0;

	int32_t	 delta	 = (int32_t)(target - g_fd_offsets[fd]);
	uint32_t new_pos = syscall_fseek(fd, delta);
	if (new_pos == (uint32_t)-1) {
		cubeos_set_errno(reent, EIO);
		return -1;
	}

	g_fd_offsets[fd] = (_off_t)new_pos;
	cubeos_clear_errno(reent);
	return g_fd_offsets[fd];
}

_off_t _lseek(int fd, _off_t offset, int whence)
{
	return _lseek_r(NULL, fd, offset, whence);
}

int _fstat_r(struct _reent *reent, int fd, struct stat *st)
{
	if (!st) {
		cubeos_set_errno(reent, EINVAL);
		return -1;
	}

	memset(st, 0, sizeof(*st));
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
		st->st_mode    = S_IFCHR;
		st->st_blksize = 1;
		cubeos_clear_errno(reent);
		return 0;
	}

	int32_t size = syscall_fstat(fd);
	if (size < 0) {
		cubeos_set_errno(reent, EBADF);
		return -1;
	}

	st->st_mode    = S_IFREG;
	st->st_size    = size;
	st->st_blksize = 512;
	cubeos_clear_errno(reent);
	return 0;
}

int _fstat(int fd, struct stat *st)
{
	return _fstat_r(NULL, fd, st);
}

int _isatty_r(struct _reent *reent, int fd)
{
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
		cubeos_clear_errno(reent);
		return 1;
	}

	cubeos_set_errno(reent, ENOTTY);
	return 0;
}

int _isatty(int fd)
{
	return _isatty_r(NULL, fd);
}

void *_sbrk_r(struct _reent *reent, ptrdiff_t increment)
{
	void *result = syscall_sbrk((int32_t)increment);
	if (result == (void *)-1) {
		cubeos_set_errno(reent, ENOMEM);
		return (void *)-1;
	}

	cubeos_clear_errno(reent);
	return result;
}

void *_sbrk(ptrdiff_t increment)
{
	return _sbrk_r(NULL, increment);
}

int _mkdir_r(struct _reent *reent, const char *path, int mode)
{
	(void)mode;
	if (syscall_mkdir(path) != 0) {
		cubeos_set_errno(reent, EIO);
		return -1;
	}

	cubeos_clear_errno(reent);
	return 0;
}

int mkdir(const char *path, mode_t mode)
{
	return _mkdir_r(NULL, path, (int)mode);
}

int remove(const char *path)
{
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	if (cubeos_remove_path(path) != 0) {
		errno = ENOENT;
		return -1;
	}

	return 0;
}

int rename(const char *oldpath, const char *newpath)
{
	if (!oldpath || !newpath) {
		errno = EINVAL;
		return -1;
	}
	if (strcmp(oldpath, newpath) == 0)
		return 0;

	if (cubeos_copy_file(oldpath, newpath) != 0) {
		errno = EIO;
		return -1;
	}
	if (cubeos_remove_path(oldpath) != 0) {
		errno = EIO;
		return -1;
	}

	return 0;
}

int system(const char *command)
{
	(void)command;
	errno = ENOSYS;
	return -1;
}

int _kill_r(struct _reent *reent, int pid, int sig)
{
	(void)pid;
	(void)sig;
	cubeos_set_errno(reent, EINVAL);
	return -1;
}

int _kill(int pid, int sig)
{
	return _kill_r(NULL, pid, sig);
}

int _getpid_r(struct _reent *reent)
{
	cubeos_clear_errno(reent);
	return 1;
}

int _getpid(void)
{
	return _getpid_r(NULL);
}

[[noreturn]] void _exit(int status)
{
	(void)status;
	syscall_exit();
}

[[noreturn]] void _exit_r(struct _reent *reent, int status)
{
	(void)reent;
	_exit(status);
}
