/*
 * clog_cfg.c
 *
 *  Created on: 11 Sep 2013
 *      Author: taylorr
 */

#include "clog_ast.h"
#include "clog_parser.h"

#include <string.h>
#include <stdio.h>

struct clog_cfg_block
{
	unsigned int gen;

	struct clog_cfg_block* a_next;
	unsigned int refcount;


	struct clog_cfg_block* branch;
	struct clog_cfg_block* fallthru;

};

static void __dump();

static void clog_cfg_out_of_memory()
{
	printf("Out of memory during code generation\n");
}


static unsigned int s_gen = 0;
static struct clog_cfg_block* s_block_list = NULL;
static struct clog_cfg_block* s_block_list_last = NULL;

static int clog_cfg_alloc_block(struct clog_cfg_block** block)
{
	*block = clog_malloc(sizeof(struct clog_cfg_block));
	if (!(*block))
	{
		clog_cfg_out_of_memory();
		return 0;
	}

	memset(*block,0,sizeof(struct clog_cfg_block));
	(*block)->refcount = 1;
	(*block)->gen = ++s_gen;

	if (!s_block_list_last)
		s_block_list = *block;
	else
		s_block_list_last->a_next = *block;
	s_block_list_last = *block;

	return 1;
}

static struct clog_cfg_block* clog_cfg_construct_expression(struct clog_cfg_block* block, const struct clog_ast_expression* ast_expr)
{
	return block;
}

static struct clog_cfg_block* clog_cfg_construct_condition(struct clog_cfg_block* block, const struct clog_ast_expression* ast_expr)
{
	switch (ast_expr->type)
	{
	case clog_ast_expression_identifier:
		/* Dropped */
		break;

	case clog_ast_expression_variable:
		break;

	case clog_ast_expression_literal:
		break;

	case clog_ast_expression_call:
		break;

	case clog_ast_expression_builtin:
		switch (ast_expr->expr.builtin->type)
		{
		case CLOG_TOKEN_COMMA:
			block = clog_cfg_construct_expression(block,ast_expr->expr.builtin->args[0]);
			if (block)
				block = clog_cfg_construct_condition(block,ast_expr->expr.builtin->args[1]);
			break;

		case CLOG_TOKEN_OR:

		case CLOG_TOKEN_AND:

		case CLOG_TOKEN_EQUALS:
		case CLOG_TOKEN_NOT_EQUALS:
		case CLOG_TOKEN_LESS_THAN:
		case CLOG_TOKEN_LESS_THAN_EQUALS:
		case CLOG_TOKEN_GREATER_THAN:
		case CLOG_TOKEN_GREATER_THAN_EQUALS:
			break;

		default:
			break;
		}
		break;
	}
	return block;
}

static struct clog_cfg_block* clog_cfg_construct_block(struct clog_cfg_block* block, const struct clog_ast_block* ast_block);

static struct clog_cfg_block* clog_cfg_add_fallthru(struct clog_cfg_block* block)
{
	if (!block->fallthru && !clog_cfg_alloc_block(&block->fallthru))
		return NULL;

	return block->fallthru;
}

static struct clog_cfg_block* clog_cfg_insert_block(struct clog_cfg_block* block)
{
	struct clog_cfg_block* fallthru = block->fallthru;
	if (!clog_cfg_alloc_block(&block->fallthru))
	{
		block->fallthru = fallthru;
		return NULL;
	}

	block->fallthru->fallthru = fallthru;
	return block->fallthru;
}

static struct clog_cfg_block* clog_cfg_construct_if(struct clog_cfg_block* block, const struct clog_ast_statement_if* ast_if)
{
	/* First create diamond shaped graph */
	struct clog_cfg_block* fallthru = clog_cfg_add_fallthru(block);
	if (!fallthru)
		return NULL;

	/* Insert true branch */
	if (!clog_cfg_alloc_block(&block->branch))
		return NULL;

	block->branch->fallthru = fallthru;
	++fallthru->refcount;

	if (ast_if->false_block && !clog_cfg_insert_block(block))
		return NULL;

	/* Now fill in the blocks */
	if (!clog_cfg_construct_condition(block,ast_if->condition))
		return NULL;

	if (!clog_cfg_construct_block(block->branch,ast_if->true_block))
		return NULL;

	if (ast_if->false_block && !clog_cfg_construct_block(block->fallthru,ast_if->false_block))
		return NULL;

	return fallthru;
}

static struct clog_cfg_block* clog_cfg_construct_do(struct clog_cfg_block* block, const struct clog_ast_statement_do* ast_do)
{
	struct clog_cfg_block* loop_body;
	struct clog_cfg_block* condition;

	if (!clog_cfg_add_fallthru(block))
		return NULL;

	loop_body = clog_cfg_insert_block(block);
	condition = clog_cfg_insert_block(loop_body);
	if (!loop_body || !condition)
		return NULL;

	condition->branch = loop_body;
	++loop_body->refcount;

	if (!clog_cfg_construct_block(loop_body,ast_do->loop_block))
		return NULL;

	return clog_cfg_construct_condition(condition,ast_do->condition);
}

static struct clog_cfg_block* clog_cfg_construct_block(struct clog_cfg_block* block, const struct clog_ast_block* ast_block)
{
	const struct clog_ast_statement_list* list = ast_block->stmts;
	for (;list && block;list = list->next)
	{
		switch (list->stmt->type)
		{
		case clog_ast_statement_declaration:
		case clog_ast_statement_constant:
			/* These have been dropped */
			break;

		case clog_ast_statement_expression:
			block = clog_cfg_construct_expression(block,list->stmt->stmt.expression);
			break;

		case clog_ast_statement_block:
			block = clog_cfg_construct_block(block,list->stmt->stmt.block);
			break;

		case clog_ast_statement_if:
			block = clog_cfg_construct_if(block,list->stmt->stmt.if_stmt);
			break;

		case clog_ast_statement_do:
			block = clog_cfg_construct_do(block,list->stmt->stmt.do_stmt);
			break;

		case clog_ast_statement_break:
		case clog_ast_statement_continue:
		case clog_ast_statement_return:
			break;
		}
	}

	return block;
}

static void __dump_link(struct clog_cfg_block* block)
{
	if (block)
	{
		printf("%u:\n",block->gen);

		if (block->branch)
			printf("  IF goto %u\n",block->branch->gen);

		if (block->fallthru)
			printf("  goto %u\n",block->fallthru->gen);
		else
			printf("END\n");

		__dump_link(block->a_next);
	}
}

static void __dump()
{
	__dump_link(s_block_list);
}

int clog_cfg_construct(const struct clog_ast_block* ast_block)
{
	struct clog_cfg_block* block;
	if (!clog_cfg_alloc_block(&block))
		return 0;

	if (!clog_cfg_construct_block(block,ast_block))
	{
		return 0;
	}

	__dump();

	return 1;
}
