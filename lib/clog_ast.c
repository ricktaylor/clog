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

int clog_ast_out_of_memory(struct clog_parser* parser)
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
		return clog_ast_out_of_memory(parser);

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
			return clog_ast_out_of_memory(parser);
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
		return printf("(string)");

	case CLOG_TOKEN_INTEGER:
		if (token && token->type == clog_token_integer)
			return printf("%ld",token->value.integer);
		return printf("(integer)");

	case CLOG_TOKEN_FLOAT:
		if (token && token->type == clog_token_real)
			return printf("%g",token->value.real);
		return printf("(arith)");

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
	case CLOG_TOKEN_CARET_ASSIGN:
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
	case CLOG_TOKEN_IN:
		return printf("in");
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

static int clog_ast_string_compare(const struct clog_string* s1, const struct clog_string* s2)
{
	if (s1 && s2)
	{
		int i = memcmp(s1->str,s2->str,s1->len < s2->len ? s1->len : s2->len);
		if (i != 0)
			return (i > 0 ? 1 : -1);

		return (s1->len > s2->len ? 1 : (s1->len == s2->len ? 0 : -1));
	}
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

int clog_ast_literal_clone(struct clog_parser* parser, struct clog_ast_literal** new, const struct clog_ast_literal* lit)
{
	*new = NULL;

	if (!lit)
		return 1;

	*new = clog_malloc(sizeof(struct clog_ast_literal));
	if (!*new)
		return clog_ast_out_of_memory(parser);

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
		(*new)->value.string.str = NULL;
		if ((*new)->value.string.len)
		{
			(*new)->value.string.str = clog_malloc((*new)->value.string.len+1);
			if (!(*new)->value.string.str)
			{
				clog_free(*new);
				*new = NULL;
				return clog_ast_out_of_memory(parser);
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
		return clog_ast_out_of_memory(parser);
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
		return clog_ast_out_of_memory(parser);

	(*lit)->type = clog_ast_literal_bool;
	(*lit)->line = parser->line;
	(*lit)->value.integer = value;

	return 1;
}

int clog_ast_literal_int_promote(struct clog_ast_literal* lit)
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
	if (lit && lit->type == clog_ast_literal_real)
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

int clog_ast_literal_arith_convert(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (lit1 && lit1->type == clog_ast_literal_real)
		return clog_ast_literal_real_promote(lit2);

	if (lit2 && lit2->type == clog_ast_literal_real)
		return clog_ast_literal_real_promote(lit1);

	return (clog_ast_literal_int_promote(lit1) && clog_ast_literal_int_promote(lit2));
}

int clog_ast_literal_compare(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (lit1 && lit2)
	{
		if (lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
			return clog_ast_string_compare(&lit1->value.string,&lit2->value.string);

		if (clog_ast_literal_arith_convert(lit1,lit2))
		{
			if (lit1->type == clog_ast_literal_real)
				return (lit1->value.real > lit2->value.real ? 1 : (lit1->value.real == lit2->value.real ? 0 : -1));

			return (lit1->value.integer > lit2->value.integer ? 1 : (lit1->value.integer == lit2->value.integer ? 0 : -1));
		}
	}
	return -2;
}

int clog_ast_literal_id_compare(const struct clog_ast_literal* lit1, const struct clog_ast_literal* lit2)
{
	if (lit1 && lit2 && lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
		return clog_ast_string_compare(&lit1->value.string,&lit2->value.string);

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
			lit = NULL;
			clog_ast_out_of_memory(parser);
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

		case clog_ast_expression_variable:
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
		return clog_ast_out_of_memory(parser);

	(*new)->type = expr->type;

	switch ((*new)->type)
	{
	case clog_ast_expression_literal:
		ok = clog_ast_literal_clone(parser,&(*new)->expr.literal,expr->expr.literal);
		break;

	case clog_ast_expression_identifier:
		ok = clog_ast_literal_clone(parser,&(*new)->expr.identifier,expr->expr.identifier);
		break;

	case clog_ast_expression_variable:
		(*new)->expr.variable = expr->expr.variable;
		ok = 1;
		break;

	case clog_ast_expression_builtin:
		(*new)->expr.builtin = clog_malloc(sizeof(struct clog_ast_expression_builtin));
		if (!(*new)->expr.builtin)
		{
			ok = clog_ast_out_of_memory(parser);
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
			ok = clog_ast_out_of_memory(parser);
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

	case clog_ast_expression_variable:
		return 0;

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
		return clog_ast_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_literal;
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
		return clog_ast_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_identifier;

	if (!clog_ast_literal_alloc(parser,&(*expr)->expr.identifier,token))
	{
		clog_free(*expr);
		*expr = NULL;
		return 0;
	}

	return 1;
}

static int clog_ast_expression_alloc_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3)
{
	*expr = clog_malloc(sizeof(struct clog_ast_expression));
	if (!*expr)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,p3);
		return clog_ast_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_builtin;
	(*expr)->expr.builtin = clog_malloc(sizeof(struct clog_ast_expression_builtin));
	if (!(*expr)->expr.builtin)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,p3);
		clog_free(*expr);
		*expr = NULL;
		return clog_ast_out_of_memory(parser);
	}

	(*expr)->expr.builtin->type = type;
	(*expr)->expr.builtin->line = parser->line;
	(*expr)->expr.builtin->args[0] = p1;
	(*expr)->expr.builtin->args[1] = p2;
	(*expr)->expr.builtin->args[2] = p3;

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

	return clog_ast_expression_alloc_builtin(parser,expr,CLOG_TOKEN_DOT,p1,p2,NULL);
}

int clog_ast_expression_alloc_builtin1(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1)
{
	*expr = NULL;

	if (!p1)
		return 0;

	if (p1->type == clog_ast_expression_literal)
	{
		switch (type)
		{
		case CLOG_TOKEN_PLUS:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"Unary + applied to string",p1->expr.literal->line);

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
				p1->expr.literal->type = clog_ast_literal_integer;
				break;

			case clog_ast_literal_integer:
			case clog_ast_literal_real:
				break;
			}
			*expr = p1;
			return 1;

		case CLOG_TOKEN_MINUS:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"Unary - applied to string",p1->expr.literal->line);

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
				p1->expr.literal->value.integer = -p1->expr.literal->value.integer;
				p1->expr.literal->type = clog_ast_literal_integer;
				break;

			case clog_ast_literal_integer:
				p1->expr.literal->value.integer = -p1->expr.literal->value.integer;
				break;

			case clog_ast_literal_real:
				p1->expr.literal->value.real = -p1->expr.literal->value.real;
				break;
			}
			*expr = p1;
			return 1;

		case CLOG_TOKEN_EXCLAMATION:
			clog_ast_literal_bool_promote(p1->expr.literal);
			p1->expr.literal->value.integer = p1->expr.literal->value.integer ? 0 : 1;
			*expr = p1;
			return 1;

		case CLOG_TOKEN_TILDA:
			if (!clog_ast_literal_int_promote(p1->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				return clog_syntax_error(parser,"~ requires an integer",p1->expr.literal->line);
			}
			p1->expr.literal->value.integer = ~p1->expr.literal->value.integer;
			*expr = p1;
			return 1;

		case CLOG_TOKEN_TRUE:
			/* Cast to bool */
			clog_ast_literal_bool_promote(p1->expr.literal);
			*expr = p1;
			return 1;

		case CLOG_TOKEN_INTEGER:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_integer:
				p1->expr.literal->type = clog_ast_literal_integer;
				*expr = p1;
				return 1;

			case clog_ast_literal_real:
				{
					long v = p1->expr.literal->value.real;
					p1->expr.literal->value.integer = v;
					*expr = p1;
					return 1;
				}
			default:
				break;
			}
			break;

		case CLOG_TOKEN_FLOAT:
			/* Cast to arithmetic type */
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_integer:
				p1->expr.literal->type = clog_ast_literal_integer;
				*expr = p1;
				return 1;

			case clog_ast_literal_real:
				*expr = p1;
				return 1;

			default:
				break;
			}
			break;

		case CLOG_TOKEN_STRING:
			if (p1->expr.literal->type == clog_ast_literal_null)
			{
				p1->expr.literal->value.string.str = NULL;
				p1->expr.literal->value.string.len = 0;
				p1->expr.literal->type = clog_ast_literal_string;
			}
			if (p1->expr.literal->type == clog_ast_literal_string)
			{
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_DOUBLE_PLUS:
		case CLOG_TOKEN_DOUBLE_MINUS:
			clog_ast_expression_free(parser,p1);
			return clog_syntax_error_token(parser,NULL," applied to literal value",type,NULL,p1->expr.literal->line);

		default:
			break;
		}
	}

	return clog_ast_expression_alloc_builtin(parser,expr,type,p1,NULL,NULL);
}

int clog_ast_expression_alloc_builtin2(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2)
{
	*expr = NULL;

	if (type == CLOG_TOKEN_DOUBLE_PLUS || type == CLOG_TOKEN_DOUBLE_MINUS)
	{
		if (!p2)
			return 0;

		if (p2->type == clog_ast_expression_literal)
		{
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error_token(parser,NULL," applied to literal value",type,NULL,p2->expr.literal->line);
		}
		return clog_ast_expression_alloc_builtin(parser,expr,type,p1,p2,NULL);
	}

	if (!p1 || !p2)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		return 0;
	}

	if (p1->type == clog_ast_expression_literal)
	{
		switch (type)
		{
		case CLOG_TOKEN_OPEN_BRACKET:
		case CLOG_TOKEN_ASSIGN:
		case CLOG_TOKEN_STAR_ASSIGN:
		case CLOG_TOKEN_SLASH_ASSIGN:
		case CLOG_TOKEN_PERCENT_ASSIGN:
		case CLOG_TOKEN_PLUS_ASSIGN:
		case CLOG_TOKEN_MINUS_ASSIGN:
		case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
		case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
		case CLOG_TOKEN_AMPERSAND_ASSIGN:
		case CLOG_TOKEN_CARET_ASSIGN:
		case CLOG_TOKEN_BAR_ASSIGN:
			clog_ast_expression_free(parser,p1);
			clog_ast_expression_free(parser,p2);
			return clog_syntax_error_token(parser,NULL," applied to literal value",type,NULL,p1->expr.literal->line);

		case CLOG_TOKEN_OR:
			clog_ast_literal_bool_promote(p1->expr.literal);
			if (p1->expr.literal->value.integer)
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			clog_ast_expression_free(parser,p1);
			return clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_TRUE,p2);

		case CLOG_TOKEN_AND:
			clog_ast_literal_bool_promote(p1->expr.literal);
			if (!p1->expr.literal->value.integer)
			{
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			clog_ast_expression_free(parser,p1);
			return clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_TRUE,p2);

		case CLOG_TOKEN_BAR:
		case CLOG_TOKEN_CARET:
		case CLOG_TOKEN_AMPERSAND:
			if (!clog_ast_literal_int_promote(p1->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error_token(parser,NULL," requires an integer",type,NULL,p1->expr.literal->line);
			}
			if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_INTEGER,p2))
			{
				clog_ast_expression_free(parser,p1);
				return 0;
			}
			if (p2->type != clog_ast_expression_literal)
				return clog_ast_expression_alloc_builtin(parser,expr,type,p2,p1,NULL);

			if (type == CLOG_TOKEN_BAR)
				p1->expr.literal->value.integer |= p2->expr.literal->value.integer;
			else if (type == CLOG_TOKEN_CARET)
				p1->expr.literal->value.integer ^= p2->expr.literal->value.integer;
			else
				p1->expr.literal->value.integer &= p2->expr.literal->value.integer;

			clog_ast_expression_free(parser,p2);
			*expr = p1;
			return 1;

		case CLOG_TOKEN_EQUALS:
		case CLOG_TOKEN_NOT_EQUALS:
		case CLOG_TOKEN_LESS_THAN:
		case CLOG_TOKEN_GREATER_THAN:
		case CLOG_TOKEN_LESS_THAN_EQUALS:
		case CLOG_TOKEN_GREATER_THAN_EQUALS:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_STRING,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				break;

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_real:
			case clog_ast_literal_integer:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_FLOAT,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				break;
			}
			if (p2->type == clog_ast_expression_literal)
			{
				int cmp = -2;
				switch (p1->expr.literal->type)
				{
				case clog_ast_literal_string:
					cmp = clog_ast_string_compare(&p1->expr.literal->value.string,&p2->expr.literal->value.string);
					clog_free(p1->expr.literal->value.string.str);
					break;

				case clog_ast_literal_bool:
				case clog_ast_literal_null:
				case clog_ast_literal_real:
				case clog_ast_literal_integer:
					clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal);
					if (p1->expr.literal->type == clog_ast_literal_real)
						cmp = (p1->expr.literal->value.real > p2->expr.literal->value.real ? 1 : (p1->expr.literal->value.real == p2->expr.literal->value.real ? 0 : -1));
					else
						cmp = (p1->expr.literal->value.integer > p2->expr.literal->value.integer ? 1 : (p1->expr.literal->value.integer == p2->expr.literal->value.integer ? 0 : -1));
					break;
				}

				clog_ast_expression_free(parser,p2);

				if (type == CLOG_TOKEN_NOT_EQUALS)
					p1->expr.literal->value.integer = (cmp != 0 ? 1 : 0);
				else if (type == CLOG_TOKEN_LESS_THAN)
					p1->expr.literal->value.integer = (cmp < 0 ? 1 : 0);
				else if (type == CLOG_TOKEN_LESS_THAN_EQUALS)
					p1->expr.literal->value.integer = (cmp <= 0 ? 1 : 0);
				else if (type == CLOG_TOKEN_GREATER_THAN)
					p1->expr.literal->value.integer = (cmp > 0 ? 1 : 0);
				else if (type == CLOG_TOKEN_GREATER_THAN_EQUALS)
					p1->expr.literal->value.integer = (cmp >= 0 ? 1 : 0);
				else
					p1->expr.literal->value.integer = (cmp == 0 ? 1 : 0);

				p1->expr.literal->type = clog_ast_literal_bool;
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_LEFT_SHIFT:
		case CLOG_TOKEN_RIGHT_SHIFT:
			if (!clog_ast_literal_int_promote(p1->expr.literal))
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error_token(parser,NULL," requires an integer",type,NULL,p1->expr.literal->line);
			}
			if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_INTEGER,p2))
			{
				clog_ast_expression_free(parser,p1);
				return 0;
			}
			if (p2->type == clog_ast_expression_literal)
			{
				if (type == CLOG_TOKEN_LEFT_SHIFT)
					p1->expr.literal->value.integer <<= p2->expr.literal->value.integer;
				else
					p1->expr.literal->value.integer >>= p2->expr.literal->value.integer;

				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_PLUS:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_STRING,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				break;

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_real:
			case clog_ast_literal_integer:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_FLOAT,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				if (p2->type != clog_ast_expression_literal)
					return clog_ast_expression_alloc_builtin(parser,expr,type,p2,p1,NULL);
				break;
			}
			if (p2->type == clog_ast_expression_literal)
			{
				switch (p1->expr.literal->type)
				{
				case clog_ast_literal_string:
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
							return clog_ast_out_of_memory(parser);
						}
						memcpy(sz+p1->expr.literal->value.string.len,p2->expr.literal->value.string.str,p2->expr.literal->value.string.len);
						p1->expr.literal->value.string.str = sz;
						p1->expr.literal->value.string.len += p2->expr.literal->value.string.len;
					}
					break;

				case clog_ast_literal_bool:
				case clog_ast_literal_null:
				case clog_ast_literal_real:
				case clog_ast_literal_integer:
					clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal);
					if (p1->expr.literal->type == clog_ast_literal_real)
						p1->expr.literal->value.real += p2->expr.literal->value.real;
					else
						p1->expr.literal->value.integer += p2->expr.literal->value.integer;
					break;
				}
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_MINUS:
		case CLOG_TOKEN_STAR:
		case CLOG_TOKEN_SLASH:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error_token(parser,NULL," requires numbers",type,NULL,p1->expr.literal->line);

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_real:
			case clog_ast_literal_integer:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_FLOAT,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				if (type == CLOG_TOKEN_STAR && p2->type != clog_ast_expression_literal)
					return clog_ast_expression_alloc_builtin(parser,expr,type,p2,p1,NULL);
				break;
			}
			if (p2->type == clog_ast_expression_literal)
			{
				clog_ast_literal_arith_convert(p1->expr.literal,p2->expr.literal);
				if (type == CLOG_TOKEN_MINUS)
				{
					if (p1->expr.literal->type == clog_ast_literal_real)
						p1->expr.literal->value.real -= p2->expr.literal->value.real;
					else
						p1->expr.literal->value.integer -= p2->expr.literal->value.integer;
				}
				else if (type == CLOG_TOKEN_STAR)
				{
					if (p1->expr.literal->type == clog_ast_literal_real)
						p1->expr.literal->value.real *= p2->expr.literal->value.real;
					else
						p1->expr.literal->value.integer *= p2->expr.literal->value.integer;
				}
				else
				{
					if ((p2->expr.literal->type == clog_ast_literal_real && p2->expr.literal->value.real == 0.0) ||
							p2->expr.literal->value.integer == 0)
					{
						clog_ast_expression_free(parser,p1);
						clog_ast_expression_free(parser,p2);
						return clog_syntax_error(parser,"Division by zero",p2->expr.literal->line);
					}
					if (p1->expr.literal->type == clog_ast_literal_real)
						p1->expr.literal->value.real /= p2->expr.literal->value.real;
					else
						p1->expr.literal->value.integer /= p2->expr.literal->value.integer;
				}
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_PERCENT:
			switch (p1->expr.literal->type)
			{
			case clog_ast_literal_string:
			case clog_ast_literal_real:
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				return clog_syntax_error(parser,"% requires integers",p1->expr.literal->line);

			case clog_ast_literal_bool:
			case clog_ast_literal_null:
			case clog_ast_literal_integer:
				if (!clog_ast_expression_alloc_builtin1(parser,&p2,CLOG_TOKEN_INTEGER,p2))
				{
					clog_ast_expression_free(parser,p1);
					return 0;
				}
				break;
			}
			if (p2->type == clog_ast_expression_literal)
			{
				p1->expr.literal->value.integer %= p2->expr.literal->value.integer;
				clog_ast_expression_free(parser,p2);
				*expr = p1;
				return 1;
			}
			break;

		case CLOG_TOKEN_IN:

		default:
			break;
		}
	}

	return clog_ast_expression_alloc_builtin(parser,expr,type,p1,p2,NULL);
}

int clog_ast_expression_alloc_builtin3(struct clog_parser* parser, struct clog_ast_expression** expr, unsigned int type, struct clog_ast_expression* p1, struct clog_ast_expression* p2, struct clog_ast_expression* p3)
{
	*expr = NULL;

	if (!p1 || !p2 || !p3)
	{
		clog_ast_expression_free(parser,p1);
		clog_ast_expression_free(parser,p2);
		clog_ast_expression_free(parser,p3);
		return 0;
	}

	if (type == CLOG_TOKEN_QUESTION)
	{
		if (p1->type != clog_ast_expression_literal)
		{
			if (!clog_ast_expression_alloc_builtin1(parser,&p1,CLOG_TOKEN_TRUE,p1))
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				clog_ast_expression_free(parser,p3);
				return 0;
			}
		}
		else
		{
			clog_ast_literal_bool_promote(p1->expr.literal);
			if (p1->expr.literal->value.integer)
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p3);
				*expr = p2;
			}
			else
			{
				clog_ast_expression_free(parser,p1);
				clog_ast_expression_free(parser,p2);
				*expr = p3;
			}
			return 1;
		}
	}

	return clog_ast_expression_alloc_builtin(parser,expr,type,p1,p2,p3);
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
		return clog_ast_out_of_memory(parser);
	}

	(*expr)->type = clog_ast_expression_call;
	(*expr)->expr.call = clog_malloc(sizeof(struct clog_ast_expression_call));
	if (!(*expr)->expr.call)
	{
		clog_ast_expression_free(parser,call);
		clog_ast_expression_list_free(parser,list);
		clog_free(*expr);
		*expr = NULL;
		return clog_ast_out_of_memory(parser);
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
		return clog_ast_out_of_memory(parser);
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
		return clog_ast_out_of_memory(parser);

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

int clog_ast_expression_list_append(struct clog_parser* parser, struct clog_ast_expression_list** list, struct clog_ast_expression* expr)
{
	while (*list)
		list = &(*list)->next;

	return clog_ast_expression_list_alloc(parser,list,expr);
}

static void clog_ast_statement_free_block(struct clog_parser* parser, struct clog_ast_block* block)
{
	if (block)
	{
		struct clog_ast_variable* v;
		for (v=block->locals;v;)
		{
			struct clog_ast_variable* n = v->next;
			clog_free(v->id.str);
			clog_free(v);
			v = n;
		}
		for (v=block->externs;v;)
		{
			struct clog_ast_variable* n = v->next;
			clog_free(v->id.str);
			clog_free(v);
			v = n;
		}
		clog_ast_statement_list_free(parser,block->stmts);
		clog_free(block);
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
		case clog_ast_statement_break:
		case clog_ast_statement_continue:
			clog_ast_expression_free(parser,stmt->stmt.expression);
			break;

		case clog_ast_statement_block:
			clog_ast_statement_free_block(parser,stmt->stmt.block);
			break;

		case clog_ast_statement_declaration:
		case clog_ast_statement_constant:
			clog_ast_literal_free(parser,stmt->stmt.declaration);
			break;

		case clog_ast_statement_if:
			clog_ast_expression_free(parser,stmt->stmt.if_stmt->condition);
			clog_ast_statement_free_block(parser,stmt->stmt.if_stmt->true_block);
			clog_ast_statement_free_block(parser,stmt->stmt.if_stmt->false_block);
			clog_free(stmt->stmt.if_stmt);
			break;

		case clog_ast_statement_do:
			clog_ast_expression_free(parser,stmt->stmt.do_stmt->condition);
			clog_ast_statement_free_block(parser,stmt->stmt.do_stmt->loop_block);
			clog_free(stmt->stmt.do_stmt);
			break;

		case clog_ast_statement_while:
			clog_ast_expression_free(parser,stmt->stmt.while_stmt->condition);
			clog_ast_statement_free_block(parser,stmt->stmt.while_stmt->loop_block);
			clog_ast_statement_list_free(parser,stmt->stmt.while_stmt->pre);
			clog_free(stmt->stmt.while_stmt);
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
		return clog_ast_out_of_memory(parser);

	(*list)->stmt = clog_malloc(sizeof(struct clog_ast_statement));
	if (!(*list)->stmt)
	{
		clog_free(*list);
		*list = NULL;
		return clog_ast_out_of_memory(parser);
	}
	(*list)->next = NULL;
	(*list)->stmt->type = type;

	if (type == clog_ast_statement_break || type == clog_ast_statement_continue)
	{
		struct clog_ast_literal* lit_null;
		if (!clog_ast_literal_alloc(parser,&lit_null,NULL) ||
				!clog_ast_expression_alloc_literal(parser,&(*list)->stmt->stmt.expression,lit_null))
		{
			clog_free(*list);
			*list = NULL;
			return 0;
		}
		lit_null->line = parser->line;
	}

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

static int clog_ast_variable_alloc(struct clog_parser* parser, struct clog_ast_variable** var, const struct clog_string* str)
{
	*var = clog_malloc(sizeof(struct clog_ast_variable));
	if (!(*var))
		return clog_ast_out_of_memory(parser);

	memset(*var,0,sizeof(struct clog_ast_variable));
	(*var)->id.len = str->len;
	if ((*var)->id.len)
	{
		(*var)->id.str = clog_malloc((*var)->id.len + 1);
		if (!(*var)->id.str)
		{
			clog_free(*var);
			*var = NULL;
			return clog_ast_out_of_memory(parser);
		}
		memcpy((*var)->id.str,str->str,(*var)->id.len);
		(*var)->id.str[(*var)->id.len] = 0;
	}
	return 1;
}

static int clog_ast_bind_expression(struct clog_parser* parser, struct clog_ast_block* block, struct clog_ast_expression* expr, int assignment)
{
	if (!expr)
		return 1;

	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		{
			struct clog_ast_variable* var;
			for (var = block->locals;var;var = var->next)
			{
				if (clog_ast_string_compare(&var->id,&expr->expr.identifier->value.string) == 0)
					break;
			}
			if (!var)
			{
				for (var = block->externs;var;var = var->next)
				{
					if (clog_ast_string_compare(&var->id,&expr->expr.identifier->value.string) == 0)
						break;
				}
				if (!var)
				{
					if (!clog_ast_variable_alloc(parser,&var,&expr->expr.identifier->value.string))
						return 0;

					var->next = block->externs;
					block->externs = var;
				}
			}

			if (assignment)
			{
				if (var->constant)
					return clog_syntax_error(parser,"Assignment to constant",expr->expr.identifier->line);

				var->assigned = 1;
			}

			clog_ast_literal_free(parser,expr->expr.identifier);
			expr->type = clog_ast_expression_variable;
			expr->expr.variable = var;
		}
		break;

	case clog_ast_expression_literal:
	case clog_ast_expression_variable:
		break;

	case clog_ast_expression_builtin:
		switch (expr->expr.builtin->type)
		{
		case CLOG_TOKEN_DOUBLE_PLUS:
		case CLOG_TOKEN_DOUBLE_MINUS:
			if (!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[0],1) ||
					!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[1],1))
			{
				return 0;
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
		case CLOG_TOKEN_CARET_ASSIGN:
		case CLOG_TOKEN_BAR_ASSIGN:
			if (!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[0],1) ||
					!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[1],0))
			{
				return 0;
			}
			break;

		default:
			if (!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[0],0) ||
					!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[1],0) ||
					!clog_ast_bind_expression(parser,block,expr->expr.builtin->args[2],0))
			{
				return 0;
			}
			break;
		}
		break;

	case clog_ast_expression_call:
		{
			struct clog_ast_expression_list* e = expr->expr.call->params;
			if (!clog_ast_bind_expression(parser,block,expr->expr.call->expr,0))
				return 0;

			for (;e;e = e->next)
			{
				if (!clog_ast_bind_expression(parser,block,e->expr,0))
					return 0;
			}
		}
		break;
	}
	return 1;
}

static int clog_ast_bind_block(struct clog_parser* parser, struct clog_ast_block* outer_block, struct clog_ast_block* inner_block)
{
	if (!inner_block)
		return 1;

	/* Match externs to outer_block's locals */
	struct clog_ast_variable* var1;
	for (var1 = inner_block->externs; var1; var1 = var1->next)
	{
		struct clog_ast_variable* var2;
		for (var2 = outer_block->locals; var2; var2 = var2->next)
		{
			if (clog_ast_string_compare(&var1->id,&var2->id) == 0)
			{
				var1->up = var2;
				break;
			}
		}
		if (!var2)
		{
			/* Match to outer_block's externs */
			for (var2 = outer_block->externs; var2; var2 = var2->next)
			{
				if (clog_ast_string_compare(&var1->id,&var2->id) == 0)
				{
					var1->up = var2;
					break;
				}
			}
			if (!var2)
			{
				if (!clog_ast_variable_alloc(parser,&var2,&var1->id))
					return 0;

				var2->next = outer_block->externs;
				outer_block->externs = var2;
			}
		}

		if (var1->assigned)
			var2->assigned = 1;
	}
	return 1;
}

static int clog_ast_bind(struct clog_parser* parser, struct clog_ast_block* block, struct clog_ast_statement_list** l)
{
	while (*l)
	{
		switch ((*l)->stmt->type)
		{
		case clog_ast_statement_expression:
			if (!clog_ast_bind_expression(parser,block,(*l)->stmt->stmt.expression,0))
				return 0;
			break;

		case clog_ast_statement_declaration:
		case clog_ast_statement_constant:
			{
				struct clog_ast_variable* var;

				/* Next statement is the initialiser, bind it first */
				if (!clog_ast_bind_expression(parser,block,(*l)->next->stmt->stmt.expression->expr.builtin->args[1],0))
					return 0;

				/* Search for a duplicate */
				for (var = block->locals;var;var = var->next)
				{
					if (clog_ast_string_compare(&var->id,&(*l)->stmt->stmt.declaration->value.string) == 0)
						return clog_syntax_error(parser,"Variable already declared",(*l)->stmt->stmt.declaration->line);
				}

				/* Add to the block's locals */
				if (!clog_ast_variable_alloc(parser,&var,&(*l)->stmt->stmt.declaration->value.string))
					return 0;

				var->constant = ((*l)->stmt->type == clog_ast_statement_constant ? 1 : 0);
				var->next = block->locals;
				block->locals = var;

				{
					struct clog_ast_statement_list* n = (*l)->next;
					(*l)->next = NULL;
					clog_ast_statement_list_free(parser,*l);
					*l = n;
				}
			}
			break;

		case clog_ast_statement_block:
			if (!clog_ast_bind_block(parser,block,(*l)->stmt->stmt.block))
				return 0;
			break;

		case clog_ast_statement_if:
			if (!clog_ast_bind_expression(parser,block,(*l)->stmt->stmt.if_stmt->condition,0) ||
					!clog_ast_bind_block(parser,block,(*l)->stmt->stmt.if_stmt->true_block) ||
					!clog_ast_bind_block(parser,block,(*l)->stmt->stmt.if_stmt->false_block))
			{
				return 0;
			}
			break;

		case clog_ast_statement_do:
			if (!clog_ast_bind_block(parser,block,(*l)->stmt->stmt.do_stmt->loop_block) ||
					!clog_ast_bind_expression(parser,block,(*l)->stmt->stmt.do_stmt->condition,0))
			{
				return 0;
			}
			break;

		case clog_ast_statement_while:
			if (!clog_ast_bind(parser,block,&(*l)->stmt->stmt.while_stmt->pre) ||
					!clog_ast_bind_expression(parser,block,(*l)->stmt->stmt.while_stmt->condition,0) ||
					!clog_ast_bind_block(parser,block,(*l)->stmt->stmt.while_stmt->loop_block))
			{
				return 0;
			}
			break;

		case clog_ast_statement_break:
		case clog_ast_statement_continue:
			if ((*l)->next)
			{
				clog_syntax_error(parser,"Warning: Unreachable code",(*l)->stmt->stmt.expression->expr.literal->line);
				clog_ast_statement_list_free(parser,(*l)->next);
			}
			return 1;

		case clog_ast_statement_return:
			if (!clog_ast_bind_expression(parser,block,(*l)->stmt->stmt.expression,0))
				return 0;

			if ((*l)->next)
			{
				clog_syntax_error(parser,"Warning: Unreachable code",(*l)->stmt->stmt.expression->expr.literal->line);
				clog_ast_statement_list_free(parser,(*l)->next);
			}
			return 1;
		}

		l = &(*l)->next;
	}

	return 1;
}

int clog_ast_statement_list_alloc_block(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* block_list)
{
	struct clog_ast_block* block;

	*list = NULL;

	if (!block_list)
		return 1;

	/* Check for block containing only 1 block */
	if (block_list->stmt->type == clog_ast_statement_block && block_list->next == NULL)
	{
		*list = block_list;
		return 1;
	}

	block = clog_malloc(sizeof(struct clog_ast_block));
	if (!block)
	{
		clog_ast_statement_list_free(parser,block_list);
		return clog_ast_out_of_memory(parser);
	}

	block->locals = NULL;
	block->externs = NULL;
	block->stmts = block_list;

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_block))
	{
		clog_ast_statement_list_free(parser,block_list);
		return 0;
	}

	(*list)->stmt->stmt.block = block;

	if (!clog_ast_bind(parser,block,&block->stmts))
	{
		clog_ast_statement_list_free(parser,*list);
		*list = NULL;
		return 0;
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

	/* Transform var x; => var x; x = null; */
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

	if (!clog_ast_expression_alloc_builtin2(parser,&assign_expr,CLOG_TOKEN_ASSIGN,id_expr,init))
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

	/* If we have no result statements replace with condition */
	if (!true_stmt && !false_stmt)
	{
		*list = cond;
		return 1;
	}

	/* If we have no true_stmt, negate the condition and swap */
	if (!true_stmt)
	{
		if (!clog_ast_expression_alloc_builtin1(parser,&cond->stmt->stmt.expression,CLOG_TOKEN_EXCLAMATION,cond->stmt->stmt.expression))
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,false_stmt);
			return 0;
		}

		true_stmt = false_stmt;
		false_stmt = NULL;
	}

	/* Rewrite to force compound statements: if (x) var i;  =>  if (x) { var i; } */
	if (!clog_ast_statement_list_alloc_block(parser,&true_stmt,true_stmt))
	{
		clog_ast_statement_list_free(parser,cond);
		clog_ast_statement_list_free(parser,false_stmt);
		return 0;
	}
	if (false_stmt)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&false_stmt,false_stmt))
		{
			clog_ast_statement_list_free(parser,cond);
			clog_ast_statement_list_free(parser,true_stmt);
			return 0;
		}
	}

	/* Check to see if we have an expression of the kind: if (var x = 12) */
	if (cond->stmt->type == clog_ast_statement_declaration || cond->stmt->type == clog_ast_statement_constant)
	{
		struct clog_ast_expression* cond_expr;
		struct clog_ast_statement_list* cond2;

		/* We must check to see if an identical declaration occurs in the outermost block of true_stmt and false_stmt */
		struct clog_ast_variable* v = true_stmt->stmt->stmt.block->locals;
		for (;v;v = v->next)
		{
			if (clog_ast_string_compare(&v->id,&cond->stmt->stmt.declaration->value.string) == 0)
			{
				unsigned long line = cond->stmt->stmt.declaration->line;
				clog_ast_statement_list_free(parser,cond);
				clog_ast_statement_list_free(parser,true_stmt);
				clog_ast_statement_list_free(parser,false_stmt);
				return clog_syntax_error(parser,"Variable already declared",line);
			}
		}

		if (false_stmt)
		{
			for (v = false_stmt->stmt->stmt.block->locals;v;v = v->next)
			{
				if (clog_ast_string_compare(&v->id,&cond->stmt->stmt.declaration->value.string) == 0)
				{
					unsigned long line = cond->stmt->stmt.declaration->line;
					clog_ast_statement_list_free(parser,cond);
					clog_ast_statement_list_free(parser,true_stmt);
					clog_ast_statement_list_free(parser,false_stmt);
					return clog_syntax_error(parser,"Variable already declared",line);
				}
			}
		}

		/* Now rewrite if (var x = 1) ... => { var x = 1; if ((bool)x) ... } */
		if (!clog_ast_expression_clone(parser,&cond_expr,cond->next->stmt->stmt.expression->expr.builtin->args[0]) ||
				!clog_ast_statement_list_alloc_expression(parser,&cond2,cond_expr) ||
				!clog_ast_statement_list_alloc_if(parser,&cond->next->next,cond2,true_stmt,false_stmt))
		{
			clog_ast_statement_list_free(parser,cond);
			return 0;
		}

		return clog_ast_statement_list_alloc_block(parser,list,cond);
	}

	/* Check for constant condition */
	if (cond->stmt->stmt.expression->type == clog_ast_expression_literal)
	{
		clog_ast_literal_bool_promote(cond->stmt->stmt.expression->expr.literal);
		if (cond->stmt->stmt.expression->expr.literal->value.integer)
		{
			clog_ast_statement_list_free(parser,false_stmt);
			*list = true_stmt;
		}
		else
		{
			clog_ast_statement_list_free(parser,true_stmt);
			*list = false_stmt;
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
		return clog_ast_out_of_memory(parser);
	}

	if (!clog_ast_expression_alloc_builtin1(parser,&(*list)->stmt->stmt.if_stmt->condition,CLOG_TOKEN_TRUE,cond->stmt->stmt.expression))
	{
		cond->stmt->stmt.expression = NULL;
		clog_ast_statement_list_free(parser,cond);
		clog_ast_statement_list_free(parser,true_stmt);
		clog_ast_statement_list_free(parser,false_stmt);
		clog_free((*list)->stmt);
		clog_free(*list);
		*list = NULL;
		return 0;
	}

	cond->stmt->stmt.expression = NULL;
	clog_ast_statement_list_free(parser,cond);

	(*list)->stmt->stmt.if_stmt->true_block = true_stmt->stmt->stmt.block;
	true_stmt->stmt->stmt.block = NULL;
	clog_ast_statement_list_free(parser,true_stmt);

	(*list)->stmt->stmt.if_stmt->false_block = NULL;
	if (false_stmt)
	{
		(*list)->stmt->stmt.if_stmt->false_block = false_stmt->stmt->stmt.block;
		false_stmt->stmt->stmt.block = NULL;
		clog_ast_statement_list_free(parser,false_stmt);
	}

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
	if (loop_stmt)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&loop_stmt,loop_stmt))
		{
			clog_ast_expression_free(parser,cond);
			return 0;
		}
	}

	/* Check for constant false condition */
	if (cond->type == clog_ast_expression_literal)
	{
		clog_ast_literal_bool_promote(cond->expr.literal);
		if (!cond->expr.literal->value.integer)
		{
			clog_ast_expression_free(parser,cond);
			*list = loop_stmt;
			return 1;
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
		return clog_ast_out_of_memory(parser);
	}

	if (!clog_ast_expression_alloc_builtin1(parser,&(*list)->stmt->stmt.do_stmt->condition,CLOG_TOKEN_TRUE,cond))
	{
		clog_ast_statement_list_free(parser,loop_stmt);
		clog_free((*list)->stmt);
		clog_free(*list);
		*list = NULL;
		return 0;
	}

	(*list)->stmt->stmt.do_stmt->loop_block = NULL;
	if (loop_stmt)
	{
		(*list)->stmt->stmt.do_stmt->loop_block = loop_stmt->stmt->stmt.block;
		loop_stmt->stmt->stmt.block = NULL;
		clog_ast_statement_list_free(parser,loop_stmt);
	}
	return 1;
}

int clog_ast_statement_list_alloc_while(struct clog_parser* parser, struct clog_ast_statement_list** list, struct clog_ast_statement_list* cond_stmt, struct clog_ast_statement_list* loop_stmt)
{
	*list = NULL;

	if (!cond_stmt)
	{
		clog_ast_statement_list_free(parser,loop_stmt);
		return 0;
	}

	/* Rewrite to force compound statements: while (1) var i  =>  while (1) { var i; } */
	if (loop_stmt)
	{
		if (!clog_ast_statement_list_alloc_block(parser,&loop_stmt,loop_stmt))
		{
			clog_ast_statement_list_free(parser,cond_stmt);
			return 0;
		}
	}

	if (!clog_ast_statement_list_alloc(parser,list,clog_ast_statement_while))
	{
		clog_ast_statement_list_free(parser,cond_stmt);
		clog_ast_statement_list_free(parser,loop_stmt);
		return 0;
	}

	(*list)->stmt->stmt.while_stmt = clog_malloc(sizeof(struct clog_ast_statement_while));
	if (!(*list)->stmt->stmt.while_stmt)
	{
		clog_ast_statement_list_free(parser,cond_stmt);
		clog_ast_statement_list_free(parser,loop_stmt);
		clog_free((*list)->stmt);
		clog_free(*list);
		*list = NULL;
		return clog_ast_out_of_memory(parser);
	}

	if (cond_stmt->stmt->type == clog_ast_statement_declaration || cond_stmt->stmt->type == clog_ast_statement_constant)
	{
		/* Now rewrite if (var x = 1) ... => { var x = 1; while ((bool)x) ... } */
		if (!clog_ast_expression_clone(parser,&(*list)->stmt->stmt.while_stmt->condition,cond_stmt->next->stmt->stmt.expression->expr.builtin->args[0]) ||
				!clog_ast_expression_alloc_builtin1(parser,&(*list)->stmt->stmt.while_stmt->condition,CLOG_TOKEN_TRUE,(*list)->stmt->stmt.while_stmt->condition))
		{
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			clog_free((*list)->stmt);
			clog_free(*list);
			*list = NULL;
			return 0;
		}

		(*list)->stmt->stmt.while_stmt->pre = cond_stmt;
	}
	else
	{
		if (!clog_ast_expression_clone(parser,&(*list)->stmt->stmt.while_stmt->condition,cond_stmt->stmt->stmt.expression) ||
				!clog_ast_expression_alloc_builtin1(parser,&(*list)->stmt->stmt.while_stmt->condition,CLOG_TOKEN_TRUE,(*list)->stmt->stmt.while_stmt->condition))
		{
			clog_ast_statement_list_free(parser,cond_stmt);
			clog_ast_statement_list_free(parser,loop_stmt);
			clog_free((*list)->stmt);
			clog_free(*list);
			*list = NULL;
			return 0;
		}

		cond_stmt->stmt->stmt.expression = NULL;
		clog_ast_statement_list_free(parser,cond_stmt);

		(*list)->stmt->stmt.while_stmt->pre = NULL;
	}

	/* Check for constant false condition */
	if ((*list)->stmt->stmt.while_stmt->condition->type == clog_ast_expression_literal)
	{
		clog_ast_literal_bool_promote((*list)->stmt->stmt.while_stmt->condition->expr.literal);
		if (!(*list)->stmt->stmt.while_stmt->condition->expr.literal->value.integer)
		{
			struct clog_ast_statement_list* l = (*list)->stmt->stmt.while_stmt->pre;
			clog_ast_statement_list_free(parser,loop_stmt);
			clog_ast_expression_free(parser,(*list)->stmt->stmt.while_stmt->condition);
			clog_free((*list)->stmt);
			clog_free(*list);
			return clog_ast_statement_list_alloc_block(parser,list,l);
		}
	}

	/* Create the loop */
	(*list)->stmt->stmt.while_stmt->loop_block = NULL;
	if (loop_stmt)
	{
		(*list)->stmt->stmt.while_stmt->loop_block = loop_stmt->stmt->stmt.block;
		loop_stmt->stmt->stmt.block = NULL;
		clog_ast_statement_list_free(parser,loop_stmt);
	}

	/* Return wrapped up in a block */
	return clog_ast_statement_list_alloc_block(parser,list,*list);
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
			(*list)->stmt->stmt.block->stmts = clog_ast_statement_list_append(parser,init_stmt,(*list)->stmt->stmt.block->stmts);
		else
		{
			*list = clog_ast_statement_list_append(parser,init_stmt,*list);

			if (!clog_ast_statement_list_alloc_block(parser,list,*list))
				return 0;
		}
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
	printf("\n");
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

	case clog_ast_expression_variable:
		printf("%.*s",(int)expr->expr.variable->id.len,expr->expr.variable->id.str);
		break;

	case clog_ast_expression_builtin:
		if (expr->expr.builtin->args[1] && expr->expr.builtin->args[0])
			__dump_expr_b(expr->expr.builtin->args[0]);

		switch (expr->expr.builtin->type)
		{
		case CLOG_TOKEN_DOUBLE_PLUS:
			printf("++");
			if (expr->expr.builtin->args[1])
				__dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_DOUBLE_MINUS:
			printf("--");
			if (expr->expr.builtin->args[1])
				__dump_expr_b(expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_QUESTION:
			printf(" ? ");
			__dump_expr_b(expr->expr.builtin->args[1]);
			printf(" : ");
			__dump_expr_b(expr->expr.builtin->args[2]);
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

static void __dump_block(size_t indent, const struct clog_ast_block* block)
{
	struct clog_ast_variable* v;
	printf("{");
	__dump_indent(indent);
	printf("  Locals = [");
	for (v=block->locals;v;v=v->next)
		printf("%s%s ",v->id.str,v->constant ? "!" : "");
	printf("], Externs = [");
	for (v=block->externs;v;v=v->next)
		printf("%s%s ",v->id.str,v->assigned ? "*" : "");
	printf("]");
	__dump(indent+1,block->stmts);
	__dump_indent(indent);
	printf("}");
}

static void __dump(size_t indent, const struct clog_ast_statement_list* list)
{
	for (;list;list = list->next)
	{
		if (indent != -1)
			__dump_indent(indent);

		switch (list->stmt->type)
		{
		case clog_ast_statement_block:
			__dump_block(indent,list->stmt->stmt.block);
			break;

		case clog_ast_statement_declaration:
			printf("var %.*s;",(int)list->stmt->stmt.declaration->value.string.len,list->stmt->stmt.declaration->value.string.str);
			break;

		case clog_ast_statement_constant:
			printf("const %.*s;",(int)list->stmt->stmt.declaration->value.string.len,list->stmt->stmt.declaration->value.string.str);
			break;

		case clog_ast_statement_expression:
			__dump_expr(list->stmt->stmt.expression);
			printf(";");
			break;

		case clog_ast_statement_return:
			printf("return ");
			__dump_expr(list->stmt->stmt.expression);
			printf(";");
			break;

		case clog_ast_statement_break:
			printf("break;");
			break;

		case clog_ast_statement_continue:
			printf("continue;");
			break;

		case clog_ast_statement_if:
			printf("if (");
			__dump_expr(list->stmt->stmt.if_stmt->condition);
			printf(")");
			if (list->stmt->stmt.if_stmt->true_block)
			{
				__dump_indent(indent);
				__dump_block(indent,list->stmt->stmt.if_stmt->true_block);
			}
			else
			{
				__dump_indent(indent+1);
				printf(";");
			}
			if (list->stmt->stmt.if_stmt->false_block)
			{
				__dump_indent(indent);
				printf("else");
				__dump_indent(indent);
				__dump_block(indent,list->stmt->stmt.if_stmt->false_block);
			}
			break;

		case clog_ast_statement_do:
			printf("do");
			if (list->stmt->stmt.do_stmt->loop_block)
			{
				__dump_indent(indent);
				__dump_block(indent,list->stmt->stmt.do_stmt->loop_block);
			}
			else
			{
				__dump_indent(indent+1);
				printf(";");
			}
			__dump_indent(indent);
			printf("while (");
			__dump_expr(list->stmt->stmt.do_stmt->condition);
			printf(")");
			break;

		case clog_ast_statement_while:
			printf("while (");
			if (list->stmt->stmt.while_stmt->pre)
				__dump(-1,list->stmt->stmt.while_stmt->pre);
			__dump_expr(list->stmt->stmt.while_stmt->condition);
			printf(")");
			if (list->stmt->stmt.while_stmt->loop_block)
			{
				__dump_indent(indent);
				__dump_block(indent,list->stmt->stmt.while_stmt->loop_block);
			}
			else
			{
				__dump_indent(indent+1);
				printf(";");
			}
			break;
		}
	}
}








int clog_cfg_construct(const struct clog_ast_block* ast_block);

/* External functions defined by ragel and lemon */
int clog_tokenize(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param, struct clog_parser* parser, void* lemon);
void* clog_parserAlloc(void* (*)(size_t));
void clog_parserFree(void *p, void (*)(void*));
void clog_parser(void* lemon, int type, struct clog_token* tok, struct clog_parser* parser);

int clog_parse(int (*rd_fn)(void* p, unsigned char* buf, size_t* len), void* rd_param)
{
	int retval = 0;
	struct clog_parser parser = {0};

	parser.line = 1;
	parser.reduce = 0;

	void* lemon = clog_parserAlloc(&clog_malloc);
	if (!lemon)
		return -1;

	/*clog_parserTrace(stdout,"lemon: ");*/

	retval = clog_tokenize(rd_fn,rd_param,&parser,lemon);

	clog_parser(lemon,0,NULL,&parser);

	clog_parserFree(lemon,&clog_free);

	__dump(0,parser.pgm);

	if (retval && !parser.failed)
	{
		printf("\n\nSuccess!\n");

		{ void* TODO; /* Check for undeclared externs */ }

		clog_cfg_construct(parser.pgm->stmt->stmt.block);
	}

	clog_ast_statement_list_free(&parser,parser.pgm);

	return retval;
}
