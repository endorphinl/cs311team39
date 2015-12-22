/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   CS311 KAIST                                               */
/*   util.h                                                    */
/*                                                             */
/***************************************************************/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FALSE 0
#define TRUE  1

/* Basic Information */
#define MEM_TEXT_START	0x00400000
#define MEM_TEXT_SIZE	0x00100000
#define MEM_DATA_START	0x10000000
#define MEM_DATA_SIZE	0x00100000
#define MIPS_REGS	32
#define BYTES_PER_WORD	4
#define PIPE_STAGE	5

typedef struct CPU_State_Struct {
    uint32_t PC;		/* program counter */
    uint32_t REGS[MIPS_REGS];	/* register file */
    uint32_t PIPE[PIPE_STAGE];	/* pipeline stage */
} CPU_State;

typedef struct inst_s {
    short opcode;
    
    /*R-type*/
    short func_code;

    union {
        /* R-type or I-type: */
        struct {
	    unsigned char rs;
	    unsigned char rt;

	    union {
	        short imm;

	        struct {
                unsigned char rd;
                unsigned char shamt;
            } r;
	    } r_i;
	} r_i;
        /* J-type: */
        uint32_t target;
    } r_t;

    uint32_t value;
    
    //int32 encoding;
    //imm_expr *expr;
    //char *source_line;
} instruction;

typedef struct pipelns {
    struct {
        int flushed;
        int stall;
        uint32_t pc;
        char *binary_inst;
    } if_id;

    struct {
        int flushed;
        uint32_t pc;
        instruction *inst;

        unsigned char reg_rs;
        uint32_t val_rs;

        unsigned char reg_rt;
        uint32_t val_rt;

        struct {
            int signal;
        } mem_read;

        struct {
            int signal;
            uint32_t val_rs;
            uint32_t val_rt;
        } forwarded;
    } id_ex;

    struct {
        unsigned char reg_rs;
        uint32_t val_rs;

        unsigned char reg_rt;
        uint32_t val_rt;

        struct {
            int signal;
            uint32_t address;
            unsigned char reg_rt; //for lw (not for sw)
            uint32_t val_rt; //for sw (not for lw)
        } mem;

        struct {
            int signal;
            unsigned char reg_rd; // use this instead of just inst.blah.blah.rd because jal's rd is implicit (reg31)
            uint32_t val_rd;
        } wb;

        uint32_t pc;
        instruction *inst;
    } ex_mem;

    struct {
        int signal;
        unsigned char reg_rd;
        uint32_t val_rd;

        uint32_t pc;
        instruction *inst;
    } mem_wb;
} pipeln;



typedef struct {
    uint32_t start, size;
    uint8_t *mem;
} mem_region_t;

/* For PC * Registers */
extern CPU_State CURRENT_STATE;

/* For Instructions */
extern instruction *INST_INFO;
extern int NUM_INST;

/* For Pipeline Latches */
extern pipeln PIPELN;

/* For Memory Regions */
extern mem_region_t MEM_REGIONS[2];

/* For Execution */
extern int RUN_BIT;	/* run bit */
extern int INSTRUCTION_COUNT;

/* Functions */
char**		str_split(char *a_str, const char a_delim);
int		fromBinary(char *s);
uint32_t	mem_read_32(uint32_t address);
void		mem_write_32(uint32_t address, uint32_t value);
void		cycle();
void		run(int num_cycles);
void		go();
void		mdump(int start, int stop);
void		rdump();
void		init_memory();
void		init_inst_info();
void        init_pipeline_latches();

/* YOU IMPLEMENT THIS FUNCTION */
void	process_instruction();

#endif
