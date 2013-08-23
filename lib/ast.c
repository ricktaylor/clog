/*
 * tokens.c
 *
 *  Created on: 9 Aug 2013
 *      Author: taylorr
 */

#include "ast.h"
#include "parser.h"

#include <string.h>
#include <stdio.h>

int clog_out_of_memory(struct clog_parser* parser)
{
	if (!parser->failed)
		printf("Out of memory building AST\n");

	parser->failed = 1;
	return 0;
}

int clog_syntax_error(struct clog_parser* parser, const char* msg, unsigned long line)
{
	if (!parser->failed)
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

void clog_ast_literal_free(struct clog_parser* parser, struct clog_ast_literal* lit)
{
	if (lit)
	{
		if (lit->type == clog_ast_literal_string && lit->value.string.str)
			clog_free(lit->value.string.str);

		clog_free(lit);
	}
}

static int clog_ast_literal_clone(struct clog_parser* parser, struct clog_ast_literal** new, struct clog_ast_literal* lit)
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
	if (!lit1 || !lit2)
		return 0;

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

	if (!p1->lvalue)
	{
		unsigned long line = clog_ast_expression_line(p1);
		clog_ast_expression_free(parser,p1);
		clog_token_free(parser,token);
		return clog_syntax_error(parser,". requires an lvalue",line);
	}

	if (!clog_ast_expression_alloc_id(parser,&p2,token))
	{
		clog_ast_expression_free(parser,p1);
		return 0;
	}

	if (!clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_DOT,p1,p2))
		return 0;

	(*expr)->lvalue = 1;
	(*expr)->constant = 0;

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

	switch (type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
		if (!p1->lvalue)
		{
			unsigned long line = clog_ast_expression_line(p1);
			clog_ast_expression_free(parser,p1);
			return clog_syntax_error(parser,type == CLOG_TOKEN_DOUBLE_PLUS ? "++ requires an lvalue" :  "++ requires an lvalue",line);
		}
		break;

	case CLOG_TOKEN_PLUS:
		if (p1->type == clog_ast_expression_literal)
		{
			/* Unary + */
			if (p1->expr.literal->type != clog_ast_literal_integer &&
				p1->expr.literal->type != clog_ast_literal_real)
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"+ requires a number",line);
			}
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1->type == clog_ast_expression_literal)
		{
			/* Unary - */
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_integer:
				p1->expr.literal->value.integer = -p1->expr.literal->value.integer;
				break;

			case clog_ast_literal_real:
				p1->expr.literal->value.real = -p1->expr.literal->value.real;
				break;

			default:
				{
					unsigned long line = clog_ast_expression_line(p1);
					clog_ast_expression_free(parser,p1);
					return clog_syntax_error(parser,"- requires a number",line);
				}
			}
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_EXCLAMATION:
		/* Logical NOT */
		if (p1->type == clog_ast_expression_literal)
		{
			clog_ast_literal_bool_promote(p1->expr.literal);
			p1->expr.literal->value.integer = (p1->expr.literal->value.integer == 0 ? 1 : 0);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_TILDA:
		/* Bitwise 1s complement */
		if (p1->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_int_promote(p1->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"~ requires integer",line);
			}

			p1->expr.literal->value.integer = ~p1->expr.literal->value.integer;
			p1->expr.literal->type = clog_ast_literal_integer;
			*expr = p1;
			return 1;
		}
		break;

	default:
		break;
	}

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

	switch (type)
	{
	case CLOG_TOKEN_OPEN_BRACKET:
		if (!p1->lvalue)
		{
			unsigned long line = clog_ast_expression_line(p1);
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error(parser,"Subscript requires an lvalue",line);
		}
		break;

	case CLOG_TOKEN_PLUS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (p1->expr.literal->type == clog_ast_literal_string && p2->expr.literal->type == clog_ast_literal_string)
			{
				if (!p1->expr.literal->value.string.len)
				{
					clog_ast_expression_free(parser,p1);
					*expr = p2;
					return 1;
				}

				if (p2->expr.literal->value.string.len)
				{
					unsigned char* sz = clog_realloc(p1->expr.literal->value.string.str,p1->expr.literal->value.string.len + p2->expr.literal->value.string.len);
					if (!sz)
					{
						clog_ast_expression_free(parser,p1);
						clog_ast_expression_free(parser,p2);
						return clog_out_of_memory(parser);
					}

					memcpy(sz+p1->expr.literal->value.string.len,p2->expr.literal->value.string.str,p2->expr.literal->value.string.len);
					p1->expr.literal->value.string.str = sz;
					p1->expr.literal->value.string.len += p2->expr.literal->value.string.len;
				}

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}

			if (p1->expr.literal->type != clog_ast_literal_string && p2->expr.literal->type != clog_ast_literal_string)
			{
				if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
				{
					unsigned long line = clog_ast_expression_line(p1);
					clog_ast_expression_free(parser,p1);
					clog_ast_expression_free(parser,p2);
					return clog_syntax_error(parser,"+ requires  numbers or strings",line);
				}

				if (p1->expr.literal->type == clog_ast_literal_real)
					p1->expr.literal->value.real += p2->expr.literal->value.real;
				else
					p1->expr.literal->value.integer += p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"- requires numbers",line);
			}

			if (p1->expr.literal->type == clog_ast_literal_real)
				p1->expr.literal->value.real -= p2->expr.literal->value.real;
			else
				p1->expr.literal->value.integer -= p2->expr.literal->value.integer;

			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_STAR:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"* requires numbers",line);
			}

			if (p1->expr.literal->type == clog_ast_literal_real)
				p1->expr.literal->value.real *= p2->expr.literal->value.real;
			else
				p1->expr.literal->value.integer *= p2->expr.literal->value.integer;

			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_SLASH:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"/ requires numbers",line);
			}

			if (p1->expr.literal->type == clog_ast_literal_real)
			{
				if (p2->expr.literal->value.real == 0.0)
				{
					unsigned long line = clog_ast_expression_line(p2);
					clog_ast_expression_free(parser,p1);
					clog_ast_expression_free(parser,p2);
					return clog_syntax_error(parser,"Division by 0.0",line);
				}
				p1->expr.literal->value.real /= p2->expr.literal->value.real;
			}
			else
			{
				if (p2->expr.literal->value.integer == 0)
				{
					unsigned long line = clog_ast_expression_line(p2);
					clog_ast_expression_free(parser,p1);
					clog_ast_expression_free(parser,p2);
					return clog_syntax_error(parser,"Division by 0",line);
				}
				p1->expr.literal->value.integer /= p2->expr.literal->value.integer;
			}
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_PERCENT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_int_promote(p1->expr.literal) ||
					!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"% requires integers",line);
			}

			p1->expr.literal->value.integer %= p2->expr.literal->value.integer;
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_RIGHT_SHIFT:
	case CLOG_TOKEN_LEFT_SHIFT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			unsigned long line = clog_ast_expression_line(p1);
			if (clog_ast_literal_int_promote(p1->expr.literal) && clog_ast_literal_int_promote(p2->expr.literal))
			{
				if (type == CLOG_TOKEN_RIGHT_SHIFT)
					p1->expr.literal->value.integer >>= p2->expr.literal->value.integer;
				else
					p1->expr.literal->value.integer <<= p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error(parser,type == CLOG_TOKEN_RIGHT_SHIFT ? ">> requires integers" : "<< requires integers",line);
		}
		break;

	case CLOG_TOKEN_LESS_THAN:
	case CLOG_TOKEN_GREATER_THAN:
	case CLOG_TOKEN_LESS_THAN_EQUALS:
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
	case CLOG_TOKEN_EQUALS:
	case CLOG_TOKEN_NOT_EQUALS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			unsigned long line = clog_ast_expression_line(p1);
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
			}

			switch (type)
			{
			case CLOG_TOKEN_LESS_THAN:
				if (b == -2)
					return clog_syntax_error(parser,"< requires numbers or strings",line);
				p1->expr.literal->value.integer = (b < 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN:
				if (b == -2)
					return clog_syntax_error(parser,"> requires numbers or strings",line);
				p1->expr.literal->value.integer = (b > 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_LESS_THAN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"<= requires numbers or strings",line);
				p1->expr.literal->value.integer = (b <= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,">= requires numbers or strings",line);
				p1->expr.literal->value.integer = (b >= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"== requires numbers or strings",line);
				p1->expr.literal->value.integer = (b == 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_NOT_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"!= requires numbers or strings",line);
				p1->expr.literal->value.integer = (b != 0 ? 1 : 0);
				break;
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;
		}
		break;

	case CLOG_TOKEN_COMMA:
		if (p1->constant)
		{
			clog_ast_expression_free(parser,p1);
			*expr = p2;
			return 1;
		}
		break;

	case CLOG_TOKEN_AND:
		if (p1->type == clog_ast_expression_literal)
		{
			clog_ast_literal_bool_promote(p1->expr.literal);
			if (p1->expr.literal->value.integer)
			{
				if (p2->type == clog_ast_expression_literal)
					clog_ast_literal_bool_promote(p2->expr.literal);

				clog_ast_expression_free(parser,p1);
				*expr = p2;
				return 1;
			}
			else
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
		}
		break;

	case CLOG_TOKEN_OR:
		if (p1->type == clog_ast_expression_literal)
		{
			clog_ast_literal_bool_promote(p1->expr.literal);
			if (!p1->expr.literal->value.integer)
			{
				if (p2->type == clog_ast_expression_literal)
					clog_ast_literal_bool_promote(p2->expr.literal);

				clog_ast_expression_free(parser,p1);
				*expr = p2;
				return 1;
			}
			else
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
		}
		break;

	case CLOG_TOKEN_BAR:
	case CLOG_TOKEN_CARET:
	case CLOG_TOKEN_AMPERSAND:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			unsigned long line = clog_ast_expression_line(p1);
			if (clog_ast_literal_int_promote(p1->expr.literal) && clog_ast_literal_int_promote(p2->expr.literal))
			{
				if (type == CLOG_TOKEN_BAR)
					p1->expr.literal->value.integer |= p2->expr.literal->value.integer;
				else if (type == CLOG_TOKEN_CARET)
					p1->expr.literal->value.integer ^= p2->expr.literal->value.integer;
				else
					p1->expr.literal->value.integer &= p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			if (type == CLOG_TOKEN_BAR)
				return clog_syntax_error(parser,"| requires integers",line);
			else if (type == CLOG_TOKEN_CARET)
				return clog_syntax_error(parser,"^ requires integers",line);
			else
				return clog_syntax_error(parser,"& requires integers",line);
		}
		break;

	default:
		break;
	}

	return clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL);
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

		if (p1->type == clog_ast_expression_literal)
		{
			if (clog_ast_literal_bool_cast(p1->expr.literal))
			{
				clog_ast_expression_free(parser,p3);
				*expr = p2;
			}
			else
			{
				clog_ast_expression_free(parser,p2);
				*expr = p3;
			}

			clog_ast_expression_free(parser,p1);
			return 1;
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

	(*expr)->constant = (p1->constant && p1->type != clog_ast_expression_identifier &&
			(p2 ? p2->constant && p2->type != clog_ast_expression_identifier : 1) &&
			(p3 ? p3->constant && p3->type != clog_ast_expression_identifier : 1) ? 1 : 0);

	return 1;
}

int clog_ast_expression_alloc_assign(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	*expr = NULL;

	if (!p1 || !p2)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return 0;
	}

	if (!p1->lvalue)
	{
		unsigned long line = clog_ast_expression_line(p1);
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return clog_syntax_error(parser,"Assignment requires an lvalue",line);
	}

	if (p2->type == clog_ast_expression_literal)
	{
		switch (type)
		{
		case CLOG_TOKEN_STAR_ASSIGN:
			if (p2->expr.literal->type != clog_ast_literal_real && !clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"*= requires a number",line);
			}
			break;

		case CLOG_TOKEN_SLASH_ASSIGN:
			if (p2->expr.literal->type == clog_ast_literal_real && p2->expr.literal->value.real == 0.0)
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"Division by 0.0",line);
			}
			else if (!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"/= requires a number",line);
			}
			else if (p2->expr.literal->value.integer == 0)
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"Division by 0",line);
			}
			break;

		case CLOG_TOKEN_PERCENT_ASSIGN:
			if (!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"%= requires an integer",line);
			}
			break;

		case CLOG_TOKEN_PLUS_ASSIGN:
			if (p2->expr.literal->type != clog_ast_literal_string &&
					p2->expr.literal->type != clog_ast_literal_real &&
					!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"+= requires a number",line);
			}
			break;

		case CLOG_TOKEN_MINUS_ASSIGN:
			if (p2->expr.literal->type != clog_ast_literal_real && !clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"-= requires a number",line);
			}
			break;

		case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
		case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
			if (!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,type == CLOG_TOKEN_RIGHT_SHIFT_ASSIGN ? ">>= requires an integer" : "<<= requires an integer",line);
			}
			break;

		case CLOG_TOKEN_AMPERSAND_ASSIGN:
		case CLOG_TOKEN_BITWISE_CARET_ASSIGN:
		case CLOG_TOKEN_BAR_ASSIGN:
			if (!clog_ast_literal_int_promote(p2->expr.literal))
			{
				unsigned long line = clog_ast_expression_line(p2);
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				if (type == CLOG_TOKEN_BAR_ASSIGN)
					return clog_syntax_error(parser,"|= requires an integer",line);
				else if (type == CLOG_TOKEN_BITWISE_CARET_ASSIGN)
					return clog_syntax_error(parser,"^= requires an integer",line);
				else
					return clog_syntax_error(parser,"&= requires an integer",line);
			}
			break;

		case CLOG_TOKEN_ASSIGN:
		case CLOG_TOKEN_COLON_ASSIGN:
		default:
			break;
		}
	}

	if (!clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL))
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

	if (!call->lvalue)
	{
		unsigned long line = clog_ast_expression_line(call);
		clog_ast_expression_free(parser,call);
		clog_ast_expression_list_free(parser,list);
		return clog_syntax_error(parser,"Function call requires an lvalue",line);
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

static int clog_ast_statement_list_is_const(const struct clog_ast_statement_list* list)
{
	for (;list;list = list->next)
	{
		if (list->stmt->type == clog_ast_statement_expression)
		{
			if (!list->stmt->stmt.expression->constant)
				break;
		}
		else if (list->stmt->type != clog_ast_statement_declaration)
			break;
	}

	return (!list);
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

int clog_ast_statement_list_alloc_expression(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr, int remove_const)
{
	*list = NULL;

	if (!expr)
		return 0;

	if (remove_const && expr->constant)
	{
		clog_ast_expression_free(parser,expr);
		return 1;
	}

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

int clog_ast_statement_list_alloc_block(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* block)
{
	*list = NULL;

	if (!block)
		return 1;

	/* Check for block containing only 1 block */
	if (block->stmt->type == clog_ast_statement_block && block->next == NULL)
	{
		*list = block;
		return 1;
	}

	/* Check for blocks containing nothing but constant expressions */
	if (clog_ast_statement_list_is_const(block))
	{
		clog_ast_statement_list_free(parser,block);
		return 1;
	}

	/* Check for blocks in block */
	clog_ast_statement_list_flatten(parser,&block);

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_block))
	{
		clog_ast_statement_list_free(parser,block);
		return 0;
	}

	(*list)->stmt->stmt.block = block;

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

	*list = NULL;

	if (!id)
	{
		clog_ast_expression_free(parser,init);
		return 0;
	}

	if (init)
	{
		if (!clog_token_alloc(parser,&id2,id->value.string.str,id->value.string.len))
		{
			clog_token_free(parser,id);
			clog_ast_expression_free(parser,init);
			return 0;
		}
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

	if (init)
	{
		/* Transform var x = 3; => var x; x = 3; */
		struct clog_ast_expression* id_expr;
		struct clog_ast_expression* assign_expr;

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

		/* There are no side-effects on a local variable declaration */
		assign_expr->constant = init->constant;

		if (!clog_ast_statement_list_alloc_expression(parser,&(*list)->next,assign_expr,0))
		{
			clog_ast_statement_list_free(parser,*list);
			*list = NULL;
			return 0;
		}
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
		/* We must check to see if an identical declaration occurs in the outermost block of true_stmt and false_stmt */
		if (true_stmt && true_stmt->stmt->type == clog_ast_statement_block)
		{
			struct clog_ast_statement_list* l = true_stmt->stmt->stmt.block;
			for (;l;l = l->next)
			{
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
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
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
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
		if (!clog_ast_statement_list_alloc_if(parser,&cond->next,cond->next,true_stmt,false_stmt))
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

	/* Eliminate constant expressions in the clauses */
	if (true_stmt && clog_ast_statement_list_is_const(true_stmt))
	{
		clog_ast_statement_list_free(parser,true_stmt);
		true_stmt = NULL;
	}
	if (false_stmt && clog_ast_statement_list_is_const(false_stmt))
	{
		clog_ast_statement_list_free(parser,false_stmt);
		false_stmt = NULL;
	}

	/* If we have no result statements, see if the comparison is constant */
	if (!true_stmt && !false_stmt)
	{
		if (clog_ast_statement_list_is_const(cond))
		{
			/* We can eliminate this if */
			clog_ast_statement_list_free(parser,cond);
		}
		else
		{
			/* Otherwise replace with a statement */
			*list = cond;
		}
		return 1;
	}

	/* Check for literal condition */
	if (cond->stmt->stmt.expression->type == clog_ast_expression_literal)
	{
		if (clog_ast_literal_bool_cast(cond->stmt->stmt.expression->expr.literal))
		{
			*list = true_stmt;
			clog_ast_statement_list_free(parser,false_stmt);
		}
		else
		{
			*list = false_stmt;
			clog_ast_statement_list_free(parser,true_stmt);
		}

		clog_ast_statement_list_free(parser,cond);
		return 1;
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

	clog_ast_statement_list_flatten(parser,&true_stmt);
	clog_ast_statement_list_flatten(parser,&false_stmt);

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
	if (loop_stmt->stmt->type == clog_ast_statement_declaration)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&loop_stmt,loop_stmt))
		{
			clog_ast_expression_free(parser,cond);
			return 0;
		}
	}

	/* Eliminate constant expressions in the loop */
	if (loop_stmt && clog_ast_statement_list_is_const(loop_stmt))
	{
		clog_ast_statement_list_free(parser,loop_stmt);
		loop_stmt = NULL;
	}

	/* If we have no loop statements, see if the comparison is constant */
	if (!loop_stmt)
	{
		/* Replace with a statement */
		return clog_ast_statement_list_alloc_expression(parser,list,cond,1);
	}

	/* Check for literal condition */
	if (cond->type == clog_ast_expression_literal)
	{
		/* Force to boolean */
		clog_ast_literal_bool_promote(cond->expr.literal);

		/* And discard if false */
		if (!cond->expr.literal->value.integer)
		{
			clog_ast_expression_free(parser,cond);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 1;
		}
	}

	clog_ast_statement_list_flatten(parser,&loop_stmt);

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
				!clog_ast_statement_list_alloc_expression(parser,&cond_stmt,expr_true,0))
		{
			clog_ast_statement_list_free(parser,init_stmt);
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			return 0;
		}
	}

	if (iter_expr)
	{
		/* Append iter_expr to loop_stmt */
		struct clog_ast_statement_list* iter_stmt = NULL;
		if (!clog_ast_statement_list_alloc_expression(parser,&iter_stmt,iter_expr,1))
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












static void clog_ast_indent(size_t indent)
{
	size_t t = 0;
	for (;t < indent;++t)
		printf("    ");
}

static void clog_ast_dump_literal(const struct clog_ast_literal* lit)
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

static void clog_ast_dump_expr(const struct clog_ast_expression* expr);

static void clog_ast_dump_expr_list(const struct clog_ast_expression_list* list)
{
	for (;list;list = list->next)
	{
		clog_ast_dump_expr(list->expr);
		if (list->next)
			printf(",");
	}
}

static void clog_ast_dump_expr_b(const struct clog_ast_expression* expr)
{
	if (expr->type == clog_ast_expression_builtin)
		printf("(");
	clog_ast_dump_expr(expr);
	if (expr->type == clog_ast_expression_builtin)
		printf(")");
}

static void clog_ast_dump_expr(const struct clog_ast_expression* expr)
{
	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		printf("%.*s",(int)expr->expr.identifier->value.string.len,expr->expr.identifier->value.string.str);
		break;

	case clog_ast_expression_literal:
		clog_ast_dump_literal(expr->expr.literal);
		break;

	case clog_ast_expression_builtin:
		if (expr->expr.builtin->args[1])
			clog_ast_dump_expr(expr->expr.builtin->args[0]);

		switch (expr->expr.builtin->type)
		{
		case CLOG_TOKEN_ASSIGN:
			printf(" = ");
			clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_EQUALS:
			printf(" == ");
			clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_LESS_THAN:
			printf(" < ");
			clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_DOT:
			printf(".");
			clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_QUESTION:
			printf(" ? ");
			clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			printf(" : ");
			clog_ast_dump_expr_b(expr->expr.builtin->args[2]);
			break;

		default:
			if (!expr->expr.builtin->args[1])
				clog_ast_dump_expr_b(expr->expr.builtin->args[0]);
			printf(" OP ");
			if (expr->expr.builtin->args[1])
				clog_ast_dump_expr_b(expr->expr.builtin->args[1]);
			break;
		}
		break;

	case clog_ast_expression_call:
		clog_ast_dump_expr(expr->expr.call->expr);
		printf("(");
		clog_ast_dump_expr_list(expr->expr.call->params);
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

static void clog_ast_dump(size_t indent, const struct clog_ast_statement_list* list)
{
	for (;list;list = list->next)
	{
		clog_ast_indent(indent);

		switch (list->stmt->type)
		{
		case clog_ast_statement_block:
			printf("{\n");
			clog_ast_dump(indent+1,list->stmt->stmt.block);
			clog_ast_indent(indent);
			printf("}\n");
			break;

		case clog_ast_statement_declaration:
			printf("var %.*s;\n",(int)list->stmt->stmt.declaration->value.string.len,list->stmt->stmt.declaration->value.string.str);
			break;

		case clog_ast_statement_expression:
			clog_ast_dump_expr(list->stmt->stmt.expression);
			printf(";\n");
			break;

		case clog_ast_statement_return:
			printf("return ");
			clog_ast_dump_expr(list->stmt->stmt.expression);
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
			clog_ast_dump_expr(list->stmt->stmt.if_stmt->condition);
			printf(")\n");
			if (list->stmt->stmt.if_stmt->true_stmt)
			{
				if (list->stmt->stmt.if_stmt->true_stmt->stmt->type == clog_ast_statement_block)
					clog_ast_dump(indent,list->stmt->stmt.if_stmt->true_stmt);
				else
					clog_ast_dump(indent+1,list->stmt->stmt.if_stmt->true_stmt);
			}
			else
			{
				clog_ast_indent(indent+1);
				printf(";\n");
			}
			if (list->stmt->stmt.if_stmt->false_stmt)
			{
				clog_ast_indent(indent);
				printf("else\n");
				if (list->stmt->stmt.if_stmt->false_stmt->stmt->type == clog_ast_statement_block)
					clog_ast_dump(indent,list->stmt->stmt.if_stmt->false_stmt);
				else
					clog_ast_dump(indent+1,list->stmt->stmt.if_stmt->false_stmt);
			}
			break;

		case clog_ast_statement_do:
			printf("do\n");
			if (list->stmt->stmt.do_stmt->loop_stmt)
			{
				if (list->stmt->stmt.do_stmt->loop_stmt->stmt->type == clog_ast_statement_block)
					clog_ast_dump(indent,list->stmt->stmt.do_stmt->loop_stmt);
				else
					clog_ast_dump(indent+1,list->stmt->stmt.do_stmt->loop_stmt);
			}
			else
			{
				clog_ast_indent(indent+1);
				printf(";\n");
			}
			clog_ast_indent(indent);
			printf("  while (");
			clog_ast_dump_expr(list->stmt->stmt.do_stmt->condition);
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

	if (!retval && !parser.failed)
		printf("Success!\n");

	clog_ast_dump(0,parser.pgm);

	clog_ast_statement_list_free(&parser,parser.pgm);

	return retval;
}
