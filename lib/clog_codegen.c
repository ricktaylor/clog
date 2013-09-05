/*
 * clog_codegen.c
 *
 *  Created on: 2 Sep 2013
 *      Author: rick
 */

#include "clog_ast.h"
#include "clog_parser.h"
#include "clog_opcodes.h"

#include <string.h>
#include <stdio.h>

int clog_codegen(const struct clog_ast_statement_list* list);

struct clog_cg_triplet;

struct clog_cg_register
{
	struct clog_cg_triplet* prev_use;
	unsigned int refcount;
};

struct clog_cg_block;

struct clog_cg_triplet
{
	enum clog_opcode op;

	union clog_cg_triplet_u0
	{
		struct clog_cg_triplet_expr
		{
			struct clog_cg_register result;
			union clog_cg_triplet_u1
			{
				struct clog_cg_register* reg;
				struct clog_ast_literal* lit;
			} args[2];
		} expr;
		struct clog_cg_block* jmp;
		struct clog_cg_triplet_phi
		{
			struct clog_cg_register* regs[2];
		} phi;
	} val;
};

struct clog_cg_block_register
{
	struct clog_ast_literal* id;
	struct clog_cg_triplet* last_use;
};

struct clog_cg_block
{
	struct clog_cg_block* next;
	struct clog_cg_block* prev;

	struct clog_cg_triplet** triplets;
	unsigned int triplet_count;
	unsigned int triplet_alloc;

	struct clog_cg_block_register* registers;
	unsigned int register_count;
	unsigned int register_alloc;

	unsigned int temp_counter;
};

static int clog_cg_out_of_memory()
{
	printf("Out of memory during code generation\n");
	return 0;
}

static int clog_cg_alloc_triplet(struct clog_cg_block* block, struct clog_cg_triplet** triplet)
{
	if (block->triplet_count == block->triplet_alloc)
	{
		/* Resize array */
		struct clog_cg_triplet** new = clog_realloc(block->triplets,block->triplet_alloc * 2 * sizeof(struct clog_cg_triplet*));
		if (!new)
			return clog_cg_out_of_memory();

		block->triplet_alloc *= 2;
		block->triplets = new;
	}

	*triplet = block->triplets[block->triplet_count++];
	return 1;
}

static void clog_cg_free_triplet(struct clog_cg_block* block, struct clog_cg_triplet* triplet)
{
	unsigned int i = block->triplet_count;
	while (i-- > 0)
	{
		if (triplet == block->triplets[i])
		{
			--block->triplet_count;
			if (i < block->triplet_count)
				memmove(&block->triplets[i],&block->triplets[i+1],(block->triplet_count - i) * sizeof(struct clog_cg_triplet*));
			break;
		}
	}
}

static struct clog_cg_block_register* clog_cg_alloc_register(struct clog_cg_block* block, struct clog_ast_literal** id)
{
	if (block->register_count == block->register_alloc)
	{
		/* Resize array */
		struct clog_cg_block_register* new = clog_realloc(block->registers,block->register_alloc * 2 * sizeof(struct clog_cg_block_register));
		if (!new)
		{
			clog_cg_out_of_memory();
			return NULL;
		}

		block->register_alloc *= 2;
		block->registers = new;
	}

	block->registers[block->register_count].last_use = NULL;
	block->registers[block->register_count].id = *id;
	*id = NULL;

	return &block->registers[block->register_count++];
}

static struct clog_cg_block_register* clog_cg_alloc_temp_register(struct clog_cg_block* block)
{
	struct clog_cg_block_register* retval;
	struct clog_ast_literal* lit;
	char szBuf[sizeof(block->temp_counter) * 2 + 2] = {0};
	sprintf(szBuf,"$%x",block->temp_counter);

	lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!lit)
	{
		clog_cg_out_of_memory();
		return NULL;
	}

	lit->line = 0;
	lit->type = clog_ast_literal_string;
	lit->value.string.len = strlen(szBuf);
	lit->value.string.str = clog_malloc(lit->value.string.len+1);
	if (!lit->value.string.str)
	{
		clog_free(lit);
		clog_cg_out_of_memory();
		return NULL;
	}

	memcpy(lit->value.string.str,szBuf,lit->value.string.len);
	lit->value.string.str[lit->value.string.len] = 0;

	retval = clog_cg_alloc_register(block,&lit);
	if (!retval)
	{
		clog_free(lit->value.string.str);
		clog_free(lit);
		clog_cg_out_of_memory();
		return NULL;
	}

	++block->temp_counter;

	return retval;
}

static struct clog_cg_block_register* clog_cg_find_register(struct clog_cg_block* block, struct clog_ast_literal* id, int recursive)
{
	unsigned int i = 0;
	for (;i < block->register_count;++i)
	{
		if (clog_ast_literal_compare(block->registers[i].id,id) == 0)
			return &block->registers[i];
	}

	if (recursive && block->prev)
		return clog_cg_find_register(block->prev,id,1);

	return NULL;
}

static int clog_cg_alloc_result(struct clog_cg_block* block, struct clog_ast_literal* id, struct clog_cg_triplet* triplet)
{
	struct clog_cg_block_register* reg = id ? clog_cg_find_register(block,id,1) : clog_cg_alloc_temp_register(block);
	if (!reg)
		return 0;

	triplet->val.expr.result.prev_use = reg->last_use;
	triplet->val.expr.result.refcount = 0;
	reg->last_use = triplet;

	return 1;
}

static struct clog_cg_register* clog_cg_emit_triplet_IRL(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_cg_register* reg, struct clog_ast_literal* lit)
{
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	if (!clog_cg_alloc_result(block,id,triplet))
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].reg = reg;
	triplet->val.expr.args[1].lit = lit;

	++reg->refcount;

	return &triplet->val.expr.result;
}

static struct clog_cg_register* clog_cg_emit_triplet_IRR(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_cg_register* reg0, struct clog_cg_register* reg1)
{
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	if (!clog_cg_alloc_result(block,id,triplet))
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].reg = reg0;
	triplet->val.expr.args[1].reg = reg1;

	++reg0->refcount;
	++reg1->refcount;

	return &triplet->val.expr.result;
}

static int clog_cg_alloc_block(struct clog_cg_block** block)
{
	*block = clog_malloc(sizeof(struct clog_cg_block));
	if (!*block)
		return clog_cg_out_of_memory();

	memset(*block,0,sizeof(struct clog_cg_block));

	return 1;
}







int clog_codegen(const struct clog_ast_statement_list* list)
{
	for (;list;list=list->next)
	{

	}

	return 1;
}
