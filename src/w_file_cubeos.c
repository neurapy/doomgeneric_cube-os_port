#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
	wad_file_t wad;
	int	   fd;
} cubeos_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_CubeOS_OpenFile(char *path)
{
	int fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		return NULL;
	}

	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < 0) {
		close(fd);
		return NULL;
	}

	cubeos_wad_file_t *result = Z_Malloc(sizeof(*result), PU_STATIC, 0);
	result->wad.file_class    = &stdc_wad_file;
	result->wad.mapped	      = NULL;
	result->wad.length	      = (unsigned int)st.st_size;
	result->fd		      = fd;

	return &result->wad;
}

static void W_CubeOS_CloseFile(wad_file_t *wad)
{
	cubeos_wad_file_t *cubeos_wad = (cubeos_wad_file_t *)wad;

	close(cubeos_wad->fd);
	Z_Free(cubeos_wad);
}

static size_t W_CubeOS_Read(wad_file_t *wad, unsigned int offset, void *buffer, size_t buffer_len)
{
	cubeos_wad_file_t *cubeos_wad = (cubeos_wad_file_t *)wad;
	uint8_t		  *out	       = (uint8_t *)buffer;
	size_t		   total_read   = 0;

	if (lseek(cubeos_wad->fd, (off_t)offset, SEEK_SET) < 0) {
		return 0;
	}

	while (total_read < buffer_len) {
		ssize_t rc = read(cubeos_wad->fd, out + total_read, buffer_len - total_read);
		if (rc <= 0) {
			break;
		}

		total_read += (size_t)rc;
	}

	return total_read;
}

wad_file_class_t stdc_wad_file = {
	W_CubeOS_OpenFile,
	W_CubeOS_CloseFile,
	W_CubeOS_Read,
};
