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
struct clog_ast_statement_list;

void clog_out_of_memory(struct clog_parser* parser);
void clog_syntax_error(struct clog_parser* parser, const char* msg);

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
void clog_token_alloc(struct clog_parser* parser, struct clog_token** token, const unsigned char* sz, size_t len);

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

	unsigned long line;

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
struct clog_ast_literal* clog_ast_literal_append_string(struct clog_parser* parser, struct clog_ast_literal* lit, struct clog_token* token);

struct clog_ast_expression_list;

struct clog_ast_expression
{
	enum
	{
		clog_ast_expression_identifier,
		clog_ast_expression_literal,
		clog_ast_expression_builtin,
		clog_ast_expression_call,

	} type;

	int lvalue;

	union clog_ast_expression_u
	{
		struct clog_ast_literal* literal;
		struct clog_ast_literal* identifier;

		struct clog_ast_expression_builtin
		{
			unsigned long line;
			unsigned int type;
			struct clog_ast_expression* args[3];
		}* builtin;

		struct clog_ast_expression_call
		{
			struct clog_ast_expression* expr;
			struct clog_ast_expression_list* params;
		}* call;

	} expr;
};

void clog_ast_expression_free(struct clog_parser* parser, struct clog_ast_expression* expr);
void clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit);
void clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token);
void clog_ast_expression_alloc_builtin1(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1);
void clog_ast_expression_alloc_builtin2(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2);
void clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3);
void clog_ast_expression_alloc_assign(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2);
void clog_ast_expression_alloc_builtin_lvalue(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2);
void clog_ast_expression_alloc_dot(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* p1, struct clog_token* token);
void clog_ast_expression_alloc_call(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* call, struct clog_ast_expression_list* list);

struct clog_ast_expression_list
{
	struct clog_ast_expression* expr;
	struct clog_ast_expression_list* next;
};

void clog_ast_expression_list_free(struct clog_parser* parser, struct clog_ast_expression_list* list);
void clog_ast_expression_list_alloc(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr);
struct clog_ast_expression_list* clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_expression* expr);

struct clog_ast_statement
{
	enum
	{
		clog_ast_statement_expression,
		clog_ast_statement_block,
		clog_ast_statement_declaration,

	} type;

	union clog_ast_statement_u
	{
		struct clog_ast_expression* expression;
		struct clog_ast_statement_list* block;
		struct clog_ast_literal* declaration;
	} stmt;
};

void clog_ast_statement_free(struct clog_parser* parser, struct clog_ast_statement* stmt);
void clog_ast_statement_alloc_expression(struct clog_parser* parser, struct clog_ast_statement** stmt, struct clog_ast_expression* expr);
void clog_ast_statement_alloc_block(struct clog_parser* parser, struct clog_ast_statement** stmt, struct clog_ast_statement_list* list);

struct clog_ast_statement_list
{
	struct clog_ast_statement* stmt;
	struct clog_ast_statement_list* next;
};

void clog_ast_statement_list_free(struct clog_parser* parser, struct clog_ast_statement_list* list);
void clog_ast_statement_list_alloc(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement* stmt);
void clog_ast_statement_list_alloc_expression(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr);
struct clog_ast_statement_list* clog_ast_statement_list_append(struct clog_parser* parser, struct clog_ast_statement_list* list, struct clog_ast_statement_list* next);

void clog_ast_statement_alloc_declaration(struct clog_parser* parser, struct clog_ast_statement_list** stmt, struct clog_token* id, struct clog_ast_expression* init);

struct clog_parser
{
	int           failed;
	unsigned long line;
};

#endif /* AST_H_ */
