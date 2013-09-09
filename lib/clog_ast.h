/*
 * ast.h
 *
 *  Created on: 14 Aug 2013
 *      Author: taylorr
 */

#ifndef CLOG_AST_H_
#define CLOG_AST_H_

#include <stddef.h>

void* clog_malloc(size_t s);
void* clog_realloc(void* p, size_t s);
void clog_free(void* p);

struct clog_parser;
struct clog_ast_statement_list;

int clog_ast_out_of_memory(struct clog_parser* parser);
int clog_syntax_error(struct clog_parser* parser, const char* msg, unsigned long line);

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
int clog_token_alloc(struct clog_parser* parser, struct clog_token** token, const unsigned char* sz, size_t len);
int clog_syntax_error_token(struct clog_parser* parser, const char* pre, const char* post, unsigned int token_id, struct clog_token* token, unsigned long line);

/* Literal handling */
struct clog_ast_literal
{
	enum clog_ast_literal_type
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
int clog_ast_literal_alloc(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_token* token);
int clog_ast_literal_alloc_bool(struct clog_parser* parser, struct clog_ast_literal** lit, int value);
struct clog_ast_literal* clog_ast_literal_append_string(struct clog_parser* parser, struct clog_ast_literal* lit, struct clog_token* token);
int clog_ast_literal_clone(struct clog_parser* parser, struct clog_ast_literal** new, const struct clog_ast_literal* lit);

int clog_ast_literal_compare(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2);
int clog_ast_literal_arith_convert(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2);
int clog_ast_literal_id_compare(const struct clog_ast_literal* lit1, const struct clog_ast_literal* lit2);

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
int clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit);
int clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token);
int clog_ast_expression_alloc_builtin1(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1);
int clog_ast_expression_alloc_builtin2(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2);
int clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3);
int clog_ast_expression_alloc_dot(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* p1, struct clog_token* token);
int clog_ast_expression_alloc_call(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* call, struct clog_ast_expression_list* list);

unsigned long clog_ast_expression_line(const struct clog_ast_expression* expr);

struct clog_ast_expression_list
{
	struct clog_ast_expression* expr;
	struct clog_ast_expression_list* next;
};

void clog_ast_expression_list_free(struct clog_parser* parser, struct clog_ast_expression_list* list);
int clog_ast_expression_list_alloc(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr);
int clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr);

struct clog_ast_statement
{
	enum clog_ast_statement_type
	{
		clog_ast_statement_expression,
		clog_ast_statement_block,
		clog_ast_statement_declaration,
		clog_ast_statement_constant,
		clog_ast_statement_if,
		clog_ast_statement_do,
		clog_ast_statement_break,
		clog_ast_statement_continue,
		clog_ast_statement_return
	} type;

	union clog_ast_statement_u
	{
		struct clog_ast_expression* expression;
		struct clog_ast_statement_list* block;
		struct clog_ast_literal* declaration;

		struct clog_ast_statement_if
		{
			struct clog_ast_expression* condition;
			struct clog_ast_statement_list* true_stmt;
			struct clog_ast_statement_list* false_stmt;
		}* if_stmt;

		struct clog_ast_statement_do
		{
			struct clog_ast_expression* condition;
			struct clog_ast_statement_list* loop_stmt;
		}* do_stmt;
	} stmt;
};

struct clog_ast_statement_list
{
	struct clog_ast_statement* stmt;
	struct clog_ast_statement_list* next;
};

void clog_ast_statement_list_free(struct clog_parser* parser, struct clog_ast_statement_list* list);
int clog_ast_statement_list_alloc_expression(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr);
int clog_ast_statement_list_alloc_block(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* block);
struct clog_ast_statement_list* clog_ast_statement_list_append(struct clog_parser* parser, struct clog_ast_statement_list* list, struct clog_ast_statement_list* next);
int clog_ast_statement_list_alloc_declaration(struct clog_parser* parser, struct clog_ast_statement_list** stmt, struct clog_token* id, struct clog_ast_expression* init);
int clog_ast_statement_list_alloc_if(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond, struct clog_ast_statement_list* true_expr, struct clog_ast_statement_list* false_expr);
int clog_ast_statement_list_alloc_do(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* cond, struct clog_ast_statement_list* loop);
int clog_ast_statement_list_alloc_while(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond, struct clog_ast_statement_list* loop_stmt);
int clog_ast_statement_list_alloc_for(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* init_stmt, struct clog_ast_statement_list* cond_stmt, struct clog_ast_expression* iter_expr, struct clog_ast_statement_list* loop_stmt);
int clog_ast_statement_list_alloc_return(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr);
int clog_ast_statement_list_alloc(struct clog_parser* parser, struct clog_ast_statement_list** list, enum clog_ast_statement_type type);

/* Optimization */
int clog_ast_statement_list_reduce_constants(struct clog_parser* parser, struct clog_ast_statement_list** block);

struct clog_parser
{
	int           reduce;
	int           failed;
	unsigned long line;

	struct clog_ast_statement_list* pgm;
};

#endif /* CLOG_AST_H_ */
