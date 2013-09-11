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

#define CLOG_CG_ERROR -1

struct clog_cg_triplet;

struct clog_cg_register
{
	struct clog_cg_triplet* prev;
	struct clog_cg_triplet* next;
	unsigned int refcount;
};

struct clog_cg_block;

struct clog_cg_triplet
{
	enum
	{
		clog_cg_triplet_expr,
		clog_cg_triplet_jmp,
		clog_cg_triplet_phi,
	} type;

	enum clog_opcode op;

	union clog_cg_triplet_u0
	{
		struct clog_cg_triplet_expr
		{
			struct clog_cg_register* result;
			union clog_cg_triplet_u1
			{
				unsigned int reg[2];
				const struct clog_ast_literal* lit;
			} expr;
		} expr;
		struct clog_cg_block* jmp;
		struct clog_cg_triplet_phi
		{
			unsigned int regs[2];
		} phi;
	} val;
};

struct clog_cg_block_register
{
	struct clog_ast_literal* id;
	struct clog_cg_triplet* first;
	struct clog_cg_triplet* last;
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

static unsigned int clog_cg_out_of_memory()
{
	printf("Out of memory during code generation\n");
	return CLOG_CG_ERROR;
}

static unsigned int clog_cg_error(const char* part1, const unsigned char* part2, unsigned long line)
{
	printf("Error: %s",part1);
	if (part2)
		printf("%s",part2);

	printf(" at line %lu\n",line);
	return CLOG_CG_ERROR;
}

static void clog_cg_warning(const char* part1, const unsigned char* part2, unsigned long line)
{
	printf("Warning: %s",part1);
	if (part2)
		printf("%s",part2);

	printf(" at line %lu\n",line);
}

static int clog_cg_alloc_triplet(struct clog_cg_block* block, struct clog_cg_triplet** triplet)
{
	struct clog_cg_triplet* new_triplet = clog_malloc(sizeof(struct clog_cg_triplet));
	if (!new_triplet)
	{
		clog_cg_out_of_memory();
		return 0;
	}

	memset(new_triplet,0,sizeof(struct clog_cg_triplet));

	if (block->triplet_count == block->triplet_alloc)
	{
		/* Resize array */
		unsigned int new_size = (block->triplet_alloc == 0 ? 4 : block->triplet_alloc * 2);
		struct clog_cg_triplet** new = clog_realloc(block->triplets,new_size * sizeof(struct clog_cg_triplet*));
		if (!new)
		{
			clog_cg_out_of_memory();
			return 0;
		}

		block->triplet_alloc = new_size;
		block->triplets = new;
	}

	block->triplets[block->triplet_count++] = new_triplet;
	*triplet = new_triplet;

	return 1;
}

static void clog_cg_free_triplet(struct clog_cg_block* block, struct clog_cg_triplet* triplet)
{
	unsigned int i = block->triplet_count;
	while (i-- > 0)
	{
		if (triplet == block->triplets[i])
		{
			if (triplet->type == clog_cg_triplet_expr)
				clog_free(triplet->val.expr.result);

			--block->triplet_count;
			if (i < block->triplet_count)
				memmove(&block->triplets[i],&block->triplets[i+1],(block->triplet_count - i) * sizeof(struct clog_cg_triplet*));
			break;
		}
	}
}

static unsigned int clog_cg_alloc_register(struct clog_cg_block* block, const struct clog_ast_literal* id)
{
	if (block->register_count == block->register_alloc)
	{
		/* Resize array */
		unsigned int new_size = (block->register_alloc == 0 ? 4 : block->register_alloc * 2);
		struct clog_cg_block_register* new = clog_realloc(block->registers,new_size * sizeof(struct clog_cg_block_register));
		if (!new)
			return clog_cg_out_of_memory();

		block->register_alloc = new_size;
		block->registers = new;
	}

	block->registers[block->register_count].first = NULL;
	block->registers[block->register_count].last = NULL;

	if (!clog_ast_literal_clone(NULL,&block->registers[block->register_count].id,id))
		return CLOG_CG_ERROR;

	return block->register_count++;
}

static unsigned int clog_cg_alloc_temp_register(struct clog_cg_block* block)
{
	unsigned int retval;

	struct clog_ast_literal* lit;
	char szBuf[sizeof(block->temp_counter) * 2 + 2] = {0};
	sprintf(szBuf,"$%x",block->temp_counter);

	lit = clog_malloc(sizeof(struct clog_ast_literal));
	if (!lit)
		return clog_cg_out_of_memory();

	lit->line = 0;
	lit->type = clog_ast_literal_string;
	lit->value.string.len = strlen(szBuf);
	lit->value.string.str = clog_malloc(lit->value.string.len+1);
	if (!lit->value.string.str)
	{
		clog_free(lit);
		return clog_cg_out_of_memory();
	}

	memcpy(lit->value.string.str,szBuf,lit->value.string.len);
	lit->value.string.str[lit->value.string.len] = 0;

	retval = clog_cg_alloc_register(block,lit);
	if (retval == CLOG_CG_ERROR)
	{
		clog_free(lit->value.string.str);
		clog_free(lit);
	}
	else
		++block->temp_counter;

	return retval;
}

static unsigned int clog_cg_find_register(struct clog_cg_block* block, const struct clog_ast_literal* id, int recursive)
{
	unsigned int i = 0;
	for (;i < block->register_count;++i)
	{
		if (clog_ast_literal_id_compare(block->registers[i].id,id) == 0)
			return i;
	}

	if (recursive && block->prev)
		return clog_cg_find_register(block->prev,id,1);

	return CLOG_CG_ERROR;
}

static unsigned int clog_cg_alloc_result(struct clog_cg_block* block, const struct clog_ast_literal* id, struct clog_cg_triplet* triplet)
{
	unsigned int reg_idx;
	struct clog_cg_register* result = clog_malloc(sizeof(struct clog_cg_register));
	if (!result)
		return clog_cg_out_of_memory();

	memset(result,0,sizeof(struct clog_cg_register));

	if (id)
	{
		reg_idx = clog_cg_find_register(block,id,1);
		if (reg_idx == CLOG_CG_ERROR)
			clog_cg_error("Undeclared identifier ",id->value.string.str,id->line);
	}
	else
		reg_idx = clog_cg_alloc_temp_register(block);
	if (reg_idx == CLOG_CG_ERROR)
	{
		clog_free(result);
		return CLOG_CG_ERROR;
	}

	result->prev = block->registers[reg_idx].last;
	result->next = NULL;
	result->refcount = 0;

	if (block->registers[reg_idx].last)
		result->next = triplet;

	block->registers[reg_idx].last = triplet;

	if (!block->registers[reg_idx].first)
		block->registers[reg_idx].first = triplet;

	triplet->type = clog_cg_triplet_expr;
	triplet->val.expr.result = result;

	return reg_idx;
}

static struct clog_cg_register* clog_cg_get_register(struct clog_cg_block* block, unsigned int reg_idx)
{
	return block->registers[reg_idx].last->val.expr.result;
}

static const char* ___dump_op(enum clog_opcode op)
{
	switch (op)
	{
	case clog_opcode_MOV:
		return "MOV";
	case clog_opcode_LOAD:
		return "LOAD";
	case clog_opcode_NEG:
		return "NEG";
	case clog_opcode_ADD:
		return "ADD";
	case clog_opcode_SUB:
		return "SUB";
	case clog_opcode_MUL:
		return "MUL";
	case clog_opcode_DIV:
		return "DIV";
	case clog_opcode_MOD:
		return "MOD";
	case clog_opcode_RSH:
		return "RSH";
	case clog_opcode_LSH:
		return "LSH";
	case clog_opcode_NOT:
		return "NOT";
	default:
		return "???";
	}
}

static unsigned int clog_cg_emit_triplet_op_L(struct clog_cg_block* block, const struct clog_ast_literal* id, enum clog_opcode op, const struct clog_ast_literal* lit)
{
	unsigned int retval;
	struct clog_cg_triplet* triplet;

	if (!clog_cg_alloc_triplet(block,&triplet))
		return CLOG_CG_ERROR;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (retval == CLOG_CG_ERROR)
		clog_cg_free_triplet(block,triplet);
	else
	{
		triplet->op = op;
		triplet->val.expr.expr.lit = lit;

		printf("#%d = %s ",retval,___dump_op(op));
		switch (lit->type)
		{
		case clog_ast_literal_null:
			printf("null\n");
			break;

		case clog_ast_literal_bool:
			if (lit->value.integer)
				printf("true\n");
			else
				printf("false\n");
			break;

		case clog_ast_literal_integer:
			printf("%ld\n",lit->value.integer);
			break;

		case clog_ast_literal_real:
			printf("%g\n",lit->value.real);
			break;

		case clog_ast_literal_string:
			printf("%s\n",lit->value.string.str);
			break;
		}
	}
	return retval;
}

static unsigned int clog_cg_emit_triplet_op_R(struct clog_cg_block* block, const struct clog_ast_literal* id, enum clog_opcode op, unsigned int reg_idx)
{
	unsigned int retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return CLOG_CG_ERROR;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (retval == CLOG_CG_ERROR)
		clog_cg_free_triplet(block,triplet);
	else
	{
		triplet->op = op;
		triplet->val.expr.expr.reg[0] = reg_idx;
		triplet->val.expr.expr.reg[1] = -1;

		++clog_cg_get_register(block,reg_idx)->refcount;

		printf("#%d = %s #%d\n",retval,___dump_op(op),reg_idx);
	}
	return retval;
}

static unsigned int clog_cg_emit_triplet_op_RR(struct clog_cg_block* block, const struct clog_ast_literal* id, enum clog_opcode op, unsigned int reg_idx0, unsigned int reg_idx1)
{
	unsigned int retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return CLOG_CG_ERROR;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (retval == CLOG_CG_ERROR)
		clog_cg_free_triplet(block,triplet);
	else
	{
		triplet->op = op;
		triplet->val.expr.expr.reg[0] = reg_idx0;
		triplet->val.expr.expr.reg[1] = reg_idx1;

		++clog_cg_get_register(block,reg_idx0)->refcount;
		++clog_cg_get_register(block,reg_idx1)->refcount;

		printf("#%d = #%d %s #%d\n",retval,reg_idx0,___dump_op(op),reg_idx1);
	}
	return retval;
}

static int clog_cg_alloc_block(struct clog_cg_block** block)
{
	*block = clog_malloc(sizeof(struct clog_cg_block));
	if (!*block)
	{
		clog_cg_out_of_memory();
		return 0;
	}

	memset(*block,0,sizeof(struct clog_cg_block));

	return 1;
}

static unsigned int clog_cg_emit_builtin(struct clog_cg_block* block, struct clog_ast_expression_builtin* expr);
static unsigned int clog_cg_emit_call(struct clog_cg_block* block, struct clog_ast_expression_call* call);

static unsigned int clog_cg_emit_expression_arg(struct clog_cg_block* block, struct clog_ast_expression* arg)
{
	switch (arg->type)
	{
	case clog_ast_expression_builtin:
		return clog_cg_emit_builtin(block,arg->expr.builtin);

	case clog_ast_expression_call:
		return clog_cg_emit_call(block,arg->expr.call);

	case clog_ast_expression_identifier:
		{
			unsigned int reg_idx = clog_cg_find_register(block,arg->expr.identifier,1);
			if (reg_idx == CLOG_CG_ERROR)
				return clog_cg_error("Undeclared identifier ",arg->expr.identifier->value.string.str,arg->expr.identifier->line);
			return reg_idx;
		}

	case clog_ast_expression_literal:
		return clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,arg->expr.literal);
	}

	return CLOG_CG_ERROR;
}

static unsigned int clog_cg_emit_builtin(struct clog_cg_block* block, struct clog_ast_expression_builtin* expr)
{
	unsigned int reg_idx0;
	unsigned int reg_idx1;

	switch (expr->type)
	{
	case CLOG_TOKEN_DOT:
	case CLOG_TOKEN_IN:
	case CLOG_TOKEN_OPEN_BRACKET:
		/* Ummm...? */
		{ void* TODO; }
		break;

	case CLOG_TOKEN_AND:
	case CLOG_TOKEN_OR:
	case CLOG_TOKEN_QUESTION:
		reg_idx0 = clog_cg_emit_expression_arg(block,expr->args[0]);
		if (reg_idx0 == CLOG_CG_ERROR)
			return reg_idx0;
		break;

	default:
		reg_idx0 = clog_cg_emit_expression_arg(block,expr->args[0]);
		if (reg_idx0 == CLOG_CG_ERROR)
			return reg_idx0;
		if (expr->args[1])
		{
			reg_idx1 = clog_cg_emit_expression_arg(block,expr->args[1]);
			if (reg_idx1 == CLOG_CG_ERROR)
				return reg_idx1;
		}
		break;
	}

	switch (expr->type)
	{
	case CLOG_TOKEN_COMMA:
		/* Just return the left-most register */
		return reg_idx1;

	case CLOG_TOKEN_DOT:
	case CLOG_TOKEN_IN:
	case CLOG_TOKEN_OPEN_BRACKET:

	case CLOG_TOKEN_TILDA:
		break;

	case CLOG_TOKEN_PLUS:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_ADD,reg_idx0,reg_idx1);

	case CLOG_TOKEN_MINUS:
		if (!expr->args[1])
		{
			/* Unary - */
			return clog_cg_emit_triplet_op_R(block,NULL,clog_opcode_NEG,reg_idx0);
		}
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_SUB,reg_idx0,reg_idx1);

	case CLOG_TOKEN_STAR:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_MUL,reg_idx0,reg_idx1);

	case CLOG_TOKEN_SLASH:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_DIV,reg_idx0,reg_idx1);

	case CLOG_TOKEN_PERCENT:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_MOD,reg_idx0,reg_idx1);

	case CLOG_TOKEN_RIGHT_SHIFT:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_RSH,reg_idx0,reg_idx1);

	case CLOG_TOKEN_LEFT_SHIFT:
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_LSH,reg_idx0,reg_idx1);

	case CLOG_TOKEN_BAR:
	case CLOG_TOKEN_CARET:
	case CLOG_TOKEN_AMPERSAND:

	case CLOG_TOKEN_EXCLAMATION:
		return clog_cg_emit_triplet_op_R(block,NULL,clog_opcode_NOT,reg_idx0);

	case CLOG_TOKEN_LESS_THAN:
	case CLOG_TOKEN_LESS_THAN_EQUALS:
	case CLOG_TOKEN_GREATER_THAN:
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
	case CLOG_TOKEN_EQUALS:
	case CLOG_TOKEN_AND:
	case CLOG_TOKEN_OR:

	case CLOG_TOKEN_QUESTION:


	case CLOG_TOKEN_ASSIGN:
	case CLOG_TOKEN_DOUBLE_PLUS:
	case CLOG_TOKEN_DOUBLE_MINUS:
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
		break;
	}

	return CLOG_CG_ERROR;
}

static unsigned int clog_cg_emit_call(struct clog_cg_block* block, struct clog_ast_expression_call* call)
{
	return CLOG_CG_ERROR;
}

static int clog_cg_emit_statement(struct clog_cg_block* block, struct clog_ast_statement_list** list);

static int clog_cg_emit_block(struct clog_cg_block* block, struct clog_ast_statement_list* list)
{
	struct clog_cg_block* block2;
	if (!clog_cg_alloc_block(&block2))
		return 0;

	if (block)
		block->next = block2;

	block2->prev = block;

	for (;list;list = list->next)
	{
		if (!clog_cg_emit_statement(block2,&list))
			return 0;

		/* Ensure we append to the last block */
		while (block2->next)
			block2 = block2->next;
	}

	return 1;
}

static int clog_cg_emit_statement(struct clog_cg_block* block, struct clog_ast_statement_list** list)
{
	switch ((*list)->stmt->type)
	{
	case clog_ast_statement_expression:
		switch ((*list)->stmt->stmt.expression->type)
		{
		case clog_ast_expression_identifier:
			if (clog_cg_find_register(block,(*list)->stmt->stmt.expression->expr.identifier,1) == CLOG_CG_ERROR)
				return clog_cg_error("Undeclared identifier ",(*list)->stmt->stmt.expression->expr.identifier->value.string.str,(*list)->stmt->stmt.expression->expr.identifier->line);

			clog_cg_warning("Statement with no effect",NULL,(*list)->stmt->stmt.expression->expr.identifier->line);
			return 1;

		case clog_ast_expression_literal:
			clog_cg_warning("Statement with no effect",NULL,(*list)->stmt->stmt.expression->expr.literal->line);
			return 1;

		case clog_ast_expression_builtin:
			return (clog_cg_emit_builtin(block,(*list)->stmt->stmt.expression->expr.builtin) != CLOG_CG_ERROR);

		case clog_ast_expression_call:
			return (clog_cg_emit_call(block,(*list)->stmt->stmt.expression->expr.call) != CLOG_CG_ERROR);
		}
		return 0;

	case clog_ast_statement_block:
		return clog_cg_emit_block(block,(*list)->stmt->stmt.block);

	case clog_ast_statement_declaration:
		{
			unsigned int assign_idx;
			unsigned int reg_idx = clog_cg_find_register(block,(*list)->stmt->stmt.declaration,0);
			if (reg_idx != CLOG_CG_ERROR)
				return clog_cg_error("Duplicate declaration of ",(*list)->stmt->stmt.declaration->value.string.str,(*list)->stmt->stmt.declaration->line);

			assign_idx = clog_cg_emit_expression_arg(block,(*list)->next->stmt->stmt.expression->expr.builtin->args[1]);
			if (assign_idx == CLOG_CG_ERROR)
				return assign_idx;

			reg_idx = clog_cg_alloc_register(block,(*list)->stmt->stmt.declaration);
			if (reg_idx == CLOG_CG_ERROR)
				return reg_idx;

			reg_idx = clog_cg_emit_triplet_op_R(block,(*list)->stmt->stmt.declaration,clog_opcode_MOV,assign_idx);
			if (reg_idx != CLOG_CG_ERROR)
				*list = (*list)->next;
			return reg_idx;
		}

	case clog_ast_statement_if:
	case clog_ast_statement_do:
	case clog_ast_statement_break:
	case clog_ast_statement_continue:
	case clog_ast_statement_return:
		break;
	}

	return 1;
}


int clog_codegen(struct clog_ast_statement_list* list)
{
	struct clog_cg_block* block;
	struct clog_cg_block* block2;
	if (!clog_cg_alloc_block(&block))
		return 0;

	for (block2 = block;list;list = list->next)
	{
		if (!clog_cg_emit_statement(block2,&list))
			return 0;

		/* Ensure we append to the last block */
		while (block2->next)
			block2 = block2->next;
	}

	/* Now do something with block! */

	return 1;
}
