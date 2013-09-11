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
	clog_opcode_MOV,
	clog_opcode_LOAD,
	clog_opcode_NEG,
	clog_opcode_ADD,
	clog_opcode_SUB,
	clog_opcode_MUL,
	clog_opcode_DIV,
	clog_opcode_MOD,
	clog_opcode_RSH,
	clog_opcode_LSH,
	clog_opcode_NOT,
};

#endif /* CLOG_OPCODES_H_ */
