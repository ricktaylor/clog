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

void run(const uint32_t* code)
{
	size_t ip = 0;

NOP:
	DISPATCH();

ARITH:
	DISPATCH();


}
