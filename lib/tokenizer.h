/*
 * tokenizer.h
 *
 *  Created on: 9 Aug 2013
 *      Author: taylorr
 */

#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#include <stddef.h>

void* clog_malloc(size_t s);
void* clog_realloc(void* p, size_t s);
void clog_free(void* p);

int tokenize(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* param);

struct Token
{
	unsigned int type;
	union t
	{
		struct String
		{
			size_t len;
			unsigned char* str;
		} string;
		unsigned long integer;
		double real;
	} value;
};

void token_destroy(struct Token* token);


#endif /* TOKENIZER_H_ */
