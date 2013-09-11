/*
 * clog_ast_reduce.c
 *
 *  Created on: 9 Sep 2013
 *      Author: taylorr
 */

#include "clog_parser.h"
#include "clog_ast.h"

#include <string.h>

struct clog_ast_reduction
{
	const struct clog_ast_literal* id;
	struct clog_ast_literal* value;
	struct clog_ast_expression** init_expr;
	int reduced;
	int referenced;
};

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

static int clog_ast_literal_add(struct clog_parser* parser, struct clog_ast_literal* p1, struct clog_ast_literal* p2)
{
	if (p1->type == clog_ast_literal_string && p2->type == clog_ast_literal_string)
	{
		if (!p1->value.string.len)
		{
			/* Swap via p3 */
			struct clog_ast_literal* p3 = p2;
			p2 = p1;
			p1 = p3;
		}

		if (p2->value.string.len)
		{
			unsigned char* sz = clog_realloc(p1->value.string.str,p1->value.string.len + p2->value.string.len);
			if (!sz)
				return clog_ast_out_of_memory(parser);

			memcpy(sz+p1->value.string.len,p2->value.string.str,p2->value.string.len);
			p1->value.string.str = sz;
			p1->value.string.len += p2->value.string.len;
		}
		return 2;
	}
	else if (p1->type != clog_ast_literal_string && p2->type != clog_ast_literal_string)
	{
		if (!clog_ast_literal_arith_convert(p1,p2))
			return clog_syntax_error(parser,"+ requires numbers or strings",p1->line);

		if (p1->type == clog_ast_literal_real)
			p1->value.real += p2->value.real;
		else
			p1->value.integer += p2->value.integer;

		return 2;
	}
	return 1;
}

static int clog_ast_expression_eval(struct clog_parser* parser, struct clog_ast_literal** v, const struct clog_ast_expression* expr, struct clog_ast_reduction* reduction)
{
	*v = NULL;

	if (expr)
	{
		switch (expr->type)
		{
		case clog_ast_expression_identifier:
			if (clog_ast_literal_id_compare(reduction->id,expr->expr.identifier) == 0)
			{
				reduction->referenced = 1;
				return clog_ast_literal_clone(parser,v,reduction->value);
			}
			break;

		case clog_ast_expression_literal:
			return clog_ast_literal_clone(parser,v,expr->expr.literal);

		case clog_ast_expression_call:
			break;

		case clog_ast_expression_builtin:
			switch (expr->expr.builtin->type)
			{
			case CLOG_TOKEN_ASSIGN:
				if (clog_ast_literal_id_compare(reduction->id,expr->expr.identifier) == 0)
					return clog_ast_expression_eval(parser,v,expr->expr.builtin->args[1],reduction);
				break;

			case CLOG_TOKEN_COMMA:
				return clog_ast_expression_eval(parser,v,expr->expr.builtin->args[1],reduction);

			default:
				break;
			}
			break;
		}
	}
	return 1;
}

static int clog_ast_expression_list_reduce(struct clog_parser* parser, struct clog_ast_expression_list* expr, struct clog_ast_reduction* reduction);
static int clog_ast_expression_reduce_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction);

static int clog_ast_expression_reduce(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	if (!*expr)
		return 1;

	switch ((*expr)->type)
	{
	case clog_ast_expression_identifier:
		if (clog_ast_literal_id_compare(reduction->id,(*expr)->expr.identifier) == 0)
		{
			if (reduction->value)
			{
				struct clog_ast_literal* new_lit;
				if (!clog_ast_literal_clone(parser,&new_lit,reduction->value))
					return 0;

				clog_ast_expression_free(parser,*expr);
				if (!clog_ast_expression_alloc_literal(parser,expr,new_lit))
					return 0;

				reduction->reduced = 1;
			}

			reduction->referenced = 1;
		}
		return 1;

	case clog_ast_expression_literal:
		return 1;

	case clog_ast_expression_builtin:
		return clog_ast_expression_reduce_builtin(parser,expr,reduction);

	case clog_ast_expression_call:
		if (!clog_ast_expression_reduce(parser,&(*expr)->expr.call->expr,reduction))
			return 0;

		return clog_ast_expression_list_reduce(parser,(*expr)->expr.call->params,reduction);
	}

	return 0;
}

static int clog_ast_expression_reduce_comma(struct clog_ast_expression** expr, size_t idx)
{
	struct clog_ast_expression* e = (*expr)->expr.builtin->args[idx];
	if (e && e->type == clog_ast_expression_builtin && e->expr.builtin->type == CLOG_TOKEN_COMMA)
	{
		(*expr)->expr.builtin->args[idx] = e->expr.builtin->args[1];
		e->expr.builtin->args[1] = *expr;
		*expr = e;
		return 1;
	}
	return 0;
}

static void clog_ast_expression_reduce_builtin_comma(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	int reduced = reduction->reduced;

	do
	{
		reduction->reduced = 0;

		switch ((*expr)->expr.builtin->type)
		{
		case CLOG_TOKEN_COMMA:
			break;

		case CLOG_TOKEN_AND:
		case CLOG_TOKEN_OR:
		case CLOG_TOKEN_QUESTION:
			if (clog_ast_expression_reduce_comma(expr,0))
				reduction->reduced = 1;
			break;

		default:
			if (clog_ast_expression_reduce_comma(expr,0) || clog_ast_expression_reduce_comma(expr,1))
				reduction->reduced = 1;
			break;
		}

		if (reduction->reduced)
			reduced = 1;
	}
	while (reduction->reduced);

	reduction->reduced = reduced;
}

static int clog_ast_expression_reduce_assign(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	struct clog_ast_expression* e;
	struct clog_ast_literal* p;

	if ((*expr)->expr.builtin->type == CLOG_TOKEN_DOUBLE_PLUS || (*expr)->expr.builtin->type == CLOG_TOKEN_DOUBLE_MINUS)
	{
		if ((*expr)->expr.builtin->args[0])
		{
			if ((*expr)->expr.builtin->args[0]->type != clog_ast_expression_identifier && !clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction))
				return 0;

			if (clog_ast_literal_id_compare(reduction->id,(*expr)->expr.builtin->args[0]->expr.identifier) != 0)
				return 1;

			/* Rewrite postfix e++ => (++e,value) */
			if (!clog_ast_literal_clone(parser,&p,reduction->value) || !clog_ast_expression_alloc_literal(parser,&e,p))
				return 0;

			(*expr)->expr.builtin->args[1] = (*expr)->expr.builtin->args[0];
			(*expr)->expr.builtin->args[0] = NULL;

			reduction->reduced = 1;
			return clog_ast_expression_alloc_builtin2(parser,expr,CLOG_TOKEN_COMMA,*expr,e);
		}

		if ((*expr)->expr.builtin->args[1]->type != clog_ast_expression_identifier && !clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction))
			return 0;

		if (clog_ast_literal_id_compare(reduction->id,(*expr)->expr.builtin->args[1]->expr.identifier) != 0)
			return 1;

		reduction->referenced = 1;

		if (!reduction->value)
			return 1;

		if ((*expr)->expr.builtin->type == CLOG_TOKEN_DOUBLE_PLUS)
		{
			/* Prefix ++ */
			if (reduction->value->type == clog_ast_literal_real)
				++reduction->value->value.real;
			else
			{
				if (!clog_ast_literal_int_promote(reduction->value))
					return 1;

				++reduction->value->value.integer;
			}
		}
		else
		{
			/* Prefix -- */
			if (reduction->value->type == clog_ast_literal_real)
				--reduction->value->value.real;
			else
			{
				if (!clog_ast_literal_int_promote(reduction->value))
					return 1;

				--reduction->value->value.integer;
			}
		}

		if (!clog_ast_literal_clone(parser,&p,reduction->value) || !clog_ast_expression_alloc_literal(parser,&e,p))
			return 0;

		(*expr)->expr.builtin->type = CLOG_TOKEN_ASSIGN;
		(*expr)->expr.builtin->args[0] = (*expr)->expr.builtin->args[1];
		(*expr)->expr.builtin->args[1] = e;
		reduction->reduced = 1;
		return 1;
	}

	if (((*expr)->expr.builtin->args[0]->type != clog_ast_expression_identifier &&
			!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction)) ||
			!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction))
	{
		return 0;
	}
	if (!clog_ast_expression_eval(parser,&p,(*expr)->expr.builtin->args[1],reduction))
		return 0;

	if (clog_ast_literal_id_compare(reduction->id,(*expr)->expr.builtin->args[0]->expr.identifier) != 0)
	{
		clog_ast_literal_free(parser,p);
		return 1;
	}

	if ((*expr)->expr.builtin->type == CLOG_TOKEN_ASSIGN)
	{
		if (clog_ast_literal_compare(reduction->value,p) == 0)
		{
			/* Replace the expression with a reference */
			e = (*expr)->expr.builtin->args[0];
			(*expr)->expr.builtin->args[0] = NULL;
			clog_ast_expression_free(parser,*expr);
			*expr = e;
			reduction->reduced = 1;
			reduction->referenced = 1;
			clog_ast_literal_free(parser,p);
			return 1;
		}

		clog_ast_literal_free(parser,reduction->value);
		if (!clog_ast_literal_clone(parser,&reduction->value,p))
		{
			clog_ast_literal_free(parser,p);
			return 0;
		}

		if (!reduction->referenced && reduction->init_expr)
		{
			if (p)
			{
				/* Update the current init_expr to this value and replace with a reference */
				e = (*expr)->expr.builtin->args[0];
				(*expr)->expr.builtin->args[0] = NULL;
				clog_ast_expression_free(parser,*expr);
				*expr = e;
				reduction->reduced = 1;
				reduction->referenced = 1;

				clog_ast_expression_free(parser,(*reduction->init_expr)->expr.builtin->args[1]);
				if (!clog_ast_expression_alloc_literal(parser,&(*reduction->init_expr)->expr.builtin->args[1],p))
					return 0;

				(*reduction->init_expr)->expr.builtin->line = clog_ast_expression_line(*expr);
				return 1;
			}
			else
			{
				/* Update the current init_expr to null */
				struct clog_ast_literal* lit_null;
				if (!clog_ast_literal_alloc(parser,&lit_null,NULL))
					return 0;

				if ((*reduction->init_expr)->expr.builtin->args[1]->type != clog_ast_expression_literal ||
						clog_ast_literal_compare(lit_null,(*reduction->init_expr)->expr.builtin->args[1]->expr.literal) != 0)
				{
					clog_ast_expression_free(parser,(*reduction->init_expr)->expr.builtin->args[1]);
					(*reduction->init_expr)->expr.builtin->args[1] = NULL;

					if (!clog_ast_expression_alloc_literal(parser,&(*reduction->init_expr)->expr.builtin->args[1],lit_null))
						return 0;

					reduction->reduced = 1;
					lit_null = NULL;
				}
				clog_ast_literal_free(parser,lit_null);

				(*reduction->init_expr)->expr.builtin->args[1]->expr.literal->type = clog_ast_literal_null;
			}
		}
		else
			clog_ast_literal_free(parser,p);

		/* This is now the latest init_expr */
		reduction->init_expr = expr;
		reduction->referenced = 1;
		return 1;
	}

	reduction->referenced = 1;

	if (!p || !reduction->value)
	{
		clog_ast_literal_free(parser,p);
		clog_ast_literal_free(parser,reduction->value);
		reduction->value = NULL;
		return 1;
	}

	switch ((*expr)->expr.builtin->type)
	{
	case CLOG_TOKEN_STAR_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p))
		{
			clog_syntax_error(parser,"*= requires a number",p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		if (reduction->value->type == clog_ast_literal_real)
			reduction->value->value.real *= p->value.real;
		else
			reduction->value->value.integer *= p->value.integer;
		break;

	case CLOG_TOKEN_SLASH_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p))
		{
			clog_syntax_error(parser,"/= requires a number",p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		if (reduction->value->type == clog_ast_literal_real)
		{
			if (p->value.real == 0.0)
			{
				clog_syntax_error(parser,"Division by 0.0",p->line);
				clog_ast_literal_free(parser,p);
				return 0;
			}
			reduction->value->value.real /= p->value.real;
		}
		else
		{
			if (p->value.integer == 0)
			{
				clog_syntax_error(parser,"Division by 0",p->line);
				clog_ast_literal_free(parser,p);
				return 0;
			}
			reduction->value->value.integer /= p->value.integer;
		}
		break;

	case CLOG_TOKEN_PERCENT_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) ||
				!clog_ast_literal_int_promote(p))
		{
			clog_syntax_error(parser,"%= require an integer",p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		reduction->value->value.integer %= p->value.integer;
		break;

	case CLOG_TOKEN_PLUS_ASSIGN:
		{
			int ret = clog_ast_literal_add(parser,reduction->value,p);
			if (!ret)
			{
				clog_ast_literal_free(parser,p);
				return 0;
			}
			if (ret != 2)
			{
				clog_ast_literal_free(parser,p);
				return 1;
			}
		}
		break;

	case CLOG_TOKEN_MINUS_ASSIGN:
		if (!clog_ast_literal_arith_convert(reduction->value,p))
		{
			clog_syntax_error(parser,"-= require a number",p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		if (reduction->value->type == clog_ast_literal_real)
			reduction->value->value.real -= p->value.real;
		else
			reduction->value->value.integer -= p->value.integer;
		break;

	case CLOG_TOKEN_RIGHT_SHIFT_ASSIGN:
	case CLOG_TOKEN_LEFT_SHIFT_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) || !clog_ast_literal_int_promote(p))
		{
			clog_syntax_error_token(parser,NULL," requires an integer",(*expr)->expr.builtin->type,NULL,p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		if ((*expr)->expr.builtin->type == CLOG_TOKEN_RIGHT_SHIFT)
			reduction->value->value.integer >>= p->value.integer;
		else
			reduction->value->value.integer <<= p->value.integer;
		break;

	case CLOG_TOKEN_AMPERSAND_ASSIGN:
	case CLOG_TOKEN_CARET_ASSIGN:
	case CLOG_TOKEN_BAR_ASSIGN:
		if (!clog_ast_literal_int_promote(reduction->value) || !clog_ast_literal_int_promote(p))
		{
			clog_syntax_error_token(parser,NULL," requires an integer",(*expr)->expr.builtin->type,NULL,p->line);
			clog_ast_literal_free(parser,p);
			return 0;
		}
		if ((*expr)->expr.builtin->type == CLOG_TOKEN_BAR)
			reduction->value->value.integer |= p->value.integer;
		else if ((*expr)->expr.builtin->type == CLOG_TOKEN_CARET)
			reduction->value->value.integer ^= p->value.integer;
		else
			reduction->value->value.integer &= p->value.integer;
		break;

	default:
		clog_ast_literal_free(parser,p);
		return 1;
	}

	clog_ast_literal_free(parser,p);
	if (!clog_ast_literal_clone(parser,&p,reduction->value) || !clog_ast_expression_alloc_literal(parser,&e,p))
		return 0;

	(*expr)->expr.builtin->type = CLOG_TOKEN_ASSIGN;
	clog_ast_expression_free(parser,(*expr)->expr.builtin->args[1]);
	(*expr)->expr.builtin->args[1] = e;
	reduction->reduced = 1;
	return 1;
}

static int clog_ast_expression_reduce_boolean(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	struct clog_ast_literal* p;
	struct clog_ast_expression* e = NULL;

	if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction) ||
			!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction) ||
			!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[2],reduction) ||
			!clog_ast_expression_eval(parser,&p,(*expr)->expr.builtin->args[0],reduction))
	{
		return 0;
	}

	if (!p)
		return 1;

	if ((*expr)->expr.builtin->type == CLOG_TOKEN_AND)
	{
		clog_ast_literal_bool_promote(p);
		if (!p->value.integer)
		{
			if (!clog_ast_expression_alloc_literal(parser,&e,p))
				return 0;
		}
		else
		{
			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
		}
	}
	else if ((*expr)->expr.builtin->type == CLOG_TOKEN_OR)
	{
		clog_ast_literal_bool_promote(p);
		if (p->value.integer)
		{
			if (!clog_ast_expression_alloc_literal(parser,&e,p))
				return 0;
		}
		else
		{
			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
		}
	}
	else if ((*expr)->expr.builtin->type == CLOG_TOKEN_QUESTION)
	{
		if (clog_ast_literal_bool_cast(p))
		{
			e = (*expr)->expr.builtin->args[1];
			(*expr)->expr.builtin->args[1] = NULL;
		}
		else
		{
			e = (*expr)->expr.builtin->args[2];
			(*expr)->expr.builtin->args[2] = NULL;
		}
	}

	if (e)
	{
		clog_ast_expression_free(parser,*expr);
		*expr = e;
		reduction->reduced = 1;
	}
	return 1;
}

static int clog_ast_expression_reduce_builtin(struct clog_parser* parser, struct clog_ast_expression** expr, struct clog_ast_reduction* reduction)
{
	struct clog_ast_expression* e = NULL;
	struct clog_ast_literal* p1 = NULL;
	struct clog_ast_literal* p2 = NULL;

	/* First, rotate out any comma expressions */
	clog_ast_expression_reduce_builtin_comma(parser,expr,reduction);

	switch ((*expr)->expr.builtin->type)
	{
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
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
		return clog_ast_expression_reduce_assign(parser,expr,reduction);

	case CLOG_TOKEN_AND:
	case CLOG_TOKEN_OR:
	case CLOG_TOKEN_QUESTION:
		return clog_ast_expression_reduce_boolean(parser,expr,reduction);

	default:
		break;
	}

	if (!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[0],reduction) ||
			!clog_ast_expression_reduce(parser,&(*expr)->expr.builtin->args[1],reduction) ||
			!clog_ast_expression_eval(parser,&p1,(*expr)->expr.builtin->args[0],reduction) ||
			!clog_ast_expression_eval(parser,&p2,(*expr)->expr.builtin->args[1],reduction))
	{
		return 0;
	}

	if (!p1 && !p2)
		return 1;

	switch ((*expr)->expr.builtin->type)
	{
	case CLOG_TOKEN_EXCLAMATION:
		/* Logical NOT */
		if (p1)
		{
			clog_ast_literal_bool_promote(p1);
			p1->value.integer = (p1->value.integer == 0 ? 1 : 0);
			goto replace_with_p1;
		}
		if ((*expr)->expr.builtin->args[0]->type == clog_ast_expression_builtin &&
				(*expr)->expr.builtin->args[0]->expr.builtin->type == CLOG_TOKEN_EXCLAMATION)
		{
			/* Remove any double negate */
			e = (*expr)->expr.builtin->args[0]->expr.builtin->args[0];
			(*expr)->expr.builtin->args[0]->expr.builtin->args[0] = NULL;
			clog_ast_expression_free(parser,*expr);
			*expr = e;
			reduction->reduced = 1;
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

	case CLOG_TOKEN_PLUS:
		if (p1 && p1->type == clog_ast_literal_real && p1->value.real != p1->value.real) /* NaN */
			goto replace_with_p1;
		if (p1 && p2)
		{
			int ret = clog_ast_literal_add(parser,p1,p2);
			if (!ret)
				goto failed;
			if (ret == 2)
				goto replace_with_p1;
		}
		if (p2)
		{
			if (p2->type == clog_ast_literal_real)
			{
				if (p2->value.real == 0.0)
					goto replace_with_arg0;
			}
			else
			{
				if (!clog_ast_literal_int_promote(p2))
				{
					clog_syntax_error(parser,"+ requires numbers",p2->line);
					goto failed;
				}
				if (p2->value.integer == 0.0)
					goto replace_with_arg0;
			}
		}
		break;

	case CLOG_TOKEN_MINUS:
		if (p1 && p1->type == clog_ast_literal_real && p1->value.real != p1->value.real) /* NaN */
			goto replace_with_p1;
		if (p1 && p2)
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
		if (p2)
		{
			if (p2->type == clog_ast_literal_real)
			{
				if (p2->value.real == 0.0)
					goto replace_with_arg0;
			}
			else
			{
				if (!clog_ast_literal_int_promote(p2))
				{
					clog_syntax_error(parser,"- requires numbers",p2->line);
					goto failed;
				}
				if (p2->value.integer == 0.0)
					goto replace_with_arg0;
			}
		}
		break;

	case CLOG_TOKEN_STAR:
		if (p1 && p1->type == clog_ast_literal_real && p1->value.real != p1->value.real) /* NaN */
			goto replace_with_p1;
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
		if (p2)
		{
			if (p2->type == clog_ast_literal_real)
			{
				if (p2->value.real == 1.0)
					goto replace_with_arg0;
				if (p2->value.real == 0.0)
					goto replace_with_p2;
			}
			else
			{
				if (!clog_ast_literal_int_promote(p2))
				{
					clog_syntax_error(parser,"* requires numbers",p2->line);
					goto failed;
				}
				if (p2->value.integer == 1)
					goto replace_with_arg0;
				if (p2->value.integer == 0.0)
					goto replace_with_p2;
			}
		}
		break;

	case CLOG_TOKEN_SLASH:
		if (p1 && p1->type == clog_ast_literal_real && p1->value.real != p1->value.real) /* NaN */
			goto replace_with_p1;
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
		if (p2)
		{
			if (p2->type == clog_ast_literal_real)
			{
				if (p2->value.real == 1.0)
					goto replace_with_arg0;
				if (p2->value.real == 0.0)
				{
					clog_syntax_error(parser,"Division by 0.0",p2->line);
					goto failed;
				}
			}
			else
			{
				if (!clog_ast_literal_int_promote(p2))
				{
					clog_syntax_error(parser,"/ requires numbers",p1->line);
					goto failed;
				}
				if (p2->value.integer == 1)
					goto replace_with_arg0;
				if (p2->value.integer == 0)
				{
					clog_syntax_error(parser,"Division by 0",p1->line);
					goto failed;
				}
			}
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

			if (p2->value.integer == 0)
			{
				clog_syntax_error(parser,"Division by 0",p2->line);
				goto failed;
			}

			p1->value.integer %= p2->value.integer;
			goto replace_with_p1;
		}
		if (p2)
		{
			if (!clog_ast_literal_int_promote(p2))
			{
				clog_syntax_error(parser,"% requires integers",p2->line);
				goto failed;
			}
			if (p2->value.integer == 1)
			{
				struct clog_ast_literal* lit_zero;
				if (!clog_ast_literal_alloc_bool(parser,&lit_zero,0))
					goto failed;
				lit_zero->type = clog_ast_literal_integer;

				clog_ast_expression_free(parser,*expr);
				if (!clog_ast_expression_alloc_literal(parser,expr,lit_zero))
					goto failed;

				reduction->reduced = 1;
				goto success;
			}
			if (p2->value.integer == 0)
			{
				clog_syntax_error(parser,"Division by 0",p1->line);
				goto failed;
			}
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
		if (p2)
		{
			if (!clog_ast_literal_int_promote(p2))
			{
				clog_syntax_error_token(parser,NULL," requires integers",(*expr)->expr.builtin->type,NULL,p2->line);
				goto failed;
			}

			if (p2->value.integer == 0)
				goto replace_with_arg0;
		}
		break;

	case CLOG_TOKEN_LESS_THAN:
	case CLOG_TOKEN_LESS_THAN_EQUALS:
	case CLOG_TOKEN_GREATER_THAN:
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
	case CLOG_TOKEN_EQUALS:
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

			case CLOG_TOKEN_LESS_THAN_EQUALS:
				p1->value.integer = (b <= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN:
				p1->value.integer = (b > 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_GREATER_THAN_EQUALS:
				p1->value.integer = (b >= 0 ? 1 : 0);
				break;

			case CLOG_TOKEN_EQUALS:
				p1->value.integer = (b == 0 ? 1 : 0);
				break;

			default:
				break;
			}

			p1->type = clog_ast_literal_bool;
			goto replace_with_p1;
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

	case CLOG_TOKEN_DOT:
	case CLOG_TOKEN_OPEN_BRACKET:
	case CLOG_TOKEN_IN:
	case CLOG_TOKEN_COMMA:
		break;

	default:
		break;
	}

success:
	clog_ast_literal_free(parser,p1);
	clog_ast_literal_free(parser,p2);
	return 1;

replace_with_arg0:
	e = (*expr)->expr.builtin->args[0];
	(*expr)->expr.builtin->args[0] = NULL;
	clog_ast_expression_free(parser,*expr);
	*expr = e;
	reduction->reduced = 1;
	goto success;

replace_with_p1:
	if (!clog_ast_expression_alloc_literal(parser,&e,p1))
	{
		p1 = NULL;
		goto failed;
	}
	p1 = NULL;
	clog_ast_expression_free(parser,*expr);
	*expr = e;
	reduction->reduced = 1;
	goto success;

replace_with_p2:
	if (!clog_ast_expression_alloc_literal(parser,&e,p2))
	{
		p2 = NULL;
		goto failed;
	}
	p2 = NULL;
	clog_ast_expression_free(parser,*expr);
	*expr = e;
	reduction->reduced = 1;
	goto success;

failed:
	clog_ast_literal_free(parser,p1);
	clog_ast_literal_free(parser,p2);
	return 0;
}

static int clog_ast_expression_list_reduce(struct clog_parser* parser, struct clog_ast_expression_list* list, struct clog_ast_reduction* reduction)
{
	for (;list;list = list->next)
	{
		if (list->expr->type == clog_ast_expression_identifier)
		{
			if (clog_ast_literal_id_compare(reduction->id,list->expr->expr.identifier) == 0)
				reduction->referenced = 1;
		}
		else if (!clog_ast_expression_reduce(parser,&list->expr,reduction))
			return 0;
	}
	return 1;
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
						continue;
					}
					break;

				case CLOG_TOKEN_DOUBLE_PLUS:
				case CLOG_TOKEN_DOUBLE_MINUS:
					if ((*stmt)->stmt->stmt.expression->expr.builtin->args[0])
					{
						/* Rewrite solitary v++ => ++v */
						(*stmt)->stmt->stmt.expression->expr.builtin->args[1] = (*stmt)->stmt->stmt.expression->expr.builtin->args[0];
						(*stmt)->stmt->stmt.expression->expr.builtin->args[0] = NULL;
						reduction->reduced = 1;
						continue;
					}
					break;

				default:
					break;
				}
			}
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

			/* Check if new variable hides the one we are reducing */
			if (clog_ast_literal_id_compare(reduction->id,(*stmt)->stmt->stmt.declaration) == 0)
			{
				int ret = 1;
				struct clog_ast_literal* old_value = reduction->value;

				/* Reduce the assignment expression that follows */
				if (!clog_ast_expression_reduce(parser,&(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1],reduction))
					return 0;

				ret = clog_ast_expression_eval(parser,&reduction->value,(*stmt)->next->stmt->stmt.expression->expr.builtin->args[1],reduction);
				if (ret)
					ret = clog_ast_statement_list_reduce(parser,&(*stmt)->next->next,reduction);

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

		if (*stmt)
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

static int clog_ast_statement_list_reduce_block(struct clog_parser* parser, struct clog_ast_statement_list** block, struct clog_ast_reduction* reduction)
{
	struct clog_ast_statement_list** l;
	int reduced = reduction->reduced;
	int referenced = reduction->referenced;

	do
	{
		reduction->reduced = 0;
		reduction->referenced = 0;

		if (!clog_ast_statement_list_reduce(parser,block,reduction))
			return 0;

		/* Now reduce the block with each declared variable */
		for (l = block;*l;)
		{
			if ((*l)->stmt->type == clog_ast_statement_declaration)
			{
				struct clog_ast_reduction reduction2 = {0};
				int ret;

				/* Declaration is always followed by an assign */
				reduction2.id = (*l)->stmt->stmt.declaration;

				/* Reduce the assignment expression that follows */
				if (!clog_ast_expression_reduce(parser,&(*l)->next->stmt->stmt.expression->expr.builtin->args[1],reduction))
					return 0;

				if (!clog_ast_expression_eval(parser,&reduction2.value,(*l)->next->stmt->stmt.expression->expr.builtin->args[1],reduction))
					return 0;

				reduction2.init_expr = &(*l)->next->stmt->stmt.expression;

				ret = clog_ast_statement_list_reduce(parser,&(*l)->next->next,&reduction2);
				clog_ast_literal_free(parser,reduction2.value);
				if (!ret)
					return 0;

				if (reduction2.reduced)
					reduction->reduced = 1;

				if (!reduction2.referenced)
				{
					/* This variable is not used */
					struct clog_ast_statement_list* next = (*l)->next->next;

					if (reduction2.init_expr != &(*l)->next->stmt->stmt.expression->expr.builtin->args[1])
					{
						/* Final assignment isn't used either */
						struct clog_ast_expression* e = (*reduction2.init_expr)->expr.builtin->args[1];
						(*reduction2.init_expr)->expr.builtin->args[1] = NULL;

						clog_ast_expression_free(parser,*reduction2.init_expr);
						*reduction2.init_expr = e;
						reduction->reduced = 1;
					}

					(*l)->next->next = NULL;
					clog_ast_statement_list_free(parser,*l);
					*l = next;
				}
				else
					l = &(*l)->next->next;
			}
			else
				l = &(*l)->next;
		}

		if (reduction->reduced)
			reduced = 1;
	}
	while (reduction->reduced);

	/* Check for blocks in block */
	clog_ast_statement_list_flatten(parser,block);

	reduction->reduced = reduced;

	if (referenced)
		reduction->referenced = referenced;

	return 1;
}

static int clog_ast_statement_list_reduce_if(struct clog_parser* parser, struct clog_ast_statement_list** if_stmt, struct clog_ast_reduction* reduction)
{
	struct clog_ast_literal* if_lit;
	struct clog_ast_expression** old_init_expr;

	/* Check for literal condition */
	if (!clog_ast_expression_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->condition,reduction) ||
			!clog_ast_expression_eval(parser,&if_lit,(*if_stmt)->stmt->stmt.if_stmt->condition,reduction))
	{
		return 0;
	}

	if (if_lit)
	{
		struct clog_ast_statement_list* next = (*if_stmt)->next;

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

		(*if_stmt)->next = NULL;
		clog_ast_statement_list_free(parser,*if_stmt);

		*if_stmt = clog_ast_statement_list_append(parser,l2,next);

		reduction->reduced = 1;

		clog_ast_literal_free(parser,if_lit);
		return 1;
	}

	/* Reduce both statements */
	old_init_expr = reduction->init_expr;
	if (reduction->value)
	{
		/* Check to see if the value changes in each statement */
		struct clog_ast_literal* true_value = NULL;
		struct clog_ast_literal* false_value = NULL;

		if ((*if_stmt)->stmt->stmt.if_stmt->true_stmt)
		{
			struct clog_ast_literal* old_value = reduction->value;
			if (!clog_ast_literal_clone(parser,&reduction->value,old_value))
				return 0;

			reduction->init_expr = NULL;

			if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->true_stmt,reduction))
			{
				clog_ast_literal_free(parser,old_value);
				return 0;
			}

			true_value = reduction->value;
			reduction->value = old_value;

			if (reduction->init_expr)
				old_init_expr = NULL;
		}
		if ((*if_stmt)->stmt->stmt.if_stmt->false_stmt)
		{
			struct clog_ast_literal* old_value = reduction->value;
			if (!clog_ast_literal_clone(parser,&reduction->value,old_value))
				return 0;

			reduction->init_expr = NULL;

			if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->false_stmt,reduction))
			{
				clog_ast_literal_free(parser,old_value);
				return 0;
			}

			false_value = reduction->value;
			reduction->value = old_value;

			if (reduction->init_expr)
				old_init_expr = NULL;
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
	else
	{
		reduction->init_expr = NULL;
		if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->true_stmt,reduction))
			return 0;

		if (reduction->init_expr)
		{
			old_init_expr = NULL;
			reduction->init_expr = NULL;
		}

		if (!clog_ast_statement_list_reduce(parser,&(*if_stmt)->stmt->stmt.if_stmt->false_stmt,reduction))
			return 0;

		if (reduction->init_expr)
			old_init_expr = NULL;
	}
	reduction->init_expr = old_init_expr;

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

	clog_ast_statement_list_flatten(parser,&(*if_stmt)->stmt->stmt.if_stmt->true_stmt);
	clog_ast_statement_list_flatten(parser,&(*if_stmt)->stmt->stmt.if_stmt->false_stmt);

	return 1;
}

static int clog_ast_statement_list_reduce_do(struct clog_parser* parser, struct clog_ast_statement_list** do_stmt, struct clog_ast_reduction* reduction)
{
	struct clog_ast_literal* if_lit;
	struct clog_ast_literal* old_value = reduction->value;
	struct clog_ast_expression** old_init_expr = reduction->init_expr;

	/* Clear value and reduce loop statement */
	reduction->value = NULL;
	reduction->init_expr = NULL;

	if (!clog_ast_statement_list_reduce(parser,&(*do_stmt)->stmt->stmt.do_stmt->loop_stmt,reduction))
	{
		reduction->value = old_value;
		return 0;
	}

	/* Check for constant false condition */
	if (!clog_ast_expression_reduce(parser,&(*do_stmt)->stmt->stmt.do_stmt->condition,reduction) ||
			!clog_ast_expression_eval(parser,&if_lit,(*do_stmt)->stmt->stmt.do_stmt->condition,reduction))
	{
		reduction->value = old_value;
		return 0;
	}

	if (!reduction->init_expr)
	{
		/* Restore old value if not assigned */
		reduction->value = old_value;
		reduction->init_expr = old_init_expr;
	}
	else
	{
		/* Clear value and init if assigned */
		clog_ast_literal_free(parser,old_value);
		clog_ast_literal_free(parser,reduction->value);
		reduction->value = NULL;
		reduction->init_expr = NULL;
	}

	/* Check for constant false condition */
	if (if_lit)
	{
		if (!clog_ast_literal_bool_cast(if_lit))
		{
			struct clog_ast_statement_list* l2 = (*do_stmt)->stmt->stmt.do_stmt->loop_stmt;
			struct clog_ast_statement_list* next = (*do_stmt)->next;

			(*do_stmt)->next = NULL;
			clog_ast_statement_list_free(parser,*do_stmt);

			*do_stmt = clog_ast_statement_list_append(parser,l2,next);

			reduction->reduced = 1;
			clog_ast_literal_free(parser,if_lit);

			return 1;
		}

		clog_ast_literal_free(parser,if_lit);
	}

	clog_ast_statement_list_flatten(parser,&(*do_stmt)->stmt->stmt.do_stmt->loop_stmt);

	return 1;
}

int clog_ast_statement_list_reduce_constants(struct clog_parser* parser, struct clog_ast_statement_list** block)
{
	struct clog_ast_reduction reduction = {0};

	return clog_ast_statement_list_reduce_block(parser,block,&reduction);
}

