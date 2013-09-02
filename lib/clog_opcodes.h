/*
 * clog_opcodes.h
 *
 *  Created on: 31 Aug 2013
 *      Author: rick
 */

#ifndef CLOG_OPCODES_H_
#define CLOG_OPCODES_H_

#include <stddef.h>
#include <limits.h>

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffUL)
#endif
#ifndef uint32_t
#if (ULONG_MAX == UINT32_MAX)
  typedef unsigned long uint32_t;
#elif (UINT_MAX == UINT32_MAX)
  typedef unsigned int uint32_t;
#elif (USHRT_MAX == UINT32_MAX)
  typedef unsigned short uint32_t;
#else
#error "Platform not supported"
#endif
#endif

/* Instructions are all 32bits
 */


enum clog_opcode
{
	clog_opcode_NOP = 0,

	clog_opcode_ARITH = 1,	/* Arithmetic opcodes, format: 4 bits, A,B,C */

	clog_opcode_MAX
};

#endif /* CLOG_OPCODES_H_ */
