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
	struct clog_cg_triplet* first;
	struct clog_cg_triplet* last;
	enum clog_ast_literal_type type;
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

static void* clog_cg_error(const char* part1, const unsigned char* part2, unsigned long line)
{
	printf("%s",part1);
	if (part2)
		printf("%s",part2);

	printf(" at line %lu\n",line);
	return NULL;
}

static void* clog_cg_warning(const char* part1, const unsigned char* part2, unsigned long line)
{
	printf("%s",part1);
	if (part2)
		printf("%s",part2);

	printf(" at line %lu\n",line);
	return NULL;
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
			if (triplet->type == clog_cg_triplet_expr)
				clog_free(triplet->val.expr.result);

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

	block->registers[block->register_count].first = NULL;
	block->registers[block->register_count].last = NULL;
	block->registers[block->register_count].type = clog_ast_literal_null;
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

	return clog_cg_error("Undeclared identifier ",id->value.string.str,id->line);
}

static struct clog_cg_block_register* clog_cg_alloc_result(struct clog_cg_block* block, struct clog_ast_literal* id, struct clog_cg_triplet* triplet)
{
	struct clog_cg_block_register* reg;
	struct clog_cg_register* result = clog_malloc(sizeof(struct clog_cg_register));
	if (!result)
	{
		clog_cg_out_of_memory();
		return NULL;
	}

	memset(result,0,sizeof(struct clog_cg_register));

	reg = id ? clog_cg_find_register(block,id,1) : clog_cg_alloc_temp_register(block);
	if (!reg)
	{
		clog_free(result);
		return NULL;
	}

	result->prev = reg->last;
	result->next = NULL;
	result->refcount = 0;

	if (reg->last)
		result->next = triplet;
	reg->last = triplet;

	if (!reg->first)
		reg->first = triplet;

	triplet->type = clog_cg_triplet_expr;
	triplet->val.expr.result = result;

	return reg;
}

static struct clog_cg_register* clog_cg_get_register(struct clog_cg_block_register* reg)
{
	return reg->last->val.expr.result;
}

static struct clog_cg_block_register* clog_cg_emit_triplet_op_L(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_ast_literal** lit)
{
	struct clog_cg_block_register* retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (!retval)
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].lit = *lit;
	*lit = NULL;

	triplet->val.expr.args[1].lit = NULL;

	return retval;
}

static struct clog_cg_block_register* clog_cg_emit_triplet_op_R(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_cg_block_register* reg)
{
	struct clog_cg_block_register* retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (!retval)
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].reg = clog_cg_get_register(reg);

	++triplet->val.expr.args[0].reg->refcount;

	return retval;
}

static struct clog_cg_block_register* clog_cg_emit_triplet_op_RL(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_cg_block_register* reg, struct clog_ast_literal** lit)
{
	struct clog_cg_block_register* retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (!retval)
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].reg = clog_cg_get_register(reg);
	triplet->val.expr.args[1].lit = *lit;
	*lit = NULL;

	++triplet->val.expr.args[0].reg->refcount;

	return retval;
}

static struct clog_cg_block_register* clog_cg_emit_triplet_op_RR(struct clog_cg_block* block, struct clog_ast_literal* id, enum clog_opcode op, struct clog_cg_block_register* reg0, struct clog_cg_block_register* reg1)
{
	struct clog_cg_block_register* retval;
	struct clog_cg_triplet* triplet;
	if (!clog_cg_alloc_triplet(block,&triplet))
		return NULL;

	retval = clog_cg_alloc_result(block,id,triplet);
	if (!retval)
	{
		clog_cg_free_triplet(block,triplet);
		return NULL;
	}

	triplet->op = op;
	triplet->val.expr.args[0].reg = clog_cg_get_register(reg0);
	triplet->val.expr.args[1].reg = clog_cg_get_register(reg1);

	++triplet->val.expr.args[0].reg->refcount;
	++triplet->val.expr.args[1].reg->refcount;

	return retval;
}

static int clog_cg_alloc_block(struct clog_cg_block** block)
{
	*block = clog_malloc(sizeof(struct clog_cg_block));
	if (!*block)
		return clog_cg_out_of_memory();

	memset(*block,0,sizeof(struct clog_cg_block));

	return 1;
}

static struct clog_cg_block_register* clog_cg_emit_expression(struct clog_cg_block* block, struct clog_ast_expression* expr);

static struct clog_cg_block_register* clog_cg_emit_expression_arg(struct clog_cg_block* block, struct clog_ast_expression* arg)
{
	if (arg->type == clog_ast_expression_builtin || arg->type == clog_ast_expression_call)
		return clog_cg_emit_expression(block,arg);

	if (arg == clog_ast_expression_identifier)
		return clog_cg_find_register(block,arg->expr.identifier,1);

	return NULL;
}

static int clog_cg_literal_add(struct clog_ast_literal* lit1, struct clog_ast_literal* lit2)
{
	if (lit1->type == clog_ast_literal_string && lit2->type == clog_ast_literal_string)
	{
		if (!lit1->value.string.len)
		{
			/* Swap */
			struct clog_ast_literal* l = lit2;
			lit2 = lit1;
			lit1 = l;
		}

		if (lit2->value.string.len)
		{
			unsigned char* sz = clog_realloc(lit1->value.string.str,lit1->value.string.len + lit2->value.string.len);
			if (!sz)
			{
				clog_cg_out_of_memory();
				return 0;
			}

			memcpy(sz+lit1->value.string.len,lit2->value.string.str,lit2->value.string.len);
			lit1->value.string.str = sz;
			lit1->value.string.len += lit2->value.string.len;
		}

		return 1;
	}
	else if (lit1->type != clog_ast_literal_string && lit2->type != clog_ast_literal_string)
	{
		if (!clog_ast_literal_arith_convert(lit1,lit2))
		{
			clog_cg_error("+ requires numbers or strings",NULL,lit1->line);
			return 0;
		}

		if (lit1->type == clog_ast_literal_real)
			lit1->value.real += lit2->value.real;
		else
			lit1->value.integer += lit2->value.integer;

		return 1;
	}
	else
	{
		void* TODO;
		return 0;
	}
}

static struct clog_cg_block_register* clog_cg_emit_builtin(struct clog_cg_block* block, struct clog_ast_expression_builtin* expr)
{
	struct clog_cg_block_register* reg0 = NULL;
	struct clog_cg_block_register* reg1 = NULL;

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
		reg0 = clog_cg_emit_expression_arg(block,expr->args[0]);
		if (!reg0)
			return NULL;
		break;

	default:
		reg0 = clog_cg_emit_expression_arg(block,expr->args[0]);
		if (!reg0)
			return NULL;

		if (expr->args[1])
		{
			reg1 = clog_cg_emit_expression_arg(block,expr->args[1]);
			if (!reg1)
				return NULL;
		}
		break;
	}

	switch (expr->type)
	{
	case CLOG_TOKEN_DOT:
	case CLOG_TOKEN_IN:
	case CLOG_TOKEN_OPEN_BRACKET:
	case CLOG_TOKEN_COMMA:
	case CLOG_TOKEN_EXCLAMATION:
		break;

	case CLOG_TOKEN_PLUS:
		if (!reg0 && reg1)
		{
			reg0 = reg1;
			reg1 = NULL;
		}
		if (!reg0)
		{
			/* Literal + Literal */
			if (!clog_cg_literal_add(expr->args[0]->expr.literal,expr->args[1]->expr.literal))
				return NULL;
			return clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,&expr->args[0]->expr.literal);
		}
		else if (!reg1)
		{
			/* Register + Literal */
			return clog_cg_emit_triplet_op_RL(block,NULL,clog_opcode_ADD_L,reg0,&expr->args[1]->expr.literal);
		}
		/* Register + Register */
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_ADD_R,reg0,reg1);

	case CLOG_TOKEN_MINUS:
		if (!expr->args[1])
		{
			/* Unary - */
			if (reg0)
				return clog_cg_emit_triplet_op_R(block,NULL,clog_opcode_NEG,reg0);

			/* - Literal */
			if (expr->args[0]->expr.literal->type == clog_ast_literal_integer)
				expr->args[0]->expr.literal->value.integer = -expr->args[0]->expr.literal->value.integer;
			else if (expr->args[0]->expr.literal->type == clog_ast_literal_real)
				expr->args[0]->expr.literal->value.real = -expr->args[0]->expr.literal->value.real;
			else
				return clog_cg_error("- requires a number",NULL,expr->args[0]->expr.literal->line);

			return clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,&expr->args[0]->expr.literal);
		}
		else
		{
			if (!reg0 && !reg1)
			{
				/* Literal - Literal */
				if (!clog_ast_literal_arith_convert(expr->args[0]->expr.literal,expr->args[1]->expr.literal))
				{
					clog_cg_error("- requires numbers",NULL,expr->args[0]->expr.literal->line);
					return NULL;
				}
				if (expr->args[0]->expr.literal->type == clog_ast_literal_real)
					expr->args[0]->expr.literal->value.real -= expr->args[1]->expr.literal->value.real;
				else
					expr->args[0]->expr.literal->value.integer -= expr->args[1]->expr.literal->value.integer;

				return clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,&expr->args[0]->expr.literal);
			}

			if (!reg1)
			{
				/* Register - Literal */
				return clog_cg_emit_triplet_op_RL(block,NULL,clog_opcode_SUB_L,reg0,&expr->args[1]->expr.literal);
			}

			if (!reg0)
			{
				/* Literal - Register */
				reg0 = clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,&expr->args[0]->expr.literal);
				if (!reg0)
					return NULL;
			}
			/* Register - Register */
			return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_SUB_R,reg0,reg1);
		}

	case CLOG_TOKEN_STAR:
		if (!reg0 && reg1)
		{
			reg0 = reg1;
			reg1 = NULL;
		}
		if (!reg0)
		{
			/* Literal * Literal */
			if (!clog_ast_literal_arith_convert(expr->args[0]->expr.literal,expr->args[1]->expr.literal))
			{
				clog_cg_error("* requires numbers",NULL,expr->args[0]->expr.literal->line);
				return NULL;
			}
			if (expr->args[0]->expr.literal->type == clog_ast_literal_real)
				expr->args[0]->expr.literal->value.real *= expr->args[1]->expr.literal->value.real;
			else
				expr->args[0]->expr.literal->value.integer *= expr->args[1]->expr.literal->value.integer;
			return clog_cg_emit_triplet_op_L(block,NULL,clog_opcode_LOAD,&expr->args[0]->expr.literal);
		}
		else if (!reg1)
		{
			/* Register * Literal */
			return clog_cg_emit_triplet_op_RL(block,NULL,clog_opcode_MUL_L,reg0,&expr->args[1]->expr.literal);
		}
		/* Register * Register */
		return clog_cg_emit_triplet_op_RR(block,NULL,clog_opcode_MUL_R,reg0,reg1);

	case CLOG_TOKEN_SLASH:

	case CLOG_TOKEN_TILDA:
		break;

	case CLOG_TOKEN_PERCENT:
	case CLOG_TOKEN_RIGHT_SHIFT:
	case CLOG_TOKEN_LEFT_SHIFT:

	case CLOG_TOKEN_LESS_THAN:
	case CLOG_TOKEN_LESS_THAN_EQUALS:
	case CLOG_TOKEN_GREATER_THAN:
	case CLOG_TOKEN_GREATER_THAN_EQUALS:
	case CLOG_TOKEN_EQUALS:
	case CLOG_TOKEN_AND:
	case CLOG_TOKEN_OR:
	case CLOG_TOKEN_BAR:
	case CLOG_TOKEN_CARET:
	case CLOG_TOKEN_AMPERSAND:
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

	return NULL;
}

static struct clog_cg_block_register* clog_cg_emit_call(struct clog_cg_block* block, struct clog_ast_expression_call* call)
{
	return NULL;
}

static struct clog_cg_block_register* clog_cg_emit_expression(struct clog_cg_block* block, struct clog_ast_expression* expr)
{
	switch (expr->type)
	{
	case clog_ast_expression_identifier:
		clog_cg_warning("Statement with no effect",NULL,expr->expr.identifier->line);
		return NULL;

	case clog_ast_expression_literal:
		clog_cg_warning("Statement with no effect",NULL,expr->expr.literal->line);
		return NULL;

	case clog_ast_expression_builtin:
		return clog_cg_emit_builtin(block,expr->expr.builtin);

	case clog_ast_expression_call:
		return clog_cg_emit_call(block,expr->expr.call);
	}

	return NULL;
}

static int clog_cg_emit_statement(struct clog_cg_block* block, struct clog_ast_statement* stmt);

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
		if (!clog_cg_emit_statement(block2,list->stmt))
			return 0;

		/* Ensure we append to the last block */
		while (block2->next)
			block2 = block2->next;
	}

	return 1;
}

static int clog_cg_emit_statement(struct clog_cg_block* block, struct clog_ast_statement* stmt)
{
	switch (stmt->type)
	{
	case clog_ast_statement_expression:
		return clog_cg_emit_expression(block,stmt->stmt.expression) != NULL;

	case clog_ast_statement_block:
		return clog_cg_emit_block(block,stmt->stmt.block);

	case clog_ast_statement_declaration:
	case clog_ast_statement_constant:
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
		if (!clog_cg_emit_statement(block2,list->stmt))
			return 0;

		/* Ensure we append to the last block */
		while (block2->next)
			block2 = block2->next;
	}

	/* Now do something with block! */

	return 1;
}
