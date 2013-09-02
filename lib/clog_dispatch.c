/*
 * clog_dispatch.c
 *
 *  Created on: 31 Aug 2013
 *      Author: rick
 */


#include "clog_opcodes.h"



#define DISPATCH() \
	switch ((enum clog_opcode)(code[ip] % clog_opcode_MAX)) \
	{ \
	case clog_opcode_MAX: \
	case clog_opcode_NOP: goto NOP; \
	case clog_opcode_ARITH: goto ARITH; \
	}

struct clog_vm_string
{
	unsigned int refcount;

};

struct clog_vm_value
{

};

enum clog_vm_value_type
{
	clog_vm_value_string,
	clog_vm_value_integer,
	clog_vm_value_real,
	clog_vm_value_pointer,
};

struct clog_vm_state
{
	struct clog_vm_value* accum;
	enum clog_vm_value_type accum_type;


};
