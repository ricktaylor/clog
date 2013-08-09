#include <stdlib.h>

void* clog_malloc(size_t s)
{
	return malloc(s);
}

void clog_free(void* p)
{
	free(p);
}

int main(int argc, char* argv[])
{
	return 0;
}
