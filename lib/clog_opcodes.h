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
	clog_opcode_ADD_L,
	clog_opcode_ADD_R,
	clog_opcode_SUB_L,
	clog_opcode_SUB_R,
	clog_opcode_MUL_L,
	clog_opcode_MUL_R,
};

#endif /* CLOG_OPCODES_H_ */
