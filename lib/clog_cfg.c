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


	struct clog_cfg_block* break_point;
	struct clog_cfg_block* branch;
	struct clog_cfg_block* fallthru;

};

static void __dump();

static void clog_cfg_out_of_memory()
{
	printf("Out of memory during code generation\n");
}


static unsigned int s_gen = 0;

static int clog_cfg_alloc_block(struct clog_cfg_block* from, struct clog_cfg_block** block)
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

	if (from)
	{
		struct clog_cfg_block* next = from->a_next;
		(*block)->a_next = next;
		from->a_next = *block;
	}

	return 1;
}

static struct clog_cfg_block* clog_cfg_construct_expression(struct clog_cfg_block* block, const struct clog_ast_expression* ast_expr)
{
	return block;
}

static struct clog_cfg_block* clog_cfg_append_fallthru(struct clog_cfg_block* block)
{
	if (!block->fallthru && !clog_cfg_alloc_block(block,&block->fallthru))
		return NULL;

	return block->fallthru;
}

static struct clog_cfg_block* clog_cfg_insert_fallthru(struct clog_cfg_block* block)
{
	struct clog_cfg_block* fallthru = block->fallthru;
	if (!clog_cfg_alloc_block(block,&block->fallthru))
		return NULL;

	block->fallthru->fallthru = fallthru;
	return block->fallthru;
}

static void clog_cfg_add_block_reference(struct clog_cfg_block* from, struct clog_cfg_block* to)
{
	/* Add a back pointer to the from block from the to block */
	void* TODO = from;

	++to->fallthru->refcount;
}

static struct clog_cfg_block* clog_cfg_insert_branch(struct clog_cfg_block* block)
{
	struct clog_cfg_block* branch = block->branch;
	if (!clog_cfg_alloc_block(block,&block->branch))
		return NULL;

	block->branch->fallthru = block->fallthru;
	clog_cfg_add_block_reference(block->branch,block->fallthru);

	block->branch->branch = branch;
	return block->branch;
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
			{
				struct clog_cfg_block* fallthru = clog_cfg_append_fallthru(block);
				struct clog_cfg_block* or_case = clog_cfg_insert_fallthru(block);
				if (!fallthru || !or_case)
					return NULL;

				or_case->branch = block->branch;
				clog_cfg_add_block_reference(or_case,block->branch);

				if (!clog_cfg_construct_condition(block,ast_expr->expr.builtin->args[0]) ||
						!clog_cfg_construct_condition(or_case,ast_expr->expr.builtin->args[1]))
				{
					return NULL;
				}
				return fallthru;
			}

		case CLOG_TOKEN_AND:
			{
				struct clog_cfg_block* fallthru = clog_cfg_append_fallthru(block);
				struct clog_cfg_block* and_case = clog_cfg_insert_branch(block);

				if (!fallthru || !and_case)
					return NULL;

				if (!clog_cfg_construct_condition(block,ast_expr->expr.builtin->args[0]) ||
						!clog_cfg_construct_condition(and_case,ast_expr->expr.builtin->args[1]))
				{
					return NULL;
				}
				return fallthru;
			}

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

static struct clog_cfg_block* clog_cfg_construct_if(struct clog_cfg_block* block, const struct clog_ast_statement_if* ast_if)
{
	struct clog_cfg_block* fallthru = clog_cfg_append_fallthru(block);
	struct clog_cfg_block* true_branch = clog_cfg_insert_branch(block);
	struct clog_cfg_block* false_branch;

	if (!fallthru || !true_branch)
		return NULL;

	if (ast_if->false_block && !(false_branch = clog_cfg_insert_fallthru(block)))
		return NULL;

	if (!clog_cfg_construct_condition(block,ast_if->condition))
		return NULL;

	if (!clog_cfg_construct_block(true_branch,ast_if->true_block))
		return NULL;

	if (ast_if->false_block && !clog_cfg_construct_block(false_branch,ast_if->false_block))
		return NULL;

	return fallthru;
}

static struct clog_cfg_block* clog_cfg_construct_do(struct clog_cfg_block* block, const struct clog_ast_statement_do* ast_do)
{
	struct clog_cfg_block* fallthru = clog_cfg_append_fallthru(block);
	struct clog_cfg_block* loop_body;
	struct clog_cfg_block* condition;

	loop_body = clog_cfg_insert_fallthru(block);
	if (!loop_body)
		return NULL;

	condition = clog_cfg_insert_fallthru(loop_body);
	if (!!condition)
		return NULL;

	condition->branch = loop_body;
	clog_cfg_add_block_reference(condition,loop_body);

	if (!clog_cfg_construct_block(loop_body,ast_do->loop_block))
		return NULL;

	if (!clog_cfg_construct_condition(condition,ast_do->condition))
		return NULL;

	return fallthru;
}

static struct clog_cfg_block* clog_cfg_construct_while(struct clog_cfg_block* block, const struct clog_ast_statement_while* ast_while)
{
	struct clog_cfg_block* fallthru = clog_cfg_append_fallthru(block);
	struct clog_cfg_block* header = clog_cfg_insert_fallthru(block);
	struct clog_cfg_block* branch;

	if (!fallthru)
		return NULL;

	if (ast_while->pre)
	{
		const struct clog_ast_statement_list* list = ast_while->pre;
		for (;list && header;list = list->next)
			header = clog_cfg_construct_expression(header,list->stmt->stmt.expression);
	}

	if (!header || !clog_cfg_alloc_block(header,&header->branch))
		return NULL;

	branch = header->branch;
	branch->fallthru = header;
	clog_cfg_add_block_reference(branch,header);

	if (!clog_cfg_construct_condition(header,ast_while->condition))
		return NULL;

	if (!clog_cfg_construct_block(branch,ast_while->loop_block))
		return NULL;

	return fallthru;
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
			/* These should have been dropped */
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

		case clog_ast_statement_while:
			block = clog_cfg_construct_while(block,list->stmt->stmt.while_stmt);
			break;

		case clog_ast_statement_break:
		case clog_ast_statement_continue:
		case clog_ast_statement_return:
			break;
		}
	}

	return block;
}

static void __dump(struct clog_cfg_block* block)
{
	if (block)
	{
		printf("%u: (ref: %u)\n",block->gen,block->refcount);

		if (block->branch)
			printf("  IF goto %u\n",block->branch->gen);

		if (block->fallthru)
			printf("  goto %u\n",block->fallthru->gen);
		else
			printf("  END\n");

		__dump(block->a_next);
	}
}

int clog_cfg_construct(const struct clog_ast_block* ast_block)
{
	struct clog_cfg_block* block;
	if (!clog_cfg_alloc_block(NULL,&block))
		return 0;

	if (!clog_cfg_construct_block(block,ast_block))
	{
		return 0;
	}

	__dump(block);

	return 1;
}
