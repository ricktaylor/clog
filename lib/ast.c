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

void clog_out_of_memory(struct clog_parser* parser)
{
	printf("Out of memory building AST\n");
	parser->failed = 1;
}

void clog_syntax_error(struct clog_parser* parser, const char* msg)
{
	printf("Syntax error at line %lu: %s\n",parser->line,msg);
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
		clog_out_of_memory(parser);
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
		return clog_out_of_memory(parser);

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
	if (lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
	{
		int i = memcmp(lit1->value.string.str,lit1->value.string.str,lit1->value.string.len < lit2->value.string.len ? lit1->value.string.len : lit2->value.string.len);
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
	if (token->value.string.len)
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
		case clog_ast_expression_identifier:
		case clog_ast_expression_literal:
			clog_ast_literal_free(parser,expr->expr.literal);
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
	}
	clog_free(expr);
}

void clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit)
{
	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_literal_free(parser,lit);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_literal;
	(*expr)->expr.literal = lit;
}

void clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token)
{
	struct clog_ast_literal* lit = NULL;

	*expr = NULL;
	clog_ast_literal_alloc(parser,&lit,token);
	if (!lit)
		return;

	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_literal_free(parser,lit);
		return clog_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_identifier;
	(*expr)->lvalue = 1;
	(*expr)->expr.literal = lit;
}

void clog_ast_expression_alloc_dot(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* p1, struct clog_token* token)
{
	*expr = NULL;
	if (!p1->lvalue)
	{
		clog_ast_expression_free(parser,p1);
		clog_token_free(parser,token);
		return clog_syntax_error(parser,". requires an lvalue");
	}

	struct clog_ast_expression* p2 = NULL;
	clog_ast_expression_alloc_id(parser,&p2,token);
	if (!p2)
	{
		clog_ast_expression_free(parser,p1);
		return;
	}

	clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_DOT,p1,p2);
	if (*expr)
		(*expr)->lvalue = 1;
}

void clog_ast_expression_alloc_builtin_lvalue(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	clog_ast_expression_alloc_builtin2(parser,expr,type,p1,p2);
	if (*expr)
		(*expr)->lvalue = 1;
}

void clog_ast_expression_alloc_builtin1(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1)
{
	*expr = NULL;
	switch (type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
		if (!p1->lvalue)
		{
			clog_ast_expression_free(parser,p1);
			return clog_syntax_error(parser,type == CLOG_TOKEN_DOUBLE_PLUS ? "++ requires an lvalue" :  "++ requires an lvalue");
		}
		break;

	case CLOG_TOKEN_PLUS:
		if (p1->type == clog_ast_expression_literal)
		{
			/* Unary + */
			if (p1->expr.literal->type != clog_ast_literal_integer &&
				p1->expr.literal->type != clog_ast_literal_real)
			{
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"+ requires a number");
			}
			*expr = p1;
			return;
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
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"- requires a number");
			}
			*expr = p1;
			return;
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
				return clog_syntax_error(parser,"~ requires integer");
			}

			p1->expr.literal->value.integer = ~p1->expr.literal->value.integer;
			p1->expr.literal->type = clog_ast_literal_integer;
			*expr = p1;
			return;
		}
		break;

	default:
		break;

	}

	clog_ast_expression_alloc_builtin3(parser,expr,type,p1,NULL,NULL);
}

void clog_ast_expression_alloc_builtin2(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	*expr = NULL;
	switch (type)
	{
	case CLOG_TOKEN_OPEN_BRACKET:
		if (!p1->lvalue)
		{
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error(parser,"Subscript requires an lvalue");
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
					return;
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
				return;
			}

			if (p1->expr.literal->type != clog_ast_literal_string && p2->expr.literal->type != clog_ast_literal_string)
			{
				if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
				{
					clog_ast_expression_free(parser,p1);
					clog_ast_expression_free(parser,p2);
					return clog_syntax_error(parser,"+ requires two numbers");
				}

				if (p1->expr.literal->type == clog_ast_literal_real)
					p1->expr.literal->value.real += p2->expr.literal->value.real;
				else
					p1->expr.literal->value.integer += p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (!clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"- requires two numbers");
			}

			if (p1->expr.literal->type == clog_ast_literal_real)
				p1->expr.literal->value.real -= p2->expr.literal->value.real;
			else
				p1->expr.literal->value.integer -= p2->expr.literal->value.integer;

			clog_ast_expression_free(parser,p2);
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
				return clog_syntax_error(parser,"* requires two numbers");
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
				return clog_syntax_error(parser,"/ requires two numbers");
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
			return clog_syntax_error(parser,"% requires two integers");
		}
		break;

	case CLOG_TOKEN_RIGHT_SHIFT:
	case CLOG_TOKEN_LEFT_SHIFT:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
			if (clog_ast_literal_int_promote(p1->expr.literal) && clog_ast_literal_int_promote(p2->expr.literal))
			{
				if (type == CLOG_TOKEN_RIGHT_SHIFT)
					p1->expr.literal->value.integer >>= p2->expr.literal->value.integer;
				else
					p1->expr.literal->value.integer <<= p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error(parser,type == CLOG_TOKEN_RIGHT_SHIFT ? ">> requires two integers" : "<< requires two integers");
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
					return clog_syntax_error(parser,"< requires two numbers or strings");
				p1->expr.literal->value.integer = (b < 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN:
				if (b == -2)
					return clog_syntax_error(parser,"> requires two numbers or strings");
				p1->expr.literal->value.integer = (b > 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_LESS_THAN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"<= requires two numbers or strings");
				p1->expr.literal->value.integer = (b <= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,">= requires two numbers or strings");
				p1->expr.literal->value.integer = (b >= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"== requires two numbers or strings");
				p1->expr.literal->value.integer = (b == 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_NOT_EQUALS:
				if (b == -2)
					return clog_syntax_error(parser,"!= requires two numbers or strings");
				p1->expr.literal->value.integer = (b != 0 ? 1 : 0);
				break;
			}

			p1->expr.literal->type = clog_ast_literal_bool;
			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return;
		}
		break;

	case CLOG_TOKEN_COMMA:
		if (p1->type == clog_ast_expression_literal || p1->type == clog_ast_expression_identifier)
		{
			clog_ast_expression_free(parser,p1);
			*expr = p2;
			return;
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
				return;
			}
			else
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
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
				return;
			}
			else
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return;
			}
		}
		break;

	case CLOG_TOKEN_BAR:
	case CLOG_TOKEN_CARET:
	case CLOG_TOKEN_AMPERSAND:
		if (p1->type == clog_ast_expression_literal && p2->type == clog_ast_expression_literal)
		{
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
				return;
			}
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			if (type == CLOG_TOKEN_BAR)
				return clog_syntax_error(parser,"| requires an integer");
			else if (type == CLOG_TOKEN_CARET)
				return clog_syntax_error(parser,"^ requires an integer");
			else
				return clog_syntax_error(parser,"& requires an integer");
		}
		break;

	default:
		break;
	}

	clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL);
}

void clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3)
{
	*expr = NULL;
	if (type == CLOG_TOKEN_QUESTION && p1->type == clog_ast_expression_literal)
	{
		clog_ast_literal_bool_promote(p1->expr.literal);
		if (p1->expr.literal->value.integer)
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
		return;
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
	(*expr)->expr.builtin->args[0] = p1;
	(*expr)->expr.builtin->args[1] = p2;
	(*expr)->expr.builtin->args[2] = p3;
}

void clog_ast_expression_alloc_call(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* call, struct clog_ast_expression_list* list)
{
	*expr = NULL;
	if (!call->lvalue)
	{
		clog_ast_expression_free(parser,call);
		clog_ast_expression_list_free(parser,list);
		return clog_syntax_error(parser,"Function call requires an lvalue");
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
	(*expr)->lvalue = 1;
}

void clog_ast_expression_list_free(struct clog_parser* parser, struct clog_ast_expression_list* list)
{
	if (list)
	{
		if (list->expr)
			clog_ast_expression_free(parser,list->expr);

		clog_ast_expression_list_free(parser,list->next);

		clog_free(list);
	}
}

void clog_ast_expression_list_alloc(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr)
{
	*list = clog_malloc(sizeof(struct clog_ast_expression_list));
	if (!*list)
	{
		clog_ast_expression_free(parser,expr);
		return clog_out_of_memory(parser);
	}

	(*list)->expr = expr;
	(*list)->next = NULL;
}

void clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_expression* expr)
{
	struct clog_ast_expression_list** tail = &list->next;
	while (list->next)
	{
		tail = &list->next;
		list = list->next;
	}

	clog_ast_expression_list_alloc(parser,tail,expr);
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

	clog_parser(parser.lemon_parser,0,NULL,&parser);

	clog_parserFree(parser.lemon_parser,&clog_free);

	if (!retval)
		printf("Success!\n");

	return retval;
}
