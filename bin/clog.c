#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int clog_parse(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param);

void* clog_malloc(size_t s)
{
	return malloc(s);
}

void* clog_realloc(void* p, size_t s)
{
	return realloc(p,s);
}

void clog_free(void* p)
{
	free(p);
}

static int read_fn(void* p, unsigned char* buf, size_t* len)
{
	*len = fread(buf,1,*len,(FILE*)p);
	if (*len == 0 && ferror((FILE*)p))
		return -1;

	return 0;
}

int main(int argc, char* argv[])
{
	FILE* f = fopen(argv[1],"r");
	if (!f)
	{
		printf("Failed to open %s: %s\n",argv[1],strerror(errno));
		return -1;
	}

	clog_parse(&read_fn,f);

	fclose(f);

	return 0;
}
