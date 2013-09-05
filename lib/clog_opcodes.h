/*
 * clog_opcodes.h
 *
 *  Created on: 31 Aug 2013
 *      Author: rick
 */

#ifndef CLOG_OPCODES_H_
#define CLOG_OPCODES_H_

enum clog_opcode
{
	clog_opcode_NOP = 0,

	clog_opcode_ARITH = 1,	/* Arithmetic opcodes, format: 4 bits, A,B,C */

	clog_opcode_MAX
};

#endif /* CLOG_OPCODES_H_ */
