#ifndef L1cache_H
#define L1cache_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
//#include<iostream.h>

#define FALSE			0
#define TRUE			1

#define	KILO			1024
#define MEGA			(KILO * KILO)

//Define the data cache
#define	INS_WAY			    2
#define	INS_SETS			(16 * KILO)
#define	DATA_WAY			4
#define	DATA_SETS			(16 * KILO)
#define	ADDRESS				32
#define	BYTE_LINES			64

//Address design
#define	DATA_OFFSET_BITS	6
#define	DATA_SET_BITS		14
#define	DATA_TAG_BITS		12
#define	INS_OFFSET_BITS	    6
#define	INS_SET_BITS		14
#define	INS_TAG_BITS		12

//Trace File Format
//	Where n is:			n
#define	L1_READ_DATA	0	// read data request to L1 data cache
#define	L1_WRITE_DATA	1	// write data request to L1 data cache
#define	L1_READ_INST	2	// instruction fetch (a read request to L1 instruction cache)
#define	L2_EVICT		3	// invalidate command from L2
#define	RESET			8	// clear the cache and reset all state (and statistics)
#define	PRINT			9	// print contents and state of the cache (allow subsequent trace activity)



// Protocol
#define	M				0
#define	V				1
#define	I				2

struct Cache{
	char state; // MVI protocol
	char valid;
	int lru; // LRU value
	uint32_t address;
	uint32_t tag;
	uint32_t set;
	uint8_t byte_offset;
};

struct Stats{
	int Cache_Read;
	int Cache_Write;
	int Cache_Hit;
	int Cache_Miss;	
};	

void AddressSplit();


#endif
