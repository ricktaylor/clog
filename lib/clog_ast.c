/*
 * tokens.c
 *
 *  Created on: 9 Aug 2013
 *      Author: taylorr
 */

#include "clog_ast.h"
#include "clog_parser.h"

#include <string.h>
#include <stdio.h>

static void __dump(size_t indent, const struct clog_ast_statement_list* list);

int clog_out_of_memory(struct clog_parser* parser)
{
	printf("Out of memory building AST\n");
	parser->failed = 1;
	return 0;
}

int clog_syntax_error(struct clog_parser* parser, const char* msg, unsigned long line)
{
	printf("Syntax error at line %lu: %s\n",line,msg);
	parser->failed = 1;
	return 0;
}

void clog_token_free(struct clog_parser* parser, struct clog_token* token)
{
	if (token)
	{
		if (token->type == clog_token_string && token->value.string.str)
			clog_free(token->value.string.str);

		clog_free(token);
	}
}

int clog_token_alloc(struct clog_parser* parser, struct clog_token** token, const unsigned char* sz, size_t len)
{
	*token = clog_malloc(sizeof(struct clog_token));
	if (!*token)
		return clog_out_of_memory(parser);

	(*token)->type = clog_token_string;
	(*token)->value.string.len = len;
	(*token)->value.string.str = NULL;

	if (len)
	{
		(*token)->value.string.str = clog_malloc(len+1);
		if (!(*token)->value.string.str)
		{
			clog_free(*token);
			*token = NULL;
			return clog_out_of_memory(parser);
		}

		memcpy((*token)->value.string.str,sz,len);
		(*token)->value.string.str[len] = 0;
	}

	return 1;
}

static int clog_token_printf(unsigned int token_id, struct clog_token* token)
{
	switch (token_id)
	{
	case CLOG_TOKEN_THIS:
	case CLOG_TOKEN_BASE:
	case CLOG_TOKEN_ID:
		if (token && token->type == clog_token_string)
			return printf("%s",token->value.string.len ? (char*)token->value.string.str : "\"\"");
		return printf("identifier");

	case CLOG_TOKEN_STRING:
		if (token && token->type == clog_token_string)
			return printf("%s",token->value.string.len ? (char*)token->value.string.str : "\"\"");
		return printf("string");

	case CLOG_TOKEN_INTEGER:
		if (token && token->type == clog_token_integer)
			return printf("%ld",token->value.integer);
		return printf("integer");

	case CLOG_TOKEN_FLOAT:
		if (token && token->type == clog_token_real)
			return printf("%g",token->value.real);
		return printf("float");

	case CLOG_TOKEN_TRUE:
		return printf("true");
	case CLOG_TOKEN_FALSE:
		return printf("false");
	case CLOG_TOKEN_NULL:
		return printf("null");
	case CLOG_TOKEN_IF:
		return printf("if");
	case CLOG_TOKEN_OPEN_PAREN:
		return printf("(");
	case CLOG_TOKEN_CLOSE_PAREN:
		return printf(")");
	case CLOG_TOKEN_ELSE:
		return printf("else");
	case CLOG_TOKEN_WHILE:
		return printf("while");
	case CLOG_TOKEN_FOR:
		return printf("for");
	case CLOG_TOKEN_SEMI_COLON:
		return printf(";");
	case CLOG_TOKEN_DO:
		return printf("do");
	case CLOG_TOKEN_VAR:
		return printf("var");
	case CLOG_TOKEN_ASSIGN:
		return printf("=");
	case CLOG_TOKEN_OPEN_BRACE:
		return printf("{");
	case CLOG_TOKEN_CLOSE_BRACE:
		return printf("}");
	case CLOG_TOKEN_TRY:
		return printf("try");
	case CLOG_TOKEN_CATCH:
		return printf("catch");
	case CLOG_TOKEN_ELIPSIS:
		return printf("...");
	case CLOG_TOKEN_BREAK:
		return printf("break");
	case CLOG_TOKEN_CONTINUE:
		return printf("continue");
	case CLOG_TOKEN_RETURN:
		return printf("return");
	case CLOG_TOKEN_COMMA:
		return printf(",");
	case CLOG_TOKEN_STAR_ASSIGN:
		return printf("*=");
	case CLOG_TOKEN_SLASH_ASSIGN:
		return printf("/=");
	case CLOG_TOKEN_PERCENT_ASSIGN:
		return printf("%%=");
	case CLOG_TOKEN_PLUS_ASSIGN:
		return printf("+=");
	case CLOG_TOKEN_MINUS_ASSIGN:
		return printf("-=");
	case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
		return printf(">>=");
	case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
		return printf("<<=");
	case CLOG_TOKEN_AMPERSAND_ASSIGN:
		return printf("&=");
	case CLOG_TOKEN_BITWISE_CARET_ASSIGN:
		return printf("^=");
	case CLOG_TOKEN_BAR_ASSIGN:
		return printf("|=");
	case CLOG_TOKEN_THROW:
		return printf("throw");
	case CLOG_TOKEN_QUESTION:
		return printf("?");
	case CLOG_TOKEN_COLON:
		return printf(":");
	case CLOG_TOKEN_OR:
		return printf("||");
	case CLOG_TOKEN_AND:
		return printf("&&");
	case CLOG_TOKEN_BAR:
		return printf("|");
	case CLOG_TOKEN_CARET:
		return printf("^");
	case CLOG_TOKEN_AMPERSAND:
		return printf("&");
	case CLOG_TOKEN_EQUALS:
		return printf("==");
	case CLOG_TOKEN_NOT_EQUALS:
		return printf("!=");
	case CLOG_TOKEN_LESS_THAN:
		return printf("<");
	case CLOG_TOKEN_GREATER_THAN:
		return printf(">");
	case CLOG_TOKEN_LESS_THAN_EQUALS:
		return printf("<=");
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
		return printf(">=");
	case CLOG_TOKEN_LEFT_SHIFT:
		return printf("<<");
	case CLOG_TOKEN_RIGHT_SHIFT:
		return printf(">>");
	case CLOG_TOKEN_PLUS:
		return printf("+");
	case CLOG_TOKEN_MINUS:
		return printf("-");
	case CLOG_TOKEN_STAR:
		return printf("*");
	case CLOG_TOKEN_SLASH:
		return printf("/");
	case CLOG_TOKEN_PERCENT:
		return printf("%%");
	case CLOG_TOKEN_DOUBLE_PLUS:
		return printf("++");
	case CLOG_TOKEN_DOUBLE_MINUS:
		return printf("--");
	case CLOG_TOKEN_EXCLAMATION:
		return printf("!");
	case CLOG_TOKEN_TILDA:
		return printf("~");
	case CLOG_TOKEN_OPEN_BRACKET:
		return printf("[");
	case CLOG_TOKEN_CLOSE_BRACKET:
		return printf("]");
	case CLOG_TOKEN_DOT:
		return printf(".");

	default:
		return printf("???");
	}
}

int clog_syntax_error_token(struct clog_parser* parser, const char* pre, const char* post, unsigned int token_id, struct clog_token* token, unsigned long line)
{
	printf("Syntax error at line %lu: ",line);
	if (pre)
		printf("%s",pre);
	clog_token_printf(token_id,token);
	if (post)
		printf("%s",post);
	printf("\n");
	parser->failed = 1;
	return 0;
}

void clog_ast_literal_free(struct clog_parser* parser, struct clog_ast_literal* lit)
{
	if (lit)
	{
		if (lit->type == clog_ast_literal_string && lit->value.string.str)
			clog_free(lit->value.string.str);

		clog_free(lit);
	}
}

static int clog_ast_literal_clone(struct clog_parser* parser, struct clog_ast_literal** new, const struct clog_ast_literal* lit)
{
	*new = NULL;

	if (!lit)
		return 0;

	*new = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*new)
		return clog_out_of_memory(parser);

	(*new)->type = lit->type;
	(*new)->line = lit->line;

	switch (lit->type)
	{
	case clog_ast_literal_integer:
	case clog_ast_literal_bool:
	case clog_ast_literal_null:
		(*new)->value.integer = lit->value.integer;
		break;

	case clog_ast_literal_real:
		(*new)->value.real = lit->value.real;
		break;

	case clog_ast_literal_string:
		(*new)->value.string.len = lit->value.string.len;
		if ((*new)->value.string.len)
		{
			(*new)->value.string.str = clog_malloc((*new)->value.string.len+1);
			if (!(*new)->value.string.str)
			{
				clog_free(*new);
				*new = NULL;
				return clog_out_of_memory(parser);
			}

			memcpy((*new)->value.string.str,lit->value.string.str,(*new)->value.string.len);
			(*new)->value.string.str[(*new)->value.string.len] = 0;
		}
		break;
	}

	return 1;
}

int clog_ast_literal_alloc(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_token* token)
{
	*lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*lit)
	{
		clog_token_free(parser,token);
		return clog_out_of_memory(parser);
	}

	if (!token)
	{
		(*lit)->type = clog_ast_literal_null;
		(*lit)->value.integer = 0;
	}
	else if (token->type == clog_token_integer)
	{
		(*lit)->type = clog_ast_literal_integer;
		(*lit)->value.integer = token->value.integer;
	}
	else if (token->type == clog_token_real)
	{
		(*lit)->type = clog_ast_literal_real;
		(*lit)->value.real = token->value.real;
	}
	else if (token->type == clog_token_string)
	{
		(*lit)->type = clog_ast_literal_string;
		(*lit)->value.string = token->value.string;
		token->value.string.str = NULL;
	}

	(*lit)->line = parser->line;

	clog_token_free(parser,token);

	return 1;
}

int clog_ast_literal_alloc_bool(struct clog_parser* parser, struct clog_ast_literal** lit, int value)
{
	*lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*lit)
		return clog_out_of_memory(parser);

	(*lit)->type = clog_ast_literal_bool;
	(*lit)->line = parser->line;
	(*lit)->value.integer = value;

	return 1;
}

static int clog_ast_literal_bool_cast(const struct clog_ast_literal* lit)
{
	if (lit)
	{
		switch (lit->type)
		{
		case clog_ast_literal_integer:
		case clog_ast_literal_bool:
		case clog_ast_literal_null:
			return (lit->value.integer == 0 ? 0 : 1);

		case clog_ast_literal_real:
			return (lit->value.real == 0.0 ? 0 : 1);

		case clog_ast_literal_string:
			return (lit->value.string.len == 0 ? 0 : 1);
		}
	}
	return 0;
}

static void clog_ast_literal_bool_promote(struct clog_ast_literal* lit)
{
	if (lit)
	{
		switch (lit->type)
		{
		case clog_ast_literal_integer:
			lit->value.integer = (lit->value.integer == 0 ? 0 : 1);
			break;

		case clog_ast_literal_real:
			lit->value.integer = (lit->value.real == 0.0 ? 0 : 1);
			break;

		case clog_ast_literal_string:
			if (lit->value.string.str)
				clog_free(lit->value.string.str);
			lit->value.integer = (lit->value.string.len == 0 ? 0 : 1);
			break;

		case clog_ast_literal_bool:
		case clog_ast_literal_null:
			break;
		}
		lit->type = clog_ast_literal_bool;
	}
}

static int clog_ast_literal_int_promote(struct clog_ast_literal* lit)
{
	if (!lit)
		return 0;

	switch (lit->type)
	{
	case clog_ast_literal_null:
	case clog_ast_literal_bool:
	case clog_ast_literal_integer:
		lit->type = clog_ast_literal_integer;
		return 1;

	default:
		return 0;
	}
}

static int clog_ast_literal_real_promote(struct clog_ast_literal* lit)
{
	if (!lit)
		return 0;

	if (lit->type == clog_ast_literal_real)
		return 1;

	if (clog_ast_literal_int_promote(lit))
	{
		double v = lit->value.integer;
		lit->value.real = v;
		lit->type = clog_ast_literal_real;
		return 1;
	}
	return 0;
}

static int clog_ast_literal_arith_convert(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (!lit1 || !lit2)
		return 0;
	else if (lit1->type == clog_ast_literal_real && clog_ast_literal_real_promote(lit2))
		return 1;
	else if (lit2->type == clog_ast_literal_real &&	clog_ast_literal_real_promote(lit1))
		return 1;
	else if (clog_ast_literal_int_promote(lit1) && clog_ast_literal_int_promote(lit2))
		return 1;

	return 0;
}

static int clog_ast_literal_compare(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (lit1 && lit2)
	{
		if (lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
		{
			int i = memcmp(lit1->value.string.str,lit2->value.string.str,lit1->value.string.len < lit2->value.string.len ? lit1->value.string.len : lit2->value.string.len);
			if (i != 0)
				return (i > 0 ? 1 : -1);

			return (lit1->value.string.len > lit2->value.string.len ? 1 : (lit1->value.string.len == lit2->value.string.len ? 0 : -1));
		}

		if (clog_ast_literal_arith_convert(lit1,lit2))
		{
			if (lit1->type == clog_ast_literal_real)
				return (lit1->value.real > lit2->value.real ? 1 : (lit1->value.real == lit2->value.real ? 0 : -1));

			return (lit1->value.integer > lit2->value.integer ? 1 : (lit1->value.integer == lit2->value.integer ? 0 : -1));
		}
	}
	return -2;
}

static int clog_ast_literal_id_compare(const struct clog_ast_literal* lit1, const struct clog_ast_literal* lit2)
{
	if (lit1 && lit2 && lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
	{
		int i = memcmp(lit1->value.string.str,lit2->value.string.str,lit1->value.string.len < lit2->value.string.len ? lit1->value.string.len : lit2->value.string.len);
		if (i != 0)
			return (i > 0 ? 1 : -1);

		return (lit1->value.string.len > lit2->value.string.len ? 1 : (lit1->value.string.len == lit2->value.string.len ? 0 : -1));
	}
	return -2;
}

struct clog_ast_literal* clog_ast_literal_append_string(struct clog_parser* parser, struct clog_ast_literal* lit, struct clog_token* token)
{
	if (lit && token && token->value.string.len)
	{
		unsigned char* sz = clog_realloc(lit->value.string.str,lit->value.string.len + token->value.string.len);
		if (!sz)
		{
			clog_ast_literal_free(parser,lit);
			clog_out_of_memory(parser);
		}
		else
		{
			memcpy(sz+lit->value.string.len,token->value.string.str,token->value.string.len);
			lit->value.string.str = sz;
			lit->value.string.len += token->value.string.len;
			lit->value.string.str[lit->value.string.len] = 0;
		}
	}

	clog_token_free(parser,token);
	return lit;
}

void clog_ast_expression_free(struct clog_parser* parser, struct clog_ast_expression* expr)
{
	if (expr)
	{
		switch (expr->type)
		{
		case clog_ast_expression_literal:
			clog_ast_literal_free(parser,expr->expr.literal);
			break;

		case clog_ast_expression_identifier:
			clog_ast_literal_free(parser,expr->expr.identifier);
			break;

		case clog_ast_expression_builtin:
			if (expr->expr.builtin)
			{
				clog_ast_expression_free(parser,expr->expr.builtin->args[0]);
				clog_ast_expression_free(parser,expr->expr.builtin->args[1]);
				clog_ast_expression_free(parser,expr->expr.builtin->args[2]);
				clog_free(expr->expr.builtin);
			}
			break;

		case clog_ast_expression_call:
			if (expr->expr.call)
			{
				clog_ast_expression_free(parser,expr->expr.call->expr);
				clog_ast_expression_list_free(parser,expr->expr.call->params);
				clog_free(expr->expr.call);
			}
			break;
		}

		clog_free(expr);
	}
}

static int clog_ast_expression_list_clone(struct clog_parser* parser, struct clog_ast_expression_list** new, const struct clog_ast_expression_list* list);

static int clog_ast_expression_clone(struct clog_parser* parser, struct clog_ast_expression** new, const struct clog_ast_expression* expr)
{
	int ok = 0;

	*new = NULL;

	if (!expr)
		return 1;

	*new = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*new)
		return clog_out_of_memory(parser);

	(*new)->type = expr->type;
	(*new)->lvalue = expr->lvalue;
	(*new)->constant = expr->constant;

	switch ((*new)->type)
	{
	case clog_ast_expression_literal:
		ok = clog_ast_literal_clone(parser,&(*new)->expr.literal,expr->expr.literal);
		break;

	case clog_ast_expression_identifier:
		ok = clog_ast_literal_clone(parser,&(*new)->expr.identifier,expr->expr.identifier);
		break;

	case clog_ast_expression_builtin:
		(*new)->expr.builtin = clog_malloc(sizeof(struct clog_ast_expression_builtin));
		if (!(*new)->expr.builtin)
		{
			ok = clog_out_of_memory(parser);
			break;
		}

		(*new)->expr.builtin->type = expr->expr.builtin->type;
		(*new)->expr.builtin->line = expr->expr.builtin->line;

		ok = clog_ast_expression_clone(parser,&(*new)->expr.builtin->args[0],expr->expr.builtin->args[0]);
		if (ok)
			ok = clog_ast_expression_clone(parser,&(*new)->expr.builtin->args[1],expr->expr.builtin->args[1]);
		if (ok)
			ok = clog_ast_expression_clone(parser,&(*new)->expr.builtin->args[2],expr->expr.builtin->args[2]);

		if (!ok)
			clog_free((*new)->expr.builtin);
		break;

	case clog_ast_expression_call:
		(*new)->expr.call = clog_malloc(sizeof(struct clog_ast_expression_call));
		if (!(*new)->expr.call)
		{
			ok = clog_out_of_memory(parser);
			break;
		}

		ok = clog_ast_expression_clone(parser,&(*new)->expr.call->expr,expr->expr.call->expr);
		if (ok)
			ok = clog_ast_expression_list_clone(parser,&(*new)->expr.call->params,expr->expr.call->params);

		if (!ok)
			clog_free((*new)->expr.call);
		break;
	}

	if (!ok)
	{
		clog_free(*new);
		*new = NULL;
	}
	return ok;
}

unsigned long clog_ast_expression_line(const struct clog_ast_expression* expr)
{
	if (!expr)
		return 0;

	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		return expr->expr.identifier->line;

	case clog_ast_expression_literal:
		return expr->expr.literal->line;

	case clog_ast_expression_builtin:
		return expr->expr.builtin->line;

	case clog_ast_expression_call:
		return clog_ast_expression_line(expr->expr.call->expr);
	}

	return 0;
}

int clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit)
{
	*expr = NULL;

	if (!lit)
		return 0;

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_literal_free(parser,lit);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_literal;
	(*expr)->lvalue = 0;
	(*expr)->constant = 1;
	(*expr)->expr.literal = lit;

	return 1;
}

int clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token)
{
	*expr = NULL;

	if (!token)
		return 0;

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_token_free(parser,token);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_identifier;
	(*expr)->lvalue = 1;
	(*expr)->constant = 1;

	if (!clog_ast_literal_alloc(parser,&(*expr)->expr.identifier,token))
	{
		clog_free(*expr);
		*expr = NULL;
		return 0;
	}

	return 1;
}

int clog_ast_expression_alloc_dot(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* p1, struct clog_token* token)
{
	struct clog_ast_expression* p2;

	*expr = NULL;

	if (!token || !p1)
	{
		clog_ast_expression_free(parser,p1);
		clog_token_free(parser,token);
		return 0;
	}

	if (!clog_ast_expression_alloc_id(parser,&p2,token))
	{
		clog_ast_expression_free(parser,p1);
		return 0;
	}

	if (!clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_DOT,p1,p2))
		return 0;

	(*expr)->lvalue = 1;

	return 1;
}

int clog_ast_expression_alloc_builtin_lvalue(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	switch (type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
		if (!clog_ast_expression_alloc_builtin1(parser,expr,type,p1))
			return 0;
		break;

	default:
		if (!clog_ast_expression_alloc_builtin2(parser,expr,type,p1,p2))
			return 0;
		break;
	}

	(*expr)->lvalue = 1;

	return 1;
}

int clog_ast_expression_alloc_builtin1(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1)
{
	*expr = NULL;

	if (!p1)
		return 0;

	return clog_ast_expression_alloc_builtin3(parser,expr,type,p1,NULL,NULL);
}

int clog_ast_expression_alloc_builtin2(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	*expr = NULL;

	if (!p1 || !p2)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return 0;
	}

	if (!clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL))
		return 0;

	return 1;
}

int clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3)
{
	*expr = NULL;

	if (type == CLOG_TOKEN_QUESTION)
	{
		if (!p1 || !p2 || !p3)
		{
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			clog_ast_expression_free(parser,p3);
			return 0;
		}
	}

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,p3);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_builtin;
	(*expr)->lvalue = 0;
	(*expr)->expr.builtin = clog_malloc(sizeof(struct clog_ast_expression_builtin));
	if (!(*expr)->expr.builtin)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,p3);
		clog_free(*expr);
		*expr = NULL;
		return clog_out_of_memory(parser);
	}

	(*expr)->expr.builtin->type = type;
	(*expr)->expr.builtin->line = parser->line;
	(*expr)->expr.builtin->args[0] = p1;
	(*expr)->expr.builtin->args[1] = p2;
	(*expr)->expr.builtin->args[2] = p3;

	if (type == CLOG_TOKEN_COMMA)
	{
		if (p2 && p2->type == clog_ast_expression_identifier)
			p2->constant = 0;

		(*expr)->lvalue = p2->lvalue;
	}
	else
	{
		if (p1 && p1->type == clog_ast_expression_identifier)
			p1->constant = 0;

		if (p2 && p2->type == clog_ast_expression_identifier)
			p2->constant = 0;

		if (p3 && p3->type == clog_ast_expression_identifier)
			p3->constant = 0;
	}

	(*expr)->constant = (p1->constant &&
			(p2 ? p2->constant : 1) &&
			(p3 ? p3->constant : 1) ? 1 : 0);

	return 1;
}

int clog_ast_expression_alloc_assign(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	if (!clog_ast_expression_alloc_builtin2(parser,expr,type,p1,p2))
		return 0;

	(*expr)->lvalue = 1;

	return 1;
}

int clog_ast_expression_alloc_call(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* call, struct clog_ast_expression_list* list)
{
	*expr = NULL;

	if (!call)
	{
		clog_ast_expression_list_free(parser,list);
		return 0;
	}

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_expression_free(parser,call);
		clog_ast_expression_list_free(parser,list);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_call;
	(*expr)->lvalue = 1;
	(*expr)->constant = 0;
	(*expr)->expr.call = clog_malloc(sizeof(struct clog_ast_expression_call));
	if (!(*expr)->expr.call)
	{
		clog_ast_expression_free(parser,call);
		clog_ast_expression_list_free(parser,list);
		clog_free(*expr);
		*expr = NULL;
		return clog_out_of_memory(parser);
	}

	(*expr)->expr.call->expr = call;
	(*expr)->expr.call->params = list;

	return 1;
}

static int clog_ast_expression_eval(struct clog_parser* parser, struct clog_ast_literal** v, const struct clog_ast_expression* expr, const struct clog_ast_literal* id, const struct clog_ast_literal* value)
{
	*v = NULL;

	if (expr)
	{
		switch (expr->type)
		{
		case clog_ast_expression_identifier:
			if (value && clog_ast_literal_id_compare(id,expr->expr.identifier) == 0)
				return clog_ast_literal_clone(parser,v,value);
			break;

		case clog_ast_expression_literal:
			return clog_ast_literal_clone(parser,v,expr->expr.literal);

		case clog_ast_expression_call:
			break;

		case clog_ast_expression_builtin:
			switch (expr->expr.builtin->type)
			{
			case CLOG_TOKEN_ASSIGN:
			case CLOG_TOKEN_COMMA:
				return clog_ast_expression_eval(parser,v,expr->expr.builtin->args[1],id,value);

			default:
				break;
			}
			break;
		}
	}
	return 1;
}

struct clog_ast_reduction
{
	const struct clog_ast_literal* id;
	struct clog_ast_literal* value;
	int reduced;
};

static int clog_ast_expression_list_reduce(struct clog_parser* parser, struct clog_ast_expression_list* expr, struct clog_ast_reduction* reduction);
static int clog_ast_expression_reduce_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction);

static int clog_ast_expression_reduce(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	if (!*expr)
		return 1;

	switch ((*expr)->type)
	{
	case clog_ast_expression_identifier:
		if (reduction->value && clog_ast_literal_id_compare(reduction->id,(*expr)->expr.identifier) == 0)
		{
			struct clog_ast_literal* new_lit;

			if (!clog_ast_literal_clone(parser,&new_lit,reduction->value))
				return 0;

			clog_ast_expression_free(parser,*expr);
			if (!clog_ast_expression_alloc_literal(parser,expr,new_lit))
				return 0;

			reduction->reduced = 1;
		}
		return 1;

	case clog_ast_expression_literal:
		return 1;

	case clog_ast_expression_builtin:
		return clog_ast_expression_reduce_builtin(parser,expr,reduction);

	case clog_ast_expression_call:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.call->expr,reduction))
			return 0;

		if (!(*expr)->expr.call->expr->lvalue)
			return clog_syntax_error(parser,"Function call requires an lvalue",clog_ast_expression_line((*expr)->expr.call->expr));

		return clog_ast_expression_list_reduce(parser,(*expr)->expr.call->params,reduction);
	}

	return 0;
}

static int clog_ast_expression_reduce_builtin_replace(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* value)
{
	int ret;
	if (*expr && (*expr)->type == clog_ast_expression_builtin)
	{
		if ((*expr)->expr.builtin->args[0]->type == clog_ast_expression_builtin)
		{
			if ((*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_ASSIGN)
			{
				(*expr)->expr.builtin->type = CLOG_TOKEN_COMMA;

				if (!(*expr)->expr.builtin->args[1])
				{
					(*expr)->expr.builtin->args[1] = value;
					(*expr)->lvalue = value->lvalue;
					return 1;
				}

				return clog_ast_expression_reduce_builtin_replace(parser,&(*expr)->expr.builtin->args[1],value);
			}
			else if ((*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_COMMA)
			{
				struct clog_ast_expression* e;
				if (!(*expr)->expr.builtin->args[1])
				{
					if (!clog_ast_expression_alloc_builtin2(parser,&(*expr)->expr.builtin->args[0]->expr.builtin->args[1],CLOG_TOKEN_COMMA,(*expr)->expr.builtin->args[0]->expr.builtin->args[1],value))
						return 0;

					e = (*expr)->expr.builtin->args[0];
					(*expr)->expr.builtin->args[0] = NULL;
					clog_ast_expression_free(parser,*expr);
					*expr = e;
					return 1;
				}

				ret = clog_ast_expression_alloc_builtin2(parser,&(*expr)->expr.builtin->args[0]->expr.builtin->args[1],CLOG_TOKEN_COMMA,(*expr)->expr.builtin->args[0]->expr.builtin->args[1],(*expr)->expr.builtin->args[1]);
				(*expr)->expr.builtin->args[1] = NULL;
				if (!ret)
				{
					clog_ast_expression_free(parser,value);
					return 0;
				}

				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				clog_ast_expression_free(parser,*expr);
				*expr = e;
				return clog_ast_expression_reduce_builtin_replace(parser,&(*expr)->expr.builtin->args[1],value);
			}
		}
		else if ((*expr)->expr.builtin->args[1] && (*expr)->expr.builtin->args[1]->type == clog_ast_expression_builtin)
		{
			if ((*expr)->expr.builtin->args[1]->expr.builtin->type == CLOG_TOKEN_ASSIGN)
			{
				clog_ast_expression_free(parser,(*expr)->expr.builtin->args[0]);
				(*expr)->expr.builtin->type = CLOG_TOKEN_COMMA;
				(*expr)->expr.builtin->args[0] = (*expr)->expr.builtin->args[1];
				(*expr)->expr.builtin->args[1] = value;
				(*expr)->lvalue = value->lvalue;
				return 1;
			}

			if ((*expr)->expr.builtin->args[1]->expr.builtin->type == CLOG_TOKEN_COMMA)
			{
				struct clog_ast_expression* e = (*expr)->expr.builtin->args[1];
				(*expr)->expr.builtin->args[1] = NULL;
				clog_ast_expression_free(parser,*expr);
				*expr = e;
				return clog_ast_expression_reduce_builtin_replace(parser,expr,value);
			}
		}
	}

	clog_ast_expression_free(parser,*expr);
	*expr = value;
	return 1;
}

static int clog_ast_expression_reduce_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	struct clog_ast_expression* e = NULL;
	struct clog_ast_literal* p1 = NULL;
	struct clog_ast_literal* p2 = NULL;
	struct clog_ast_literal* p3 = NULL;

	/* Rewrite x = (A,B); => A,x=B; */
	if ((*expr)->expr.builtin->type == CLOG_TOKEN_ASSIGN &&
			(*expr)->expr.builtin->args[1]->type == clog_ast_expression_builtin &&
			(*expr)->expr.builtin->args[1]->expr.builtin->type == CLOG_TOKEN_COMMA)
	{
		(*expr)->expr.builtin->type = CLOG_TOKEN_COMMA;
		(*expr)->expr.builtin->args[1]->expr.builtin->type = CLOG_TOKEN_ASSIGN;
		e = (*expr)->expr.builtin->args[1]->expr.builtin->args[0];
		(*expr)->expr.builtin->args[1]->expr.builtin->args[0] = (*expr)->expr.builtin->args[0];
		(*expr)->expr.builtin->args[0] = e;
		(*expr)->lvalue = (*expr)->expr.builtin->args[1]->lvalue;
		reduction->reduced = 1;
	}

	switch ((*expr)->expr.builtin->type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
		if ((*expr)->expr.builtin->args[0]->type != clog_ast_expression_identifier && !clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction))
			return 0;
		if (!(*expr)->expr.builtin->args[0]->lvalue)
			return clog_syntax_error_token(parser,NULL," requires an lvalue",(*expr)->expr.builtin->type,NULL,clog_ast_expression_line((*expr)->expr.builtin->args[0]));

		if ((*expr)->expr.builtin->args[0]->type != clog_ast_expression_identifier)
		{
			/* Rewrite as comma */
			struct clog_ast_expression* e = (*expr)->expr.builtin->args[0];
			if (!clog_ast_expression_clone(parser,&(*expr)->expr.builtin->args[0],(*expr)->expr.builtin->args[0]->expr.builtin->args[0]) ||
					!clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_COMMA,e,*expr))
			{
				return 0;
			}
			reduction->reduced = 1;
			return 1;
		}

		if (!reduction->value || clog_ast_literal_id_compare(reduction->id,(*expr)->expr.builtin->args[0]->expr.identifier) != 0)
			return 1;

		if (!(*expr)->lvalue)
		{
			/* Rewrite postfix e++ => (++e,value) */
			if (!clog_ast_literal_clone(parser,&p1,reduction->value) || !clog_ast_expression_alloc_literal(parser,&e,p1))
				return 0;
			(*expr)->lvalue = 1;
			reduction->reduced = 1;
			return clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_COMMA,*expr,e);
		}
		break;

	case CLOG_TOKEN_ASSIGN:
	case CLOG_TOKEN_STAR_ASSIGN:
	case CLOG_TOKEN_SLASH_ASSIGN:
	case CLOG_TOKEN_PERCENT_ASSIGN:
	case CLOG_TOKEN_PLUS_ASSIGN:
	case CLOG_TOKEN_MINUS_ASSIGN:
	case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
	case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
	case CLOG_TOKEN_AMPERSAND_ASSIGN:
	case CLOG_TOKEN_BITWISE_CARET_ASSIGN:
	case CLOG_TOKEN_BAR_ASSIGN:
		if (((*expr)->expr.builtin->args[0]->type != clog_ast_expression_identifier &&
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction)) ||
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction))
		{
			return 0;
		}
		if (!(*expr)->expr.builtin->args[0]->lvalue)
			return clog_syntax_error_token(parser,NULL," requires an lvalue",(*expr)->expr.builtin->type,NULL,clog_ast_expression_line((*expr)->expr.builtin->args[0]));

		if ((*expr)->expr.builtin->args[0]->type == clog_ast_expression_builtin)
		{
			if ((*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_DOT ||
					(*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_OPEN_BRACKET)
			{
				return 1;
			}

			/* Rewrite as comma */
			if ((*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_COMMA)
			{
				/* Rotate around existing comma */
				struct clog_ast_expression* e0 = *expr;
				*expr = (*expr)->expr.builtin->args[0];
				e0->expr.builtin->args[0] = (*expr)->expr.builtin->args[1];
				(*expr)->expr.builtin->args[1] = e0;
			}
			else
			{
				struct clog_ast_expression* e = (*expr)->expr.builtin->args[0];
				if (!clog_ast_expression_clone(parser,&(*expr)->expr.builtin->args[0],(*expr)->expr.builtin->args[0]->expr.builtin->args[0]) ||
						!clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_COMMA,e,*expr))
				{
					return 0;
				}
			}
			reduction->reduced = 1;
			return 1;
		}

		if (!reduction->value || clog_ast_literal_id_compare(reduction->id,(*expr)->expr.builtin->args[0]->expr.identifier) != 0)
			return 1;

		if (!clog_ast_expression_eval(parser,&p2,(*expr)->expr.builtin->args[1],reduction->id,reduction->value))
			return 0;
		if (!p2)
		{
			clog_ast_literal_free(parser,reduction->value);
			reduction->value = NULL;
			return 1;
		}
		break;

	case CLOG_TOKEN_DOT:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction))
			return 0;
		if (!(*expr)->expr.builtin->args[0]->lvalue)
			return clog_syntax_error_token(parser,NULL," requires an lvalue",(*expr)->expr.builtin->type,NULL,clog_ast_expression_line((*expr)->expr.builtin->args[0]));
		return 1;

	case CLOG_TOKEN_OPEN_BRACKET:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction))
			return 0;
		if (!(*expr)->expr.builtin->args[0]->lvalue)
			return clog_syntax_error(parser,"Subscript requires an lvalue",clog_ast_expression_line((*expr)->expr.builtin->args[0]));
		return clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction);

	case CLOG_TOKEN_COMMA:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction) ||
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction))
		{
			return 0;
		}
		if ((*expr)->expr.builtin->args[0]->constant)
		{
			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
			clog_ast_expression_free(parser,*expr);
			*expr = e;
			reduction->reduced = 1;
		}
		return 1;

	case CLOG_TOKEN_AND:
	case CLOG_TOKEN_OR:
	case CLOG_TOKEN_QUESTION:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction) ||
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction) ||
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[2],reduction) ||
				!clog_ast_expression_eval(parser,&p1,(*expr)->expr.builtin->args[0],reduction->id,reduction->value))
		{
			return 0;
		}
		break;

	default:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction) ||
				!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction) ||
				!clog_ast_expression_eval(parser,&p1,(*expr)->expr.builtin->args[0],reduction->id,reduction->value) ||
				!clog_ast_expression_eval(parser,&p2,(*expr)->expr.builtin->args[1],reduction->id,reduction->value))
		{
			return 0;
		}
		break;
	}

	switch ((*expr)->expr.builtin->type)
	{
	case CLOG_TOKEN_PLUS:
		if (p1)
		{
			if (!p2)
			{
				if ((*expr)->expr.builtin->args[1])
					break;

				/* Unary + */
				if (p1->type != clog_ast_literal_integer &&
					p1->type != clog_ast_literal_real)
				{
					clog_syntax_error(parser,"+ requires a number",p1->line);
					goto failed;
				}
				goto replace_with_p1;
			}

			if (p1->type == clog_ast_literal_string && p2->type == clog_ast_literal_string)
			{
				if (!p1->value.string.len)
				{
					/* Swap via p3 */
					p3 = p2;
					p2 = p1;
					p1 = p3;
				}

				if (p2->value.string.len)
				{
					unsigned char* sz = clog_realloc(p1->value.string.str,p1->value.string.len + p2->value.string.len);
					if (!sz)
					{
						clog_out_of_memory(parser);
						goto failed;
					}

					memcpy(sz+p1->value.string.len,p2->value.string.str,p2->value.string.len);
					p1->value.string.str = sz;
					p1->value.string.len += p2->value.string.len;
				}
				goto replace_with_p1;
			}

			if (p1->type != clog_ast_literal_string && p2->type != clog_ast_literal_string)
			{
				if (!clog_ast_literal_arith_convert(p1,p2))
				{
					clog_syntax_error(parser,"+ requires numbers or strings",p1->line);
					goto failed;
				}

				if (p1->type == clog_ast_literal_real)
					p1->value.real += p2->value.real;
				else
					p1->value.integer += p2->value.integer;

				goto replace_with_p1;
			}
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1)
		{
			if (!p2)
			{
				if ((*expr)->expr.builtin->args[1])
					break;

				/* Unary - */
				switch (p1->type)
				{
				case clog_ast_literal_integer:
					p1->value.integer = -p1->value.integer;
					break;

				case clog_ast_literal_real:
					p1->value.real = -p1->value.real;
					break;

				default:
					clog_syntax_error(parser,"- requires a number",p1->line);
					goto failed;
				}
				goto replace_with_p1;
			}
			else if (p2)
			{
				if (!clog_ast_literal_arith_convert(p1,p2))
				{
					clog_syntax_error(parser,"- requires numbers",p1->line);
					goto failed;
				}

				if (p1->type == clog_ast_literal_real)
					p1->value.real -= p2->value.real;
				else
					p1->value.integer -= p2->value.integer;

				goto replace_with_p1;
			}
		}
		break;

	case CLOG_TOKEN_EXCLAMATION:
		/* Logical NOT */
		if (p1)
		{
			clog_ast_literal_bool_promote(p1);
			p1->value.integer = (p1->value.integer == 0 ? 1 : 0);
			goto replace_with_p1;
		}
		if ((*expr)->expr.builtin->args[0]->type == clog_ast_expression_builtin)
		{
			struct clog_ast_expression* e = NULL;

			/* Negate any equality operator */
			switch ((*expr)->expr.builtin->args[0]->expr.builtin->type)
			{
			case CLOG_TOKEN_EXCLAMATION:
				e = (*expr)->expr.builtin->args[0]->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0]->expr.builtin->args[0] = NULL;
				break;

			case CLOG_TOKEN_LESS_THAN:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_GREATER_THAN_EQUALS;
				break;

			case CLOG_TOKEN_GREATER_THAN:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_LESS_THAN_EQUALS;
				break;

			case CLOG_TOKEN_LESS_THAN_EQUALS:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_GREATER_THAN;
				break;

			case CLOG_TOKEN_GREATER_THAN_EQUALS:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_LESS_THAN;
				break;

			case CLOG_TOKEN_EQUALS:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_NOT_EQUALS;
				break;

			case CLOG_TOKEN_NOT_EQUALS:
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				e->expr.builtin->type = CLOG_TOKEN_EQUALS;
				break;

			default:
				break;
			}

			if (e)
			{
				clog_ast_expression_free(parser,*expr);
				*expr = e;
				reduction->reduced = 1;
			}
		}
		break;

	case CLOG_TOKEN_TILDA:
		/* Bitwise 1s complement */
		if (p1)
		{
			if (!clog_ast_literal_int_promote(p1))
				return clog_syntax_error(parser,"~ requires an integer",p1->line);

			p1->value.integer = ~p1->value.integer;
			goto replace_with_p1;
		}
		break;

	case CLOG_TOKEN_STAR:
		if (p1 && p2)
		{
			if (!clog_ast_literal_arith_convert(p1,p2))
			{
				clog_syntax_error(parser,"* requires numbers",p1->line);
				goto failed;
			}

			if (p1->type == clog_ast_literal_real)
				p1->value.real *= p2->value.real;
			else
				p1->value.integer *= p2->value.integer;
			goto replace_with_p1;
		}
		break;

	case CLOG_TOKEN_SLASH:
		if (p1 && p2)
		{
			if (!clog_ast_literal_arith_convert(p1,p2))
			{
				clog_syntax_error(parser,"/ requires numbers",p1->line);
				goto failed;
			}

			if (p1->type == clog_ast_literal_real)
			{
				if (p2->value.real == 0.0)
				{
					clog_syntax_error(parser,"Division by 0.0",p2->line);
					goto failed;
				}
				p1->value.real /= p2->value.real;
			}
			else
			{
				if (p2->value.integer == 0)
				{
					clog_syntax_error(parser,"Division by 0",p2->line);
					goto failed;
				}
				p1->value.integer /= p2->value.integer;
			}
			goto replace_with_p1;
		}
		break;

	case CLOG_TOKEN_PERCENT:
		if (p1 && p2)
		{
			if (!clog_ast_literal_int_promote(p1) ||
					!clog_ast_literal_int_promote(p2))
			{
				clog_syntax_error(parser,"% requires integers",p1->line);
				goto failed;
			}

			p1->value.integer %= p2->value.integer;
			goto replace_with_p1;
		}
		break;

	case CLOG_TOKEN_RIGHT_SHIFT:
	case CLOG_TOKEN_LEFT_SHIFT:
		if (p1 && p2)
		{
			if (clog_ast_literal_int_promote(p1) && clog_ast_literal_int_promote(p2))
			{
				if ((*expr)->expr.builtin->type == CLOG_TOKEN_RIGHT_SHIFT)
					p1->value.integer >>= p2->value.integer;
				else
					p1->value.integer <<= p2->value.integer;

				goto replace_with_p1;
			}
			clog_syntax_error_token(parser,NULL," requires integers",(*expr)->expr.builtin->type,NULL,p1->line);
			goto failed;
		}
		break;

	case CLOG_TOKEN_LESS_THAN:
	case CLOG_TOKEN_GREATER_THAN:
	case CLOG_TOKEN_LESS_THAN_EQUALS:
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
	case CLOG_TOKEN_EQUALS:
	case CLOG_TOKEN_NOT_EQUALS:
		if (p1 && p2)
		{
			int b = clog_ast_literal_compare(p1,p2);
			if (b == -2)
			{
				clog_syntax_error_token(parser,NULL," requires numbers or strings",(*expr)->expr.builtin->type,NULL,p1->line);
				goto failed;
			}
			switch ((*expr)->expr.builtin->type)
			{
			case CLOG_TOKEN_LESS_THAN:
				p1->value.integer = (b < 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN:
				p1->value.integer = (b > 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_LESS_THAN_EQUALS:
				p1->value.integer = (b <= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN_EQUALS:
				p1->value.integer = (b >= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_EQUALS:
				p1->value.integer = (b == 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_NOT_EQUALS:
				p1->value.integer = (b != 0 ? 1 : 0);
				break;
			}

			p1->type = clog_ast_literal_bool;
			goto replace_with_p1;
		}
		break;

	case CLOG_TOKEN_AND:
		if (p1)
		{
			clog_ast_literal_bool_promote(p1);
			if (!p1->value.integer)
				goto replace_with_p1;

			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
			if (!clog_ast_expression_reduce_builtin_replace(parser,expr,e))
				goto failed;
			reduction->reduced = 1;
		}
		break;

	case CLOG_TOKEN_OR:
		if (p1)
		{
			clog_ast_literal_bool_promote(p1);
			if (p1->value.integer)
				goto replace_with_p1;

			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
			if (!clog_ast_expression_reduce_builtin_replace(parser,expr,e))
				goto failed;
			reduction->reduced = 1;
		}
		break;

	case CLOG_TOKEN_BAR:
	case CLOG_TOKEN_CARET:
	case CLOG_TOKEN_AMPERSAND:
		if (p1 && p2)
		{
			if (clog_ast_literal_int_promote(p1) && clog_ast_literal_int_promote(p2))
			{
				if ((*expr)->expr.builtin->type == CLOG_TOKEN_BAR)
					p1->value.integer |= p2->value.integer;
				else if ((*expr)->expr.builtin->type == CLOG_TOKEN_CARET)
					p1->value.integer ^= p2->value.integer;
				else
					p1->value.integer &= p2->value.integer;

				goto replace_with_p1;
			}
			clog_syntax_error_token(parser,NULL," requires integers",(*expr)->expr.builtin->type,NULL,p1->line);
			goto failed;
		}
		break;

	case CLOG_TOKEN_QUESTION:
		if (p1)
		{
			if (clog_ast_literal_bool_cast(p1))
			{
				e = (*expr)->expr.builtin->args[1];
				(*expr)->expr.builtin->args[1] = NULL;
			}
			else
			{
				e = (*expr)->expr.builtin->args[2];
				(*expr)->expr.builtin->args[2] = NULL;
			}
			if (!clog_ast_expression_reduce_builtin_replace(parser,expr,e))
				goto failed;
			reduction->reduced = 1;
		}
		break;

	case CLOG_TOKEN_ASSIGN:
		clog_ast_literal_free(parser,reduction->value);
		if (!clog_ast_literal_clone(parser,&reduction->value,p2))
			goto failed;
		break;

	case CLOG_TOKEN_DOUBLE_PLUS:
		/* Prefix ++ */
		if (reduction->value->type == clog_ast_literal_real)
			++reduction->value->value.real;
		else
		{
			if (!clog_ast_literal_int_promote(reduction->value))
				goto failed;
			++reduction->value->value.integer;
		}
		goto replace_with_assign;

	case CLOG_TOKEN_DOUBLE_MINUS:
		/* Prefix -- */
		if (reduction->value->type == clog_ast_literal_real)
			++reduction->value->value.real;
		else
		{
			if (!clog_ast_literal_int_promote(reduction->value))
				goto failed;
			++reduction->value->value.integer;
		}
		goto replace_with_assign;

	case CLOG_TOKEN_STAR_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p2))
		{
			clog_syntax_error(parser,"*= requires a number",p2->line);
			goto failed;
		}
		if (reduction->value->type == clog_ast_literal_real)
			reduction->value->value.real *= p2->value.real;
		else
			reduction->value->value.integer *= p2->value.integer;
		goto replace_with_assign;

	case CLOG_TOKEN_SLASH_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p2))
		{
			clog_syntax_error(parser,"/= requires a number",p1->line);
			goto failed;
		}
		if (reduction->value->type == clog_ast_literal_real)
		{
			if (p2->value.real == 0.0)
			{
				clog_syntax_error(parser,"Division by 0.0",p2->line);
				goto failed;
			}
			reduction->value->value.real /= p2->value.real;
		}
		else
		{
			if (p2->value.integer == 0)
			{
				clog_syntax_error(parser,"Division by 0",p2->line);
				goto failed;
			}
			reduction->value->value.integer /= p2->value.integer;
		}
		goto replace_with_assign;

	case CLOG_TOKEN_PERCENT_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) ||
				!clog_ast_literal_int_promote(p2))
		{
			clog_syntax_error(parser,"%= require an integer",p1->line);
			goto failed;
		}
		reduction->value->value.integer %= p2->value.integer;
		goto replace_with_assign;

	case CLOG_TOKEN_PLUS_ASSIGN:
		if (reduction->value->type == clog_ast_literal_string && p2->type == clog_ast_literal_string)
		{
			if (!reduction->value->value.string.len)
			{
				clog_ast_literal_free(parser,reduction->value);
				reduction->value = p2;
				p2 = NULL;
			}
			else if (p2->value.string.len)
			{
				unsigned char* sz = clog_realloc(reduction->value->value.string.str,reduction->value->value.string.len + p2->value.string.len);
				if (!sz)
				{
					clog_out_of_memory(parser);
					goto failed;
				}

				memcpy(sz+reduction->value->value.string.len,p2->value.string.str,p2->value.string.len);
				reduction->value->value.string.str = sz;
				reduction->value->value.string.len += p2->value.string.len;
			}
		}
		else
		{
			if (!clog_ast_literal_arith_convert(reduction->value,p2))
			{
				clog_syntax_error(parser,"+= requires a number or string",p2->line);
				goto failed;
			}
			if (reduction->value->type == clog_ast_literal_real)
				reduction->value->value.real += p2->value.real;
			else
				reduction->value->value.integer += p2->value.integer;
		}
		goto replace_with_assign;

	case CLOG_TOKEN_MINUS_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p2))
		{
			clog_syntax_error(parser,"-= require a number",p2->line);
			goto failed;
		}
		if (reduction->value->type == clog_ast_literal_real)
			reduction->value->value.real -= p2->value.real;
		else
			reduction->value->value.integer -= p2->value.integer;
		goto replace_with_assign;

	case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
	case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) || !clog_ast_literal_int_promote(p2))
		{
			clog_syntax_error_token(parser,NULL," requires an integer",(*expr)->expr.builtin->type,NULL,p2->line);
			goto failed;
		}
		if ((*expr)->expr.builtin->type == CLOG_TOKEN_RIGHT_SHIFT)
			reduction->value->value.integer >>= p2->value.integer;
		else
			reduction->value->value.integer <<= p2->value.integer;
		goto replace_with_assign;

	case CLOG_TOKEN_AMPERSAND_ASSIGN:
	case CLOG_TOKEN_BITWISE_CARET_ASSIGN:
	case CLOG_TOKEN_BAR_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) || !clog_ast_literal_int_promote(p2))
		{
			clog_syntax_error_token(parser,NULL," requires an integer",(*expr)->expr.builtin->type,NULL,p2->line);
			goto failed;
		}
		if ((*expr)->expr.builtin->type == CLOG_TOKEN_BAR)
			reduction->value->value.integer |= p2->value.integer;
		else if ((*expr)->expr.builtin->type == CLOG_TOKEN_CARET)
			reduction->value->value.integer ^= p2->value.integer;
		else
			reduction->value->value.integer &= p2->value.integer;
		goto replace_with_assign;

	default:
		break;
	}

success:
	clog_ast_literal_free(parser,p1);
	clog_ast_literal_free(parser,p2);
	clog_ast_literal_free(parser,p3);
	return 1;

replace_with_p1:
	if (!clog_ast_expression_alloc_literal(parser,&e,p1))
	{
		p1 = NULL;
		goto failed;
	}
	p1 = NULL;
	if (!clog_ast_expression_reduce_builtin_replace(parser,expr,e))
		goto failed;
	reduction->reduced = 1;
	goto success;

replace_with_assign:
	{
		struct clog_ast_literal* p;
		if (!clog_ast_literal_clone(parser,&p,reduction->value) || !clog_ast_expression_alloc_literal(parser,&e,p))
			goto failed;
		(*expr)->expr.builtin->type = CLOG_TOKEN_ASSIGN;
		clog_ast_expression_free(parser,(*expr)->expr.builtin->args[1]);
		(*expr)->expr.builtin->args[1] = NULL;
		if (!clog_ast_expression_reduce_builtin_replace(parser,&(*expr)->expr.builtin->args[1],e))
			goto failed;
		reduction->reduced = 1;
		goto success;
	}

failed:
	clog_ast_literal_free(parser,p1);
	clog_ast_literal_free(parser,p2);
	clog_ast_literal_free(parser,p3);
	return 0;
}

void clog_ast_expression_list_free(struct clog_parser* parser, struct clog_ast_expression_list* list)
{
	if (list)
	{
		clog_ast_expression_free(parser,list->expr);
		clog_ast_expression_list_free(parser,list->next);

		clog_free(list);
	}
}

int clog_ast_expression_list_alloc(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr)
{
	if (!expr)
		return 0;

	*list = clog_malloc(sizeof(struct clog_ast_expression_list));
	if (!*list)
	{
		clog_ast_expression_free(parser,expr);
		return clog_out_of_memory(parser);
	}

	(*list)->expr = expr;
	(*list)->next = NULL;

	return 1;
}

static int clog_ast_expression_list_clone(struct clog_parser* parser, struct clog_ast_expression_list** new, const struct clog_ast_expression_list* list)
{
	*new = NULL;

	if (!list)
		return 1;

	*new = clog_malloc(sizeof(struct clog_ast_expression_list));
	if (!*new)
		return clog_out_of_memory(parser);

	if (!clog_ast_expression_clone(parser,&(*new)->expr,list->expr))
	{
		clog_free(*new);
		*new = NULL;
		return 0;
	}

	if (!clog_ast_expression_list_clone(parser,&(*new)->next,list->next))
	{
		clog_ast_expression_free(parser,(*new)->expr);
		clog_free(*new);
		*new = NULL;
		return 0;
	}

	return 1;
}

struct clog_ast_expression_list* clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_expression* expr)
{
	if (!expr || !list)
	{
		clog_ast_expression_free(parser,expr);
		clog_ast_expression_list_free(parser,list);
		return NULL;
	}
	else
	{
		struct clog_ast_expression** tail = &list->expr;
		struct clog_ast_expression_list* next = list;

		while (next)
		{
			tail = &list->expr;
			next = next->next;
		}

		*tail = expr;

		return list;
	}
}

static int clog_ast_expression_list_reduce(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_reduction* reduction)
{
	for (;list && !reduction->reduced;list = list->next)
	{
		if (!clog_ast_expression_reduce(parser,&list->expr,reduction))
			return 0;
	}
	return 1;
}

static void clog_ast_statement_free(struct clog_parser* parser, struct clog_ast_statement* stmt)
{
	if (stmt)
	{
		switch (stmt->type)
		{
		case clog_ast_statement_expression:
		case clog_ast_statement_return:
			clog_ast_expression_free(parser,stmt->stmt.expression);
			break;

		case clog_ast_statement_block:
			clog_ast_statement_list_free(parser,stmt->stmt.block);
			break;

		case clog_ast_statement_declaration:
			clog_ast_literal_free(parser,stmt->stmt.declaration);
			break;

		case clog_ast_statement_if:
			clog_ast_expression_free(parser,stmt->stmt.if_stmt->condition);
			clog_ast_statement_list_free(parser,stmt->stmt.if_stmt->true_stmt);
			clog_ast_statement_list_free(parser,stmt->stmt.if_stmt->false_stmt);
			clog_free(stmt->stmt.if_stmt);
			break;

		case clog_ast_statement_do:
			clog_ast_expression_free(parser,stmt->stmt.do_stmt->condition);
			clog_ast_statement_list_free(parser,stmt->stmt.do_stmt->loop_stmt);
			clog_free(stmt->stmt.do_stmt);
			break;

		case clog_ast_statement_break:
		case clog_ast_statement_continue:
			break;
		}

		clog_free(stmt);
	}
}

void clog_ast_statement_list_free(struct clog_parser* parser, struct clog_ast_statement_list* list)
{
	if (list)
	{
		clog_ast_statement_free(parser,list->stmt);
		clog_ast_statement_list_free(parser,list->next);

		clog_free(list);
	}
}

int clog_ast_statement_list_alloc(struct clog_parser* parser, struct clog_ast_statement_list** list, enum clog_ast_statement_type type)
{
	*list = clog_malloc(sizeof(struct clog_ast_statement_list));
	if (!*list)
		return clog_out_of_memory(parser);

	(*list)->stmt = clog_malloc(sizeof(struct clog_ast_statement));
	if (!(*list)->stmt)
	{
		clog_free(*list);
		*list = NULL;
		return clog_out_of_memory(parser);
	}
	(*list)->next = NULL;
	(*list)->stmt->type = type;

	return 1;
}

int clog_ast_statement_list_alloc_expression(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr)
{
	*list = NULL;

	if (!expr)
		return 0;

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_expression))
	{
		clog_ast_expression_free(parser,expr);
		return 0;
	}

	(*list)->stmt->stmt.expression = expr;

	return 1;
}

static void clog_ast_statement_list_flatten(struct clog_parser* parser, struct clog_ast_statement_list** list)
{
	if (!*list)
		return;

	if ((*list)->stmt->type == clog_ast_statement_block)
	{
		struct clog_ast_statement_list* find = (*list)->stmt->stmt.block;
		for (;find;find = find->next)
		{
			if (find->stmt->type == clog_ast_statement_declaration)
				break;
		}

		if (!find)
		{
			struct clog_ast_statement_list* l = (*list)->stmt->stmt.block;
			l = clog_ast_statement_list_append(parser,l,(*list)->next);
			(*list)->stmt->stmt.block = NULL;
			(*list)->next = NULL;
			clog_ast_statement_list_free(parser,*list);
			*list = l;
		}
	}

	clog_ast_statement_list_flatten(parser,&(*list)->next);
}

static int clog_ast_statement_list_reduce_block(struct clog_parser* parser, struct clog_ast_statement_list** block, struct clog_ast_reduction* reduction);
static int clog_ast_statement_list_reduce_if(struct clog_parser* parser, struct clog_ast_statement_list** if_stmt, struct clog_ast_reduction* reduction);
static int clog_ast_statement_list_reduce_do(struct clog_parser* parser, struct clog_ast_statement_list** if_stmt, struct clog_ast_reduction* reduction);

static int clog_ast_statement_list_reduce(struct clog_parser* parser, struct clog_ast_statement_list** stmt, struct clog_ast_reduction* reduction)
{
	/* Reduce each statement */
	while (*stmt)
	{
		switch ((*stmt)->stmt->type)
		{
		case clog_ast_statement_expression:
			if (!clog_ast_expression_reduce(parser,&(*stmt)->stmt->stmt.expression,reduction))
				return 0;

			if ((*stmt)->stmt->stmt.expression->type == clog_ast_expression_builtin)
			{
				switch ((*stmt)->stmt->stmt.expression->expr.builtin->type)
				{
				case CLOG_TOKEN_COMMA:
					{
						/* Rewrite comma expressions as statements */
						struct clog_ast_expression* e0 = (*stmt)->stmt->stmt.expression->expr.builtin->args[0];
						struct clog_ast_expression* e1 = (*stmt)->stmt->stmt.expression->expr.builtin->args[1];
						struct clog_ast_statement_list* n = (*stmt)->next;

						(*stmt)->stmt->stmt.expression->expr.builtin->args[0] = NULL;
						(*stmt)->stmt->stmt.expression->expr.builtin->args[1] = NULL;
						clog_ast_expression_free(parser,(*stmt)->stmt->stmt.expression);
						(*stmt)->stmt->stmt.expression = e0;

						if (!clog_ast_statement_list_alloc_expression(parser,&(*stmt)->next,e1))
						{
							(*stmt)->next = n;
							return 0;
						}

						*stmt = clog_ast_statement_list_append(parser,*stmt,n);
						reduction->reduced = 1;
					}
					break;

				case CLOG_TOKEN_LESS_THAN:
				case CLOG_TOKEN_GREATER_THAN:
				case CLOG_TOKEN_LESS_THAN_EQUALS:
				case CLOG_TOKEN_GREATER_THAN_EQUALS:
				case CLOG_TOKEN_EQUALS:
				case CLOG_TOKEN_NOT_EQUALS:
				case CLOG_TOKEN_AMPERSAND:
				case CLOG_TOKEN_CARET:
				case CLOG_TOKEN_BAR:
				case CLOG_TOKEN_AND:
				case CLOG_TOKEN_OR:
					if ((*stmt)->stmt->stmt.expression->expr.builtin->args[0]->constant)
					{
						struct clog_ast_expression* e = (*stmt)->stmt->stmt.expression->expr.builtin->args[1];
						(*stmt)->stmt->stmt.expression->expr.builtin->args[1] = NULL;
						clog_ast_expression_free(parser,(*stmt)->stmt->stmt.expression);
						(*stmt)->stmt->stmt.expression = e;
						reduction->reduced = 1;
					}
					else if ((*stmt)->stmt->stmt.expression->expr.builtin->args[1]->constant)
					{
						struct clog_ast_expression* e = (*stmt)->stmt->stmt.expression->expr.builtin->args[0];
						(*stmt)->stmt->stmt.expression->expr.builtin->args[0] = NULL;
						clog_ast_expression_free(parser,(*stmt)->stmt->stmt.expression);
						(*stmt)->stmt->stmt.expression = e;
						reduction->reduced = 1;
					}
					break;

				case CLOG_TOKEN_EXCLAMATION:
				case CLOG_TOKEN_TILDA:
					{
						struct clog_ast_expression* e = (*stmt)->stmt->stmt.expression->expr.builtin->args[0];
						(*stmt)->stmt->stmt.expression->expr.builtin->args[0] = NULL;
						clog_ast_expression_free(parser,(*stmt)->stmt->stmt.expression);
						(*stmt)->stmt->stmt.expression = e;
						reduction->reduced = 1;
					}
					break;

				default:
					break;
				}
			}

			if ((*stmt)->stmt->stmt.expression->constant)
				goto drop;

			break;

		case clog_ast_statement_block:
			if (!clog_ast_statement_list_reduce_block(parser,&(*stmt)->stmt->stmt.block,reduction))
				return 0;

			if (!(*stmt)->stmt->stmt.block)
				goto drop;

			break;

		case clog_ast_statement_declaration:
			/* Rewrite comma expressions as statements before the declaration */
			if ((*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->type == clog_ast_expression_builtin &&
					(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->expr.builtin->type == CLOG_TOKEN_COMMA)
			{
				struct clog_ast_expression* e0 = (*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->expr.builtin->args[0];
				struct clog_ast_expression* e1 = (*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->expr.builtin->args[1];
				struct clog_ast_statement_list* p = *stmt;

				(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->expr.builtin->args[0] = NULL;
				(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]->expr.builtin->args[1] = NULL;
				clog_ast_expression_free(parser,(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1]);
				(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1] = e1;

				if (!clog_ast_statement_list_alloc_expression(parser,stmt,e0))
				{
					*stmt = p;
					return 0;
				}
				*stmt = clog_ast_statement_list_append(parser,*stmt,p);
				reduction->reduced = 1;
				continue;
			}

			stmt = &(*stmt)->next;

			/* Reduce the assignment expression that follows */
			if (!clog_ast_expression_reduce(parser,&(*stmt)->stmt->stmt.expression->expr.builtin->args[1],reduction))
				return 0;

			/* Check if new variable hides the one we are reducing */
			if (reduction->id && clog_ast_literal_id_compare(reduction->id,(*stmt)->stmt->stmt.expression->expr.builtin->args[0]->expr.identifier) == 0)
			{
				int ret = 1;
				struct clog_ast_literal* old_value = reduction->value;

				ret = clog_ast_expression_eval(parser,&reduction->value,(*stmt)->stmt->stmt.expression->expr.builtin->args[1],reduction->id,reduction->value);
				if (ret && reduction->value)
					ret = clog_ast_statement_list_reduce(parser,&(*stmt)->next,reduction);

				clog_ast_literal_free(parser,reduction->value);
				reduction->value = old_value;
				return ret;
			}
			break;

		case clog_ast_statement_if:
			if (!clog_ast_statement_list_reduce_if(parser,stmt,reduction))
				return 0;
			break;

		case clog_ast_statement_do:
			if (!clog_ast_statement_list_reduce_do(parser,stmt,reduction))
				return 0;
			break;

		case clog_ast_statement_return:
			if (!clog_ast_expression_reduce(parser,&(*stmt)->stmt->stmt.expression,reduction))
				return 0;

			/* Anything after return is unreachable... */
			if ((*stmt)->next)
			{
				reduction->reduced = 1;
				clog_ast_statement_list_free(parser,(*stmt)->next);
				(*stmt)->next = NULL;
			}
			return 1;

		case clog_ast_statement_break:
		case clog_ast_statement_continue:
			/* Anything after break or continue is unreachable... */
			if ((*stmt)->next)
			{
				reduction->reduced = 1;
				clog_ast_statement_list_free(parser,(*stmt)->next);
				(*stmt)->next = NULL;
			}
			return 1;
		}

		stmt = &(*stmt)->next;
		continue;

	drop:
		{
			struct clog_ast_statement_list* next = (*stmt)->next;
			(*stmt)->next = NULL;
			clog_ast_statement_list_free(parser,*stmt);
			*stmt = next;
			reduction->reduced = 1;
		}
	}

	return 1;
}

static int clog_ast_statement_list_reduce_block(struct clog_parser* parser, struct clog_ast_statement_list** block, struct clog_ast_reduction* reduction)
{
	struct clog_ast_statement_list* l;

	if (!clog_ast_statement_list_reduce(parser,block,reduction))
		return 0;

	/* Now reduce the block with each declared variable */
	for (l = *block;l;l = l->next)
	{
		struct clog_ast_reduction reduction2;
		if (l->stmt->type == clog_ast_statement_declaration)
		{
			/* Declaration is always followed by an assign */
			reduction2.id = l->stmt->stmt.declaration;
			reduction2.reduced = 0;

			l = l->next;

			/* Reduce the assignment expression that follows */
			if (!clog_ast_expression_reduce(parser,&l->stmt->stmt.expression->expr.builtin->args[1],reduction))
				return 0;

			if (!clog_ast_expression_eval(parser,&reduction2.value,l->stmt->stmt.expression->expr.builtin->args[1],NULL,NULL))
				return 0;

			if (reduction2.value)
			{
				int ret = clog_ast_statement_list_reduce(parser,&l->next,&reduction2);
				clog_ast_literal_free(parser,reduction2.value);
				if (!ret)
					return 0;

				if (reduction2.reduced)
					reduction->reduced = 1;
			}
		}
	}

	return 1;
}

static int clog_ast_statement_list_reduce_if(struct clog_parser* parser, struct clog_ast_statement_list** if_stmt, struct clog_ast_reduction* reduction)
{
	struct clog_ast_literal* if_lit;
	struct clog_ast_literal* true_value = NULL;
	struct clog_ast_literal* false_value = NULL;

	if (!clog_ast_expression_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->condition,reduction))
		return 0;

	/* Check for literal condition */
	if (!clog_ast_expression_eval(parser,&if_lit,(*if_stmt)->stmt->stmt.if_stmt->condition,reduction->id,reduction->value))
		return 0;
	if (if_lit)
	{
		struct clog_ast_statement_list* l1;
		struct clog_ast_statement_list* l2;
		if (clog_ast_literal_bool_cast(if_lit))
		{
			l2 = (*if_stmt)->stmt->stmt.if_stmt->true_stmt;
			(*if_stmt)->stmt->stmt.if_stmt->true_stmt = NULL;
		}
		else
		{
			l2 = (*if_stmt)->stmt->stmt.if_stmt->false_stmt;
			(*if_stmt)->stmt->stmt.if_stmt->false_stmt = NULL;
		}

		l1 = (*if_stmt);
		*if_stmt = clog_ast_statement_list_append(parser,l2,(*if_stmt)->next);

		l1->next = NULL;
		clog_ast_statement_list_free(parser,l1);
		reduction->reduced = 1;

		clog_ast_literal_free(parser,if_lit);
		return 1;
	}

	/* Reduce both statements */
	if (reduction->value)
	{
		if ((*if_stmt)->stmt->stmt.if_stmt->true_stmt)
		{
			struct clog_ast_literal* old_value = reduction->value;
			if (!clog_ast_literal_clone(parser,&reduction->value,old_value))
				return 0;

			if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->true_stmt,reduction))
			{
				clog_ast_literal_free(parser,old_value);
				return 0;
			}

			true_value = reduction->value;
			reduction->value = old_value;
		}
		if ((*if_stmt)->stmt->stmt.if_stmt->false_stmt)
		{
			struct clog_ast_literal* old_value = reduction->value;
			if (!clog_ast_literal_clone(parser,&reduction->value,old_value))
				return 0;

			if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->false_stmt,reduction))
			{
				clog_ast_literal_free(parser,old_value);
				return 0;
			}

			false_value = reduction->value;
			reduction->value = old_value;
		}

		/* If both sides result in the same value, carry on */
		if ((*if_stmt)->stmt->stmt.if_stmt->true_stmt &&
				(*if_stmt)->stmt->stmt.if_stmt->false_stmt)
		{
			if (clog_ast_literal_compare(true_value,false_value) == 0)
			{
				clog_ast_literal_free(parser,false_value);
				clog_ast_literal_free(parser,reduction->value);
				reduction->value = true_value;
			}
			else
			{
				clog_ast_literal_free(parser,true_value);
				clog_ast_literal_free(parser,false_value);
				clog_ast_literal_free(parser,reduction->value);
				reduction->value = NULL;
			}
		}
		else if ((*if_stmt)->stmt->stmt.if_stmt->true_stmt)
		{
			if (clog_ast_literal_compare(true_value,reduction->value) == 0)
				clog_ast_literal_free(parser,true_value);
			else
			{
				clog_ast_literal_free(parser,true_value);
				clog_ast_literal_free(parser,reduction->value);
				reduction->value = NULL;
			}
		}
		else if ((*if_stmt)->stmt->stmt.if_stmt->false_stmt)
		{
			if (clog_ast_literal_compare(false_value,reduction->value) == 0)
				clog_ast_literal_free(parser,false_value);
			else
			{
				clog_ast_literal_free(parser,false_value);
				clog_ast_literal_free(parser,reduction->value);
				reduction->value = NULL;
			}
		}
	}
	else if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->true_stmt,reduction) ||
				!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->false_stmt,reduction))
	{
		return 0;
	}

	/* If we have no result statements replace with condition */
	if (!(*if_stmt)->stmt->stmt.if_stmt->true_stmt && !(*if_stmt)->stmt->stmt.if_stmt->false_stmt)
	{
		struct clog_ast_expression* e = (*if_stmt)->stmt->stmt.if_stmt->condition;
		struct clog_ast_statement_list* n = (*if_stmt)->next;
		(*if_stmt)->next = NULL;
		(*if_stmt)->stmt->stmt.if_stmt->condition = NULL;
		clog_ast_statement_list_free(parser,*if_stmt);
		if (!clog_ast_statement_list_alloc_expression(parser,if_stmt,e))
			return 0;
		(*if_stmt)->next = n;
		reduction->reduced = 1;
		return 1;
	}

	/* If we have no true_stmt, negate the condition and swap */
	if (!(*if_stmt)->stmt->stmt.if_stmt->true_stmt)
	{
		if (!clog_ast_expression_alloc_builtin1(parser,&(*if_stmt)->stmt->stmt.if_stmt->condition,CLOG_TOKEN_EXCLAMATION,(*if_stmt)->stmt->stmt.if_stmt->condition))
			return 0;

		(*if_stmt)->stmt->stmt.if_stmt->true_stmt = (*if_stmt)->stmt->stmt.if_stmt->false_stmt;
		(*if_stmt)->stmt->stmt.if_stmt->false_stmt = NULL;

		reduction->reduced = 1;
	}

	return 1;
}

static int clog_ast_statement_list_reduce_do(struct clog_parser* parser, struct clog_ast_statement_list** do_stmt, struct clog_ast_reduction* reduction)
{
	struct clog_ast_literal* if_lit;

	/* Clear value and reduce loop statement */
	clog_ast_literal_free(parser,reduction->value);
	reduction->value = NULL;

	if (!clog_ast_statement_list_reduce(parser,&(*do_stmt)->stmt->stmt.do_stmt->loop_stmt,reduction))
		return 0;

	/* Check for constant false condition */
	if (!clog_ast_expression_eval(parser,&if_lit,(*do_stmt)->stmt->stmt.do_stmt->condition,NULL,NULL))
		return 0;
	if (if_lit)
	{
		if (!clog_ast_literal_bool_cast(if_lit))
		{
			/* Replace with loop */
			struct clog_ast_statement_list* l1;
			struct clog_ast_statement_list* l2 = (*do_stmt)->stmt->stmt.do_stmt->loop_stmt;

			l1 = (*do_stmt);
			*do_stmt = clog_ast_statement_list_append(parser,l2,(*do_stmt)->next);
			l1->next = NULL;
			clog_ast_statement_list_free(parser,l1);
			reduction->reduced = 1;
			clog_ast_literal_free(parser,if_lit);

			return 1;
		}

		clog_ast_literal_free(parser,if_lit);
	}

	return 1;
}

int clog_ast_statement_list_alloc_block(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* block)
{
	*list = NULL;

	if (!block)
		return 1;

	/* Check for blocks in block */
	clog_ast_statement_list_flatten(parser,&block);

	/* Check for block containing only 1 block */
	if (block->stmt->type == clog_ast_statement_block && block->next == NULL)
	{
		*list = block;
		return 1;
	}

	/* Check for duplicate declarations first */
	{
		struct clog_ast_statement_list* l;
		for (l = block;l;l = l->next)
		{
			if (l->stmt->type == clog_ast_statement_declaration)
			{
				struct clog_ast_statement_list* l2 = l->next;
				for (;l2;l2 = l2->next)
				{
					if (l2->stmt->type == clog_ast_statement_declaration)
					{
						if (clog_ast_literal_id_compare(l->stmt->stmt.declaration,l2->stmt->stmt.declaration) == 0)
						{
							clog_ast_statement_list_free(parser,block);
							return clog_syntax_error(parser,"Variable already declared",l2->stmt->stmt.declaration->line);
						}
					}
				}
			}
		}
	}

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_block))
	{
		clog_ast_statement_list_free(parser,block);
		return 0;
	}

	(*list)->stmt->stmt.block = block;

	printf("WAS:\n");
	__dump(1,*list);

	/* Now reduce the block */
	{
		struct clog_ast_reduction reduction = {0};
		do
		{
			reduction.reduced = 0;

			if (!clog_ast_statement_list_reduce(parser,list,&reduction))
			{
				clog_ast_statement_list_free(parser,*list);
				return 0;
			}

			printf("NOW:\n");
			__dump(1,*list);
		}
		while (*list && reduction.reduced);

		printf("DONE\n\n");
	}

	return 1;
}

struct clog_ast_statement_list* clog_ast_statement_list_append(struct clog_parser* parser, struct clog_ast_statement_list* list, struct clog_ast_statement_list* next)
{
	if (!list)
		list = next;
	else
	{
		struct clog_ast_statement_list** tail = &list->next;
		while (*tail)
			tail = &(*tail)->next;

		*tail = next;
	}

	return list;
}

int clog_ast_statement_list_alloc_declaration(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_token* id, struct clog_ast_expression* init)
{
	struct clog_token* id2 = NULL;
	struct clog_ast_expression* id_expr;
	struct clog_ast_expression* assign_expr;

	*list = NULL;

	if (!id)
	{
		clog_ast_expression_free(parser,init);
		return 0;
	}

	/* transform var x; => var x; x = null; */
	if (!init)
	{
		struct clog_ast_literal* lit_null;
		if (!clog_ast_literal_alloc(parser,&lit_null,NULL) ||
				!clog_ast_expression_alloc_literal(parser,&init,lit_null))
		{
			clog_token_free(parser,id);
			return 0;
		}
		lit_null->line = parser->line;
	}

	if (!clog_token_alloc(parser,&id2,id->value.string.str,id->value.string.len))
	{
		clog_token_free(parser,id);
		clog_ast_expression_free(parser,init);
		return 0;
	}

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_declaration))
	{
		clog_token_free(parser,id);
		clog_token_free(parser,id2);
		clog_ast_expression_free(parser,init);
		return 0;
	}

	if (!clog_ast_literal_alloc(parser,&(*list)->stmt->stmt.declaration,id))
	{
		clog_token_free(parser,id2);
		clog_ast_expression_free(parser,init);
		clog_free(*list);
		*list = NULL;
		return 0;
	}

	/* Transform var x = 3; => var x; x = 3; */
	if (!clog_ast_expression_alloc_id(parser,&id_expr,id2))
	{
		clog_ast_expression_free(parser,init);
		clog_ast_statement_list_free(parser,*list);
		*list = NULL;
		return 0;
	}

	if (!clog_ast_expression_alloc_assign(parser,&assign_expr,CLOG_TOKEN_ASSIGN,id_expr,init))
	{
		clog_ast_statement_list_free(parser,*list);
		*list = NULL;
		return 0;
	}

	if (!clog_ast_statement_list_alloc_expression(parser,&(*list)->next,assign_expr))
	{
		clog_ast_statement_list_free(parser,*list);
		*list = NULL;
		return 0;
	}

	return 1;
}

int clog_ast_statement_list_alloc_if(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond, struct clog_ast_statement_list* true_stmt, struct clog_ast_statement_list* false_stmt)
{
	*list = NULL;

	if (!cond)
	{
		clog_ast_statement_list_free(parser,true_stmt);
		clog_ast_statement_list_free(parser,false_stmt);
		return 0;
	}

	/* Check to see if we have an expression of the kind: if (var x = 12) */
	if (cond->stmt->type == clog_ast_statement_declaration)
	{
		struct clog_ast_expression* cond_expr;
		struct clog_ast_statement_list* cond2;

		/* We must check to see if an identical declaration occurs in the outermost block of true_stmt and false_stmt */
		if (true_stmt && true_stmt->stmt->type == clog_ast_statement_block)
		{
			struct clog_ast_statement_list* l = true_stmt->stmt->stmt.block;
			for (;l;l = l->next)
			{
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_id_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
				{
					unsigned long line = l->stmt->stmt.declaration->line;
					clog_ast_statement_list_free(parser,cond);
					clog_ast_statement_list_free(parser,true_stmt);
					clog_ast_statement_list_free(parser,false_stmt);
					return clog_syntax_error(parser,"Variable already declared",line);
				}
			}
		}
		if (false_stmt && false_stmt->stmt->type == clog_ast_statement_block)
		{
			struct clog_ast_statement_list* l = false_stmt->stmt->stmt.block;
			for (;l;l = l->next)
			{
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_id_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
				{
					unsigned long line = l->stmt->stmt.declaration->line;
					clog_ast_statement_list_free(parser,cond);
					clog_ast_statement_list_free(parser,true_stmt);
					clog_ast_statement_list_free(parser,false_stmt);
					return clog_syntax_error(parser,"Variable already declared",line);
				}
			}
		}

		/* Now rewrite if (var x = 1) ... => { var x; if (x = 1) ... } */
		if (!clog_ast_expression_clone(parser,&cond_expr,cond->next->stmt->stmt.expression) ||
				!clog_ast_statement_list_alloc_expression(parser,&cond2,cond_expr) ||
				!clog_ast_statement_list_alloc_if(parser,&cond->next->next,cond2,true_stmt,false_stmt))
		{
			clog_ast_statement_list_free(parser,cond);
			return 0;
		}

		return clog_ast_statement_list_alloc_block(parser,list,cond);
	}

	/* Rewrite to force compound statements: if (x) var i;  =>  if (x) { var i; } */
	if (true_stmt && true_stmt->stmt->type == clog_ast_statement_declaration)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&true_stmt,true_stmt))
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,false_stmt);
			return 0;
		}
	}
	if (false_stmt && false_stmt->stmt->type == clog_ast_statement_declaration)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&false_stmt,false_stmt))
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,true_stmt);
			return 0;
		}
	}

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_if))
	{
		clog_ast_statement_list_free(parser,cond);
		clog_ast_statement_list_free(parser,true_stmt);
		clog_ast_statement_list_free(parser,false_stmt);
		return 0;
	}

	(*list)->stmt->stmt.if_stmt = clog_malloc(sizeof(struct clog_ast_statement_if));
	if (!(*list)->stmt->stmt.if_stmt)
	{
		clog_ast_statement_list_free(parser,cond);
		clog_ast_statement_list_free(parser,true_stmt);
		clog_ast_statement_list_free(parser,false_stmt);
		clog_free((*list)->stmt);
		clog_free(*list);
		*list = NULL;
		return clog_out_of_memory(parser);
	}

	(*list)->stmt->stmt.if_stmt->condition = cond->stmt->stmt.expression;
	cond->stmt->stmt.expression = NULL;
	clog_ast_statement_list_free(parser,cond);

	(*list)->stmt->stmt.if_stmt->true_stmt = true_stmt;
	(*list)->stmt->stmt.if_stmt->false_stmt = false_stmt;

	return 1;
}

int clog_ast_statement_list_alloc_do(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* cond, struct clog_ast_statement_list* loop_stmt)
{
	*list = NULL;

	if (!cond)
	{
		clog_ast_statement_list_free(parser,loop_stmt);
		return 0;
	}

	/* Rewrite to force compound statements: do var i while(1)  =>  do { var i; } while(1) */
	if (loop_stmt && loop_stmt->stmt->type == clog_ast_statement_declaration)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&loop_stmt,loop_stmt))
		{
			clog_ast_expression_free(parser,cond);
			return 0;
		}
	}

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_do))
	{
		clog_ast_expression_free(parser,cond);
		clog_ast_statement_list_free(parser,loop_stmt);
		return 0;
	}

	(*list)->stmt->stmt.do_stmt = clog_malloc(sizeof(struct clog_ast_statement_do));
	if (!(*list)->stmt->stmt.do_stmt)
	{
		clog_ast_expression_free(parser,cond);
		clog_ast_statement_list_free(parser,loop_stmt);
		clog_free((*list)->stmt);
		clog_free(*list);
		*list = NULL;
		return clog_out_of_memory(parser);
	}

	(*list)->stmt->stmt.do_stmt->condition = cond;
	(*list)->stmt->stmt.do_stmt->loop_stmt = loop_stmt;

	return 1;
}

int clog_ast_statement_list_alloc_while(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond_stmt, struct clog_ast_statement_list* loop_stmt)
{
	struct clog_ast_expression* cond_expr = NULL;

	*list = NULL;
	if (!cond_stmt)
	{
		clog_ast_statement_list_free(parser,loop_stmt);
		return 0;
	}

	/* Translate while (E) {S} => if (E) do {S} while (E) */

	/* Clone the condition expression */
	if (cond_stmt->stmt->type == clog_ast_statement_declaration)
	{
		if (!clog_ast_expression_clone(parser,&cond_expr,cond_stmt->next->stmt->stmt.expression))
		{
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 0;
		}
	}
	else
	{
		if (!clog_ast_expression_clone(parser,&cond_expr,cond_stmt->stmt->stmt.expression))
		{
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 0;
		}
	}

	/* Create the do loop */
	if (!clog_ast_statement_list_alloc_do(parser,list,cond_expr,loop_stmt))
	{
		clog_ast_statement_list_free(parser,cond_stmt);
		return 0;
	}

	/* Create the if */
	return clog_ast_statement_list_alloc_if(parser,list,cond_stmt,*list,NULL);
}

int clog_ast_statement_list_alloc_for(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* init_stmt, struct clog_ast_statement_list* cond_stmt, struct clog_ast_expression* iter_expr, struct clog_ast_statement_list* loop_stmt)
{
	/* Translate for (A;B;C) {D}  =>  { A; while (B) { D; C; } } */
	*list = NULL;

	if (!cond_stmt)
	{
		/* Empty condition => true */
		struct clog_ast_literal* lit_true;
		struct clog_ast_expression* expr_true;
		if (!clog_ast_literal_alloc_bool(parser,&lit_true,1) ||
				!clog_ast_expression_alloc_literal(parser,&expr_true,lit_true) ||
				!clog_ast_statement_list_alloc_expression(parser,&cond_stmt,expr_true))
		{
			clog_ast_statement_list_free(parser,init_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 0;
		}
		lit_true->line = parser->line;
	}

	if (iter_expr)
	{
		/* Append iter_expr to loop_stmt */
		struct clog_ast_statement_list* iter_stmt = NULL;
		if (!clog_ast_statement_list_alloc_expression(parser,&iter_stmt,iter_expr))
		{
			clog_ast_statement_list_free(parser,init_stmt);
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 0;
		}

		loop_stmt = clog_ast_statement_list_append(parser,loop_stmt,iter_stmt);
	}

	/* Create the while loop */
	if (!clog_ast_statement_list_alloc_while(parser,list,cond_stmt,loop_stmt))
	{
		clog_ast_statement_list_free(parser,init_stmt);
		return 0;
	}

	if (init_stmt)
	{
		/* Add init_expression at the same scope as any declarations from cond_stmt */
		if (*list && (*list)->stmt->type == clog_ast_statement_block)
			(*list)->stmt->stmt.block = clog_ast_statement_list_append(parser,init_stmt,(*list)->stmt->stmt.block);
		else if (!clog_ast_statement_list_alloc_block(parser,list,clog_ast_statement_list_append(parser,init_stmt,*list)))
			return 0;
	}

	return 1;
}

int clog_ast_statement_list_alloc_return(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr)
{
	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_return))
	{
		clog_ast_expression_free(parser,expr);
		return 0;
	}

	(*list)->stmt->stmt.expression = expr;

	return 1;
}












static void __dump_indent(size_t indent)
{
	size_t t = 0;
	for (;t < indent;++t)
		printf("    ");
}

static void __dump_literal(const struct clog_ast_literal* lit)
{
	switch (lit->type)
	{
	case clog_ast_literal_string:
		printf("\"%.*s\"",(int)lit->value.string.len,lit->value.string.str);
		break;

	case clog_ast_literal_integer:
		printf("%lu",lit->value.integer);
		break;

	case clog_ast_literal_real:
		printf("%g",lit->value.real);
		break;

	case clog_ast_literal_bool:
		printf("%s",lit->value.integer ? "true" : "false");
		break;

	case clog_ast_literal_null:
		printf("null");
		break;
	}
}

static void __dump_expr(const struct clog_ast_expression* expr);

static void __dump_expr_list(const struct clog_ast_expression_list* list)
{
	for (;list;list = list->next)
	{
		__dump_expr(list->expr);
		if (list->next)
			printf(",");
	}
}

static void __dump_expr_b(const struct clog_ast_expression* expr)
{
	if (expr->type == clog_ast_expression_builtin)
		printf("(");
	__dump_expr(expr);
	if (expr->type == clog_ast_expression_builtin)
		printf(")");
}

static void __dump_expr(const struct clog_ast_expression* expr)
{
	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		printf("%.*s",(int)expr->expr.identifier->value.string.len,expr->expr.identifier->value.string.str);
		break;

	case clog_ast_expression_literal:
		__dump_literal(expr->expr.literal);
		break;

	case clog_ast_expression_builtin:
		if (expr->expr.builtin->args[1])
			__dump_expr(expr->expr.builtin->args[0]);

		switch (expr->expr.builtin->type)
		{
		case CLOG_TOKEN_DOUBLE_PLUS:
			if (expr->lvalue)
				printf("++");
			__dump_expr_b(expr->expr.builtin->args[0]);
			if (!expr->lvalue)
				printf("++");
			break;

		case CLOG_TOKEN_DOUBLE_MINUS:
			if (expr->lvalue)
				printf("++");
			__dump_expr_b(expr->expr.builtin->args[0]);
			if (!expr->lvalue)
				printf("++");
			break;

		case CLOG_TOKEN_QUESTION:
			printf(" ? ");
			__dump_expr_b(expr->expr.builtin->args[1]);
			printf(" : ");
			__dump_expr_b(expr->expr.builtin->args[2]);
			break;

		case CLOG_TOKEN_COMMA:
			printf(", ");
			__dump_expr_b(expr->expr.builtin->args[1]);
			break;

		default:
			if (expr->expr.builtin->args[1])
				printf(" ");
			clog_token_printf(expr->expr.builtin->type,NULL);
			if (expr->expr.builtin->args[1])
			{
				printf(" ");
				__dump_expr_b(expr->expr.builtin->args[1]);
			}
			else
				__dump_expr(expr->expr.builtin->args[0]);
			break;
		}
		break;

	case clog_ast_expression_call:
		__dump_expr(expr->expr.call->expr);
		printf("(");
		__dump_expr_list(expr->expr.call->params);
		printf(")");
		break;
	}

	/*printf("[");
	if (expr->constant)
		printf("C");
	if (expr->lvalue)
		printf("L");
	printf("]");*/
}

static void __dump(size_t indent, const struct clog_ast_statement_list* list)
{
	for (;list;list = list->next)
	{
		__dump_indent(indent);

		switch (list->stmt->type)
		{
		case clog_ast_statement_block:
			printf("{\n");
			__dump(indent+1,list->stmt->stmt.block);
			__dump_indent(indent);
			printf("}\n");
			break;

		case clog_ast_statement_declaration:
			printf("var %.*s;\n",(int)list->stmt->stmt.declaration->value.string.len,list->stmt->stmt.declaration->value.string.str);
			break;

		case clog_ast_statement_expression:
			__dump_expr(list->stmt->stmt.expression);
			printf(";\n");
			break;

		case clog_ast_statement_return:
			printf("return ");
			__dump_expr(list->stmt->stmt.expression);
			printf(";\n");
			break;

		case clog_ast_statement_break:
			printf("break;\n");
			break;

		case clog_ast_statement_continue:
			printf("continue;\n");
			break;

		case clog_ast_statement_if:
			printf("if (");
			__dump_expr(list->stmt->stmt.if_stmt->condition);
			printf(")\n");
			if (list->stmt->stmt.if_stmt->true_stmt)
			{
				if (list->stmt->stmt.if_stmt->true_stmt->stmt->type == clog_ast_statement_block)
					__dump(indent,list->stmt->stmt.if_stmt->true_stmt);
				else
					__dump(indent+1,list->stmt->stmt.if_stmt->true_stmt);
			}
			else
			{
				__dump_indent(indent+1);
				printf(";\n");
			}
			if (list->stmt->stmt.if_stmt->false_stmt)
			{
				__dump_indent(indent);
				printf("else\n");
				if (list->stmt->stmt.if_stmt->false_stmt->stmt->type == clog_ast_statement_block)
					__dump(indent,list->stmt->stmt.if_stmt->false_stmt);
				else
					__dump(indent+1,list->stmt->stmt.if_stmt->false_stmt);
			}
			break;

		case clog_ast_statement_do:
			printf("do\n");
			if (list->stmt->stmt.do_stmt->loop_stmt)
				__dump(indent+1,list->stmt->stmt.do_stmt->loop_stmt);
			else
			{
				__dump_indent(indent+1);
				printf(";\n");
			}
			__dump_indent(indent);
			printf("  while (");
			__dump_expr(list->stmt->stmt.do_stmt->condition);
			printf(")\n");
			break;
		}
	}
}










/* External functions defined by ragel and lemon */
int clog_tokenize(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param, struct clog_parser* parser, void* lemon);
void* clog_parserAlloc(void* (*)(size_t));
void clog_parserFree(void *p, void (*)(void*));
void clog_parser(void* lemon, int type, struct clog_token* tok, struct clog_parser* parser);

int clog_parse(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param)
{
	int retval = 0;
	struct clog_parser parser = {0,1};

	void* lemon = clog_parserAlloc(&clog_malloc);
	if (!lemon)
		return -1;

	retval = clog_tokenize(rd_fn,rd_param,&parser,lemon);

	clog_parser(lemon,0,NULL,&parser);

	clog_parserFree(lemon,&clog_free);

	if (retval && !parser.failed)
		printf("Success!\n");

	__dump(0,parser.pgm);

	clog_ast_statement_list_free(&parser,parser.pgm);

	return retval;
}
