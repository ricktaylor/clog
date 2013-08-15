/*
 * ast.h
 *
 *  Created on: 14 Aug 2013
 *      Author: taylorr
 */

#ifndef AST_H_
#define AST_H_

#include <stddef.h>
#include <stdarg.h>

void* clog_malloc(size_t s);
void* clog_realloc(void* p, size_t s);
void clog_free(void* p);

struct clog_parser;

/* Simple string */
struct clog_string
{
	size_t         len;
	unsigned char* str;
};

/* Token handling */
struct clog_token
{
	enum etype
	{
		clog_token_string,
		clog_token_integer,
		clog_token_real,
	} type;

	union clog_token_u
	{
		struct clog_string string;
		long               integer;
		double             real;
	} value;
};

void clog_token_free(struct clog_parser* parser, struct clog_token* token);

/* Literal handling */
struct clog_ast_literal
{
	enum
	{
		clog_ast_literal_string,
		clog_ast_literal_integer,
		clog_ast_literal_real,
		clog_ast_literal_bool,
		clog_ast_literal_null
	} type;

	union clog_ast_literal_u
	{
		struct clog_string string;
		long               integer;
		double             real;
	} value;
};

void clog_ast_literal_free(struct clog_parser* parser, struct clog_ast_literal* lit);
void clog_ast_literal_alloc(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_token* token);
void clog_ast_literal_alloc_bool(struct clog_parser* parser, struct clog_ast_literal** lit, int value);
void clog_ast_literal_append_string(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_ast_literal* str, struct clog_token* token);

struct clog_ast_expression
{
	enum
	{
		clog_ast_expression_identifier,
		clog_ast_expression_literal,
		clog_ast_expression_builtin,
		clog_ast_expression_function,

	} type;

	int lvalue;

	union clog_ast_expression_u
	{
		struct clog_ast_literal* literal;

		struct clog_ast_expression_builtin
		{
			unsigned int type;
			struct clog_ast_expression* args[2];
		}* builtin;

		struct clog_ast_expression_function
		{
			struct clog_string name;
			size_t argc;
			struct clog_ast_expression* argv[1];
		}* function;

	} expr;
};

void clog_ast_expression_free(struct clog_parser* parser, struct clog_ast_expression* expr);
void clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit);
void clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token);
void clog_ast_expression_alloc_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2);
void clog_ast_expression_alloc_preincdec(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1);

struct clog_parser
{
	int           failed;
	unsigned long line;
	void*         lemon_parser;
};

void clog_parser(void* lemon_parser, int type, struct clog_token* tok, struct clog_parser* parser);

#endif /* AST_H_ */
