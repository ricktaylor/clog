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

static void out_of_memory(struct clog_parser* parser)
{
	printf("Out of memory building AST\n");
	parser->failed = 1;
}

static void syntax_error(struct clog_parser* parser)
{
	parser->failed = 1;
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

void clog_ast_literal_free(struct clog_parser* parser, struct clog_ast_literal* lit)
{
	if (lit)
	{
		if (lit->type == clog_ast_literal_string && lit->value.string.str)
			clog_free(lit->value.string.str);

		clog_free(lit);
	}
}

void clog_ast_literal_alloc(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_token* token)
{
	*lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*lit)
		out_of_memory(parser);
	else
	{
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

			/* Don't free string data */
			token->value.string.str = NULL;
		}
	}

	clog_token_free(parser,token);
}

void clog_ast_literal_alloc_bool(struct clog_parser* parser, struct clog_ast_literal** lit, int value)
{
	*lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*lit)
		return out_of_memory(parser);

	(*lit)->type = clog_ast_literal_bool;
	(*lit)->value.integer = value;
}

static void clog_ast_literal_bool_promote(struct clog_ast_literal* lit)
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

static int clog_ast_literal_int_promote(struct clog_ast_literal* lit)
{
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
	if (lit1->type == clog_ast_literal_real && clog_ast_literal_real_promote(lit2))
		return 1;
	else if (lit2->type == clog_ast_literal_real &&	clog_ast_literal_real_promote(lit1))
		return 1;
	else if (clog_ast_literal_int_promote(lit1) && clog_ast_literal_int_promote(lit2))
		return 1;

	return 0;
}

static int clog_ast_literal_compare(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (clog_ast_literal_arith_convert(lit1,lit2))
	{
		if (lit1->type == clog_ast_literal_real)
			return (lit1->value.real > lit2->value.real ? 1 : (lit1->value.real == lit2->value.real ? 0 : -1));

		return (lit1->value.integer > lit2->value.integer ? 1 : (lit1->value.integer == lit2->value.integer ? 0 : -1));
	}

	return -2;
}

void clog_ast_literal_append_string(struct clog_parser* parser, struct clog_ast_literal** lit, struct clog_ast_literal* str, struct clog_token* token)
{
	if (!token->value.string.len)
		*lit = str;
	else
	{
		unsigned char* sz = clog_realloc(str->value.string.str,str->value.string.len + token->value.string.len);
		if (!sz)
		{
			clog_ast_literal_free(parser,str);
			out_of_memory(parser);
		}
		else
		{
			memcpy(sz+str->value.string.len,token->value.string.str,token->value.string.len);
			str->value.string.str = sz;
			str->value.string.len += token->value.string.len;
		}
	}

	clog_token_free(parser,token);
}

void clog_ast_expression_free(struct clog_parser* parser, struct clog_ast_expression* expr)
{
	if (expr)
	{
		switch (expr->type)
		{
		case clog_ast_expression_identifier:
		case clog_ast_expression_literal:
			clog_ast_literal_free(parser,expr->expr.literal);
			break;

		case clog_ast_expression_builtin:
			if (expr->expr.builtin)
			{
				clog_ast_expression_free(parser,expr->expr.builtin->args[0]);
				clog_ast_expression_free(parser,expr->expr.builtin->args[1]);
				clog_free(expr->expr.builtin);
			}
			break;

		case clog_ast_expression_function:
			if (expr->expr.function)
			{
				while (expr->expr.function->argc-- > 0)
					clog_ast_expression_free(parser,expr->expr.builtin->args[expr->expr.function->argc]);

				if (expr->expr.function->name.str)
					clog_free(expr->expr.function->name.str);

				clog_free(expr->expr.function);
			}
			break;
		}
	}
	clog_free(expr);
}

void clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit)
{
	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
		out_of_memory(parser);
	else
	{
		(*expr)->type = clog_ast_expression_literal;
		(*expr)->expr.literal = lit;
	}

	clog_ast_literal_free(parser,lit);
}

void clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token)
{
	struct clog_ast_literal* lit = NULL;
	clog_ast_literal_alloc(parser,&lit,token);
	if (!lit)
		return;

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_literal_free(parser,lit);
		return out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_identifier;
	(*expr)->expr.literal = lit;
}

void clog_ast_expression_alloc_preincdec(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1)
{
	clog_ast_expression_alloc_builtin(parser,expr,type,p1,NULL);
	if (*expr)
		(*expr)->lvalue = 1; /* <-- This is all that marks this as a pre-increment */
}

void clog_ast_expression_alloc_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	switch (type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
		if (!p1->lvalue)
		{
			clog_ast_expression_free(parser,p1);
			return syntax_error(parser);
		}
		break;

	case CLOG_TOKEN_PLUS:
		if (p1->type == clog_ast_expression_literal)
		{
			if (!p2)
			{
				/* Unary + */
				if (p1->expr.literal->type != clog_ast_literal_integer &&
					p1->expr.literal->type != clog_ast_literal_real)
				{
					clog_ast_expression_free(parser,p1);
					return syntax_error(parser);
				}
				*expr = p1;
				return;
			}
			else if (p2->type == clog_ast_expression_literal)
			{
				{ void* TODO; /* TODO: String + Literal... */ }

				if (p1->expr.literal->type == clog_ast_literal_string && p2->expr.literal->type == clog_ast_literal_string)
				{
					if (!p1->expr.literal->value.string.len)
					{
						clog_ast_expression_free(parser,p1);
						*expr = p2;
						return;
					}

					if (p2->expr.literal->value.string.len)
					{
						unsigned char* sz = clog_realloc(p1->expr.literal->value.string.str,p1->expr.literal->value.string.len + p2->expr.literal->value.string.len);
						if (!sz)
						{
							clog_ast_expression_free(parser,p1);
							clog_ast_expression_free(parser,p2);
							return out_of_memory(parser);
						}

						memcpy(sz+p1->expr.literal->value.string.len,p2->expr.literal->value.string.str,p2->expr.literal->value.string.len);
						p1->expr.literal->value.string.str = sz;
						p1->expr.literal->value.string.len += p2->expr.literal->value.string.len;
					}

					clog_ast_expression_free(parser,p2);
					*expr = p1;
					return;
				}

				if (clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
				{
					if (p1->expr.literal->type == clog_ast_literal_real)
						p1->expr.literal->value.real += p2->expr.literal->value.real;
					else
						p1->expr.literal->value.integer += p2->expr.literal->value.integer;

					clog_ast_expression_free(parser,p2);
					*expr = p1;
					return;
				}

				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1->type == clog_ast_expression_literal)
		{
			if (!p2)
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
					clog_ast_expression_free(parser,p1);
					return syntax_error(parser);
				}
				*expr = p1;
				return;
			}
			else if (p2->type == clog_ast_expression_literal)
			{
				if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
				{
					clog_ast_expression_free(parser,p1);
					clog_ast_expression_free(parser,p2);
					return syntax_error(parser);
				}

				if (p1->expr.literal->type == clog_ast_literal_real)
					p1->expr.literal->value.real -= p2->expr.literal->value.real;
				else
					p1->expr.literal->value.integer -= p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
		}
		break;

	case CLOG_TOKEN_EXCLAMATION:
		/* Logical NOT */
		if (p1->type == clog_ast_expression_literal)
		{
			clog_ast_literal_bool_promote(p1->expr.literal);
			p1->expr.literal->value.integer = (p1->expr.literal->value.integer == 0 ? 1 : 0);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_TILDA:
		/* Bitwise 1s complement */
		if (p1->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_int_promote(p1->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				return syntax_error(parser);
			}

			p1->expr.literal->value.integer = ~p1->expr.literal->value.integer;
			p1->expr.literal->type = clog_ast_literal_integer;
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_STAR:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			if (p1->expr.literal->type == clog_ast_literal_real)
				p1->expr.literal->value.real *= p2->expr.literal->value.real;
			else
				p1->expr.literal->value.integer *= p2->expr.literal->value.integer;

			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_SLASH:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int ok = 0;
			if (clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				if (p1->expr.literal->type == clog_ast_literal_real)
				{
					if (p2->expr.literal->value.real != 0.0)
					{
						p1->expr.literal->value.real *= p2->expr.literal->value.real;
						ok = 1;
					}
				}
				else if (p2->expr.literal->value.integer != 0.0)
				{
					p1->expr.literal->value.integer *= p2->expr.literal->value.integer;
					ok = 1;
				}
			}

			clog_ast_expression_free(parser,p2);
			if (ok)
			{
				clog_ast_expression_free(parser,p1);
				return syntax_error(parser);
			}
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_PERCENT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (clog_ast_literal_int_promote(p1->expr.literal) &&
					clog_ast_literal_int_promote(p2->expr.literal) &&
					p2->expr.literal->value.integer != 0)
			{
				p1->expr.literal->value.integer %= p2->expr.literal->value.integer;
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return syntax_error(parser);
		}
		break;

	case CLOG_TOKEN_RIGHT_SHIFT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (clog_ast_literal_int_promote(p1->expr.literal) && clog_ast_literal_int_promote(p2->expr.literal))
			{
				p1->expr.literal->value.integer >>= p2->expr.literal->value.integer;
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return syntax_error(parser);
		}
		break;

	case CLOG_TOKEN_LEFT_SHIFT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (clog_ast_literal_int_promote(p1->expr.literal) && clog_ast_literal_int_promote(p2->expr.literal))
			{
				p1->expr.literal->value.integer <<= p2->expr.literal->value.integer;
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return syntax_error(parser);
		}
		break;

	case CLOG_TOKEN_LESS_THAN:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b == -1 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_GREATER_THAN:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b == 1 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_LESS_THAN_EQUALS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b != 1 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_GREATER_THAN_EQUALS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b != -1 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_EQUALS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b == 0 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_NOT_EQUALS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			int b = clog_ast_literal_compare(p1->expr.literal,p2->expr.literal);
			if (b == -2)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return syntax_error(parser);
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			p1->expr.literal->value.integer = (b != 0 ? 1 : 0);
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	default:
		break;
	}

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_builtin;
	(*expr)->lvalue = 0;
	(*expr)->expr.builtin = clog_malloc(sizeof(struct clog_ast_expression_builtin));
	if (!(*expr)->expr.builtin)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,*expr);
		*expr = NULL;
		return out_of_memory(parser);
	}

	(*expr)->expr.builtin->type = type;
	(*expr)->expr.builtin->args[0] = p1;
	(*expr)->expr.builtin->args[1] = p2;
}









/* External functions defined by ragel and lemon */
int clog_tokenize(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param, void* parser);
void* clog_parserAlloc(void* (*)(size_t));
void clog_parserFree(void *p, void (*)(void*));

int clog_parse(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param)
{
	int retval = 0;
	struct clog_parser parser = {0,1,NULL};

	parser.lemon_parser = clog_parserAlloc(&clog_malloc);
	if (!parser.lemon_parser)
		return -1;

	retval = clog_tokenize(rd_fn,rd_param,&parser);
	if (retval == 0 && !parser.failed)
		clog_parser(parser.lemon_parser,0,NULL,&parser);

	clog_parserFree(parser.lemon_parser,&clog_free);

	return retval;
}
