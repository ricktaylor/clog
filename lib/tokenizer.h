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

struct clog_parser_t
{
	int failed;
	unsigned long line;
	void* lemon_parser;
};

struct clog_token_t
{
	enum etype
	{
		clog_token_string,
		clog_token_integer,
		clog_token_real,
	} type;
	union clog_utoken
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

void clog_parser(void* lemon_parser, int type, struct clog_token_t* tok, struct clog_parser_t* parser);

#endif /* TOKENIZER_H_ */
