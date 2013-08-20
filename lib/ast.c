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

void clog_syntax_error(struct clog_parser* parser, const char* msg, unsigned long line)
{
	printf("Syntax error at line %lu: %s\n",line,msg);
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

void clog_token_alloc(struct clog_parser* parser, struct clog_token** token, const unsigned char* sz, size_t len)
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
			token->value.string.str = NULL;
		}

		(*lit)->line = parser->line;
	}

	clog_token_free(parser,token);
}

static struct clog_ast_literal* clog_ast_literal_clone(struct clog_parser* parser, const struct clog_ast_literal* lit)
{
	struct clog_ast_literal* ret = NULL;

	if (!lit)
		return NULL;

	ret = clog_malloc(sizeof(struct clog_ast_literal));
	if (!ret)
	{
		clog_out_of_memory(parser);
		return NULL;
	}

	ret->type = lit->type;
	ret->line = lit->line;

	switch (ret->type)
	{
	case clog_ast_literal_null:
	case clog_ast_literal_bool:
	case clog_ast_literal_integer:
		ret->value.integer = lit->value.integer;
		break;

	case clog_ast_literal_string:
		{
			ret->value.string.len = lit->value.string.len;
			ret->value.string.str = NULL;

			if (ret->value.string.len)
			{
				ret->value.string.str = clog_malloc(ret->value.string.len+1);
				if (!ret->value.string.str)
				{
					clog_free(ret);
					ret = NULL;
					clog_out_of_memory(parser);
				}
				else
				{
					memcpy(ret->value.string.str,lit->value.string.str,ret->value.string.len);
					ret->value.string.str[ret->value.string.len] = 0;
				}
			}
		}
		break;

	case clog_ast_literal_real:
		ret->value.real = lit->value.real;
		break;
	}

	return ret;
}

void clog_ast_literal_alloc_bool(struct clog_parser* parser, struct clog_ast_literal** lit, int value)
{
	*lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*lit)
		return clog_out_of_memory(parser);

	(*lit)->type = clog_ast_literal_bool;
	(*lit)->line = parser->line;
	(*lit)->value.integer = value;
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

void clog_ast_expression_alloc_literal(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_literal* lit)
{
	*expr = NULL;

	if (lit)
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
}

void clog_ast_expression_alloc_id(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_token* token)
{
	*expr = NULL;

	if (token)
	{
		*expr = clog_malloc(sizeof(struct clog_ast_expression));
		if (!*expr)
		{
			clog_token_free(parser,token);
			return clog_out_of_memory(parser);
		}

		(*expr)->type = clog_ast_expression_identifier;
		(*expr)->lvalue = 1;

		clog_ast_literal_alloc(parser,&(*expr)->expr.identifier,token);
		if (!(*expr)->expr.identifier)
		{
			clog_free(*expr);
			*expr = NULL;
		}
	}
}

void clog_ast_expression_alloc_dot(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* p1, struct clog_token* token)
{
	*expr = NULL;

	if (!token || !p1)
	{
		clog_ast_expression_free(parser,p1);
		clog_token_free(parser,token);
		return;
	}

	if (!p1->lvalue)
	{
		unsigned long line = clog_ast_expression_line(p1);
		clog_ast_expression_free(parser,p1);
		clog_token_free(parser,token);
		return clog_syntax_error(parser,". requires an lvalue",line);
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

	if (!p1)
		return;

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
				{
					unsigned long line = clog_ast_expression_line(p1);
					clog_ast_expression_free(parser,p1);
					return clog_syntax_error(parser,"- requires a number",line);
				}
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
				unsigned long line = clog_ast_expression_line(p1);
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"~ requires integer",line);
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

	if (!p1 || !p2)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return;
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
				return;
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
			return;
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
			return;
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
			return;
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
			return;
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
				return;
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
				return;
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

	clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL);
}

void clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3)
{
	*expr = NULL;

	if (type == CLOG_TOKEN_QUESTION)
	{
		if (!p1 || !p2 || !p3)
		{
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			clog_ast_expression_free(parser,p3);
			return;
		}

		if (p1->type == clog_ast_expression_literal)
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
}

void clog_ast_expression_alloc_assign(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	*expr = NULL;

	if (!p1 || !p2)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return;
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

	clog_ast_expression_alloc_builtin3(parser,expr,type,p1,p2,NULL);
	if (*expr)
		(*expr)->lvalue = 1;
}

void clog_ast_expression_alloc_call(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_expression* call, struct clog_ast_expression_list* list)
{
	*expr = NULL;

	if (!call)
	{
		clog_ast_expression_list_free(parser,list);
		return;
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
		clog_ast_expression_free(parser,list->expr);
		clog_ast_expression_list_free(parser,list->next);

		clog_free(list);
	}
}

void clog_ast_expression_list_alloc(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr)
{
	*list = NULL;

	if (expr)
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
}

struct clog_ast_expression_list* clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_expression* expr)
{
	if (list && expr)
	{
		struct clog_ast_expression** tail = &list->expr;
		struct clog_ast_expression_list* next = list;
		while (next)
		{
			tail = &list->expr;
			next = next->next;
		}

		*tail = expr;
	}

	return list;
}

void clog_ast_statement_free(struct clog_parser* parser, struct clog_ast_statement* stmt)
{
	if (stmt)
	{
		switch (stmt->type)
		{
		case clog_ast_statement_expression:
			clog_ast_expression_free(parser,stmt->stmt.expression);
			break;

		case clog_ast_statement_block:
			clog_ast_statement_list_free(parser,stmt->stmt.block);
			break;

		case clog_ast_statement_declaration:
			clog_ast_literal_free(parser,stmt->stmt.declaration);
			break;
		}

		clog_free(stmt);
	}
}

void clog_ast_statement_alloc_expression(struct clog_parser* parser, struct clog_ast_statement** stmt, struct clog_ast_expression* expr)
{
	*stmt = NULL;

	if (expr)
	{
		*stmt = clog_malloc(sizeof(struct clog_ast_statement));
		if (!*stmt)
		{
			clog_ast_expression_free(parser,expr);
			return clog_out_of_memory(parser);
		}

		(*stmt)->type = clog_ast_statement_expression;
		(*stmt)->stmt.expression = expr;
	}
}

void clog_ast_statement_alloc_block(struct clog_parser* parser, struct clog_ast_statement** stmt, struct clog_ast_statement_list* list)
{
	*stmt = NULL;

	if (list)
	{
		/* Check for block containing only 1 block */
		if (list->stmt->type == clog_ast_statement_block && list->next == NULL)
		{
			*stmt = list->stmt;
			list->stmt = NULL;
			clog_ast_statement_list_free(parser,list);
			return;
		}

		*stmt = clog_malloc(sizeof(struct clog_ast_statement));
		if (!*stmt)
		{
			clog_ast_statement_list_free(parser,list);
			return clog_out_of_memory(parser);
		}

		(*stmt)->type = clog_ast_statement_block;
		(*stmt)->stmt.block = list;
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

void clog_ast_statement_list_alloc(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement* stmt)
{
	*list = NULL;

	if (stmt)
	{
		*list = clog_malloc(sizeof(struct clog_ast_statement_list));
		if (!*list)
		{
			clog_ast_statement_free(parser,stmt);
			return clog_out_of_memory(parser);
		}

		(*list)->stmt = stmt;
		(*list)->next = NULL;
	}
}

void clog_ast_statement_list_alloc_expression(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_expression* expr)
{
	*list = NULL;

	if (expr)
	{
		*list = clog_malloc(sizeof(struct clog_ast_statement_list));
		if (!*list)
		{
			clog_ast_expression_free(parser,expr);
			return clog_out_of_memory(parser);
		}

		clog_ast_statement_alloc_expression(parser,&(*list)->stmt,expr);
		if (!(*list)->stmt)
		{
			clog_free(*list);
			*list = NULL;
			return;
		}

		(*list)->next = NULL;
	}
}

struct clog_ast_statement_list* clog_ast_statement_list_append(struct clog_parser* parser, struct clog_ast_statement_list* list, struct clog_ast_statement_list* next)
{
	if (next)
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
	}
	return list;
}

void clog_ast_statement_alloc_declaration(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_token* id, struct clog_ast_expression* init)
{
	struct clog_ast_statement* decl = NULL;

	*list = NULL;

	if (!id)
		return;

	if (!init)
	{
		decl = clog_malloc(sizeof(struct clog_ast_statement));
		if (!decl)
		{
			clog_token_free(parser,id);
			return clog_out_of_memory(parser);
		}

		decl->type = clog_ast_statement_declaration;

		clog_ast_literal_alloc(parser,&decl->stmt.declaration,id);
		if (!decl->stmt.declaration)
		{
			clog_free(decl);
			return;
		}

		id = NULL;
	}
	else
	{
		struct clog_token* id2;
		clog_token_alloc(parser,&id2,id->value.string.str,id->value.string.len);
		if (!id2)
		{
			clog_token_free(parser,id);
			clog_ast_expression_free(parser,init);
			return;
		}

		decl = clog_malloc(sizeof(struct clog_ast_statement));
		if (!decl)
		{
			clog_token_free(parser,id);
			clog_token_free(parser,id2);
			clog_ast_expression_free(parser,init);
			return clog_out_of_memory(parser);
		}

		decl->type = clog_ast_statement_declaration;

		clog_ast_literal_alloc(parser,&decl->stmt.declaration,id2);
		if (!decl->stmt.declaration)
		{
			clog_free(decl);
			clog_token_free(parser,id);
			clog_ast_expression_free(parser,init);
			return;
		}
	}

	clog_ast_statement_list_alloc(parser,list,decl);
	if (!*list)
	{
		clog_token_free(parser,id);
		clog_ast_expression_free(parser,init);
		return;
	}

	if (init)
	{
		struct clog_ast_expression* id_expr = NULL;
		struct clog_ast_expression* assign_expr = NULL;

		clog_ast_expression_alloc_id(parser,&id_expr,id);
		if (!id_expr)
		{
			clog_ast_expression_free(parser,init);
			clog_ast_statement_list_free(parser,*list);
			*list = NULL;
			return;
		}

		clog_ast_expression_alloc_assign(parser,&assign_expr,CLOG_TOKEN_ASSIGN,id_expr,init);
		if (!assign_expr)
		{
			clog_ast_statement_list_free(parser,*list);
			*list = NULL;
			return;
		}

		clog_ast_statement_list_alloc_expression(parser,&(*list)->next,assign_expr);
		if (!(*list)->next)
		{
			clog_ast_statement_list_free(parser,*list);
			*list = NULL;
		}
	}
}

void clog_ast_statement_alloc_if(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond, struct clog_ast_statement_list* true_expr, struct clog_ast_statement_list* false_expr)
{
	*list = NULL;

	if (!cond)
		return;

	/* Rewrite to force compound statements: if (x) int i;  =>  if (x) { int i; } */
	if (true_expr && true_expr->stmt->type != clog_ast_statement_block)
	{
		struct clog_ast_statement* s = NULL;
		clog_ast_statement_alloc_block(parser,&s,true_expr);
		if (!s)
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,false_expr);
			return;
		}

		clog_ast_statement_list_alloc(parser,&true_expr,s);
		if (!true_expr)
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,false_expr);
			return;
		}
	}
	if (false_expr && false_expr->stmt->type != clog_ast_statement_block)
	{
		struct clog_ast_statement* s = NULL;
		clog_ast_statement_alloc_block(parser,&s,false_expr);
		if (!s)
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,true_expr);
			return;
		}

		clog_ast_statement_list_alloc(parser,&false_expr,s);
		if (!false_expr)
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,true_expr);
			return;
		}
	}

	/* Check to see if we have an expression of the kind: if (var x = 12) */
	if (cond && cond->stmt->type == clog_ast_statement_declaration)
	{
		/* We must check to see if an identical declaration occurs in the outermost block true_expr and false_expr */
		if (true_expr)
		{
			struct clog_ast_statement_list* l = true_expr->stmt->stmt.block;
			for (;l;l = l->next)
			{
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
				{
					unsigned long line = l->stmt->stmt.declaration->line;
					clog_ast_statement_list_free(parser,cond);
					clog_ast_statement_list_free(parser,true_expr);
					clog_ast_statement_list_free(parser,false_expr);
					return clog_syntax_error(parser,"Variable already declared",line);
				}
			}
		}

		if (false_expr)
		{
			struct clog_ast_statement_list* l = false_expr->stmt->stmt.block;
			for (;l;l = l->next)
			{
				if (l->stmt->type == clog_ast_statement_declaration && clog_ast_literal_compare(l->stmt->stmt.declaration,cond->stmt->stmt.declaration) == 0)
				{
					unsigned long line = l->stmt->stmt.declaration->line;
					clog_ast_statement_list_free(parser,cond);
					clog_ast_statement_list_free(parser,true_expr);
					clog_ast_statement_list_free(parser,false_expr);
					return clog_syntax_error(parser,"Variable already declared",line);
				}
			}
		}
	}
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

		default:
			printf("*******WTF***********\n");
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
