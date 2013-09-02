/*
 * clog_codegen.c
 *
 *  Created on: 2 Sep 2013
 *      Author: rick
 */

#include "clog_ast.h"
#include "clog_parser.h"

#include <stdio.h>

int clog_codegen(const struct clog_ast_statement_list* list);

static int clog_codegen_emit(const char* opcode)
{
	printf("%s\n",opcode);

	return 1;
}

static int clog_codegen_expression_builtin(const struct clog_ast_expression_builtin* builtin);

static int clog_codegen_loadA(const struct clog_ast_expression* expr)
{
	if (!expr)
		return 1;

	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		printf("LOADA (%s)\n",expr->expr.identifier->value.string.str);
		return 1;

	case clog_ast_expression_literal:
		switch (expr->expr.literal->type)
		{
		case clog_ast_literal_string:
			printf("LOADA string: %s\n",expr->expr.literal->value.string.str);
			break;

		case clog_ast_literal_null:
		case clog_ast_literal_bool:
		case clog_ast_literal_integer:
			printf("LOADA int: %ld\n",expr->expr.literal->value.integer);
			break;

		case clog_ast_literal_real:
			printf("LOADA real: %g\n",expr->expr.literal->value.real);
			break;
		}
		return 1;

	case clog_ast_expression_builtin:
		return clog_codegen_expression_builtin(expr->expr.builtin);

	case clog_ast_expression_call:
		break;
	}

	return 1;
}

static int clog_codegen_loadX(const struct clog_ast_expression* expr)
{
	if (!expr)
		return 1;

	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		printf("LOADX (%s)\n",expr->expr.identifier->value.string.str);
		return 1;

	case clog_ast_expression_literal:
		switch (expr->expr.literal->type)
		{
		case clog_ast_literal_string:
			printf("LOADX string: %s\n",expr->expr.literal->value.string.str);
			break;

		case clog_ast_literal_null:
		case clog_ast_literal_bool:
		case clog_ast_literal_integer:
			printf("LOADX int: %ld\n",expr->expr.literal->value.integer);
			break;

		case clog_ast_literal_real:
			printf("LOADX real: %g\n",expr->expr.literal->value.real);
			break;
		}
		return 1;

	case clog_ast_expression_builtin:
		return (clog_codegen_expression_builtin(expr->expr.builtin) &&
				clog_codegen_emit("TAX"));

	case clog_ast_expression_call:
		return (clog_codegen_expression_builtin(expr->expr.builtin) &&
				clog_codegen_emit("TAX"));
		break;
	}

	return 1;
}

static int clog_codegen_expression_builtin(const struct clog_ast_expression_builtin* builtin)
{
	if (!builtin)
		return 1;

	switch (builtin->type)
	{
	case CLOG_TOKEN_ASSIGN:
		return (clog_codegen_loadA(builtin->args[1]) &&
				clog_codegen_loadA(builtin->args[0]) &&
				clog_codegen_emit("*A = X"));

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
	case CLOG_TOKEN_IN:
		return printf("in");
	case CLOG_TOKEN_LEFT_SHIFT:
		return printf("<<");
	case CLOG_TOKEN_RIGHT_SHIFT:
		return printf(">>");

	case CLOG_TOKEN_PLUS:
		if (!builtin->args[1])
			return clog_codegen_loadA(builtin->args[0]);

		return (clog_codegen_loadX(builtin->args[1]) &&
				clog_codegen_loadA(builtin->args[0]) &&
				clog_codegen_emit("ADD X"));

	case CLOG_TOKEN_MINUS:
		if (!builtin->args[1])
			return (clog_codegen_loadA(builtin->args[0]) &&
					clog_codegen_emit("NEG"));

		return (clog_codegen_loadX(builtin->args[1]) &&
				clog_codegen_loadA(builtin->args[0]) &&
				clog_codegen_emit("SUB X"));

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
	case CLOG_TOKEN_DOT:
		return printf(".");
	}

	return 1;
}

int clog_codegen(const struct clog_ast_statement_list* list)
{
	for (;list;list=list->next)
	{
		if (list->stmt)
		{
			switch (list->stmt->type)
			{
			case clog_ast_statement_expression:
				if (list->stmt->stmt.expression)
				{
					switch (list->stmt->stmt.expression->type)
					{
					case clog_ast_expression_identifier:
					case clog_ast_expression_literal:
						/* Emit nothing */
						break;

					case clog_ast_expression_builtin:
						if (!clog_codegen_expression_builtin(list->stmt->stmt.expression->expr.builtin))
							return 0;
						break;

					case clog_ast_expression_call:
						break;
					}
				}
				break;

			case clog_ast_statement_block:
				if (!clog_codegen(list->stmt->stmt.block))
					return 0;
				break;

			case clog_ast_statement_declaration:
			case clog_ast_statement_constant:
				if (!clog_codegen_loadA(list->next->stmt->stmt.expression->expr.builtin->args[1]))
					return 0;

				printf("PUSHA (%s)\n",list->stmt->stmt.declaration->value.string.str);

				list=list->next;
				break;

			case clog_ast_statement_if:
			case clog_ast_statement_do:
			case clog_ast_statement_break:
			case clog_ast_statement_continue:
			case clog_ast_statement_return:
				break;
			}
		}
	}

	return 1;
}
