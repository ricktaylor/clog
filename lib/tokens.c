/*
 * tokens.c
 *
 *  Created on: 9 Aug 2013
 *      Author: taylorr
 */

#include "tokenizer.h"

int clog_tokenize(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param, void* parser);

void* clog_parserAlloc(void* (*)(size_t));
void clog_parserFree(void *p, void (*)(void*));

int clog_parse(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param)
{
	int retval = 0;
	struct clog_parser_t parser = {0,1,NULL};

	parser.lemon_parser = clog_parserAlloc(&clog_malloc);
	if (!parser.lemon_parser)
		return -1;

	retval = clog_tokenize(rd_fn,rd_param,&parser);
	if (retval == 0 && !parser.failed)
		clog_parser(parser.lemon_parser,0,NULL,&parser);

	clog_parserFree(parser.lemon_parser,&clog_free);

	return retval;
}
