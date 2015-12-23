/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   CS311 KAIST                                               */
/*   run.c                                                     */
/*                                                             */
/***************************************************************/

#include <stdio.h>

#include "util.h"
#include "run.h"
#include "parse.h"

/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
char* get_inst_info(uint32_t pc) 
{ 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}
/*
instruction* get_inst_info(uint32_t pc) 
{ 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}
*/

uint32_t fetch(int no_bp_set)
{
    if(pipeln.if_id.flushed)
        pipeln.if_id.flushed = 0;
    else
    {
        pipeln.if_id.binary_inst = get_inst_info(CURRENT_STATE.PC);
        pipeln.if_id.pc = CURRENT_STATE.PC;

        //hazard unit
        instruction *temp;
        temp = parsing_instr(pipeln.if_id.binary_inst, (pipeln.if_id.pc - MEM_TEXT_START) >> 2);

        if(pipeln.id_ex.mem_read.signal == 1 && ((pipeln.id_ex.reg_rt == temp.r_t.r_i.rs) || (pipeln.id_ex.reg_rt == temp.r_t.r_i.rt)))
        {
            pipeln.if_id.proceed_and_stall = 0;
            pipeln.if_id.stall = 1;
            return pipeln.if_id.pc;
            //CURRENT_STATE.PC -= 4;
        }
        
        if(temp.opcode == 2 || temp.opcode == 3 || (temp.opcode == 0 && temp.func_code == 0x08))
        {
            if(pipeln.if_id.stall == 0 && pipeln.if_id.proceed_and_stall < 0)
                pipeln.if_id.proceed_and_stall = 0;
            else if(pipeln.if_id.stall == 0 && pipeln.if_id.proceed_and_stall == 0)
            {
                pipeln.if_id.proceed_and_stall = 1;
                pipeln.if_id.stall = 1;
            }
            return pipeln.if_id.pc;
        }

        if(no_bp_set)   //bp is not present
        {
            if(temp.opcode == 4 || temp.opcode == 5) //beq or bne
            {
                if(pipeln.if_id.stall == 0 && pipeln.if_id.proceed_and_stall < 0)
                    pipeln.if_id.proceed_and_stall = 0;
                else if(pipeln.if_id.stall == 0 && pipeln.if_id.proceed_and_stall == 0)
                {
                    pipeln.if_id.proceed_and_stall = 1;
                    pipeln.if_id.stall = 1;
                }
                return pipeln.if_id.pc;
                //CURRENT_STATE.PC -= 4;
            }
        }
        else            //bp is present
        {
            //bp unit
            if(temp.opcode == 4 || temp.opcode == 5) //beq or bne - always take branch
            {
                return pipeln.if_id.pc + (sign_ext_imm << 2) + 4;
                //CURRENT_STATE.PC = CURRENT_STATE.PC + (sign_ext_imm << 2);
                //CURRENT_STATE.PC = CURRENT_STATE.PC + (sign_ext_imm << 2) - 4; //-4 cuz cycle() has +4 at the end
            }
        }
    }
    return pipeln.if_id.pc + 4;
}

void decode()
{
    if(pipeln.id_ex.flushed)
        pipeln.id_ex.flushed = 0;
    else if(pipeln.if_id.proceed_and_stall == 0 && pipeln.if_id.stall > 0)
    {
        stall();
        pipeln.if_id.stall -= 1;
        pipeln.if_id.proceed_and_stall -= 1; //reset proceed_and_stall (nobp and jumps)
    }
    else
    {
        pipeln.if_id.proceed_and_stall -= 1; //reset proceed_and_stall (nobp and jumps)

        pipeln.id_ex.inst = parsing_instr(pipeln.if_id.binary_inst, (pipeln.if_id.pc - MEM_TEXT_START) >> 2);
        pipeln.id_ex.pc = pipeln.if_id.pc;

        if(pipeln.id_ex.inst.opcode == 35) // "lw"
            pipeln.id_ex.mem_read.signal = 1;

        //forwarding unit
        //rs
        pipeln.id_ex.reg_rs = pipeln.id_ex.inst.r_t.r_i.rs;
        pipeln.id_ex.val_rs = CURRENT_STATE.REGS[pipeln.id_ex.reg_rs];
        pipeln.id_ex.reg_rt = pipeln.id_ex.inst.r_t.r_i.rt;
        pipeln.id_ex.val_rt = CURRENT_STATE.REGS[pipeln.id_ex.reg_rt];

        if(pipeln.mem_wb.signal == 1 && pipeln.mem_wb.reg_rd != 0 && !(pipeln.ex_mem.wb.signal == 1 && pipeln.ex_mem.wb.reg_rd != 0 && pipeln.ex_mem.wb.reg_rd == pipeln.id_ex.reg_rs) && pipeln.mem_wb.reg_rd == pipeln.id_ex.reg_rs)
        {
            //mem_wb forwarding
            pipeln.id_ex.forwarded.signal_rs = 1;
            pipeln.id_ex.forwarded.val_rs = pipeln.mem_wb.val_rd;
        }
        else if(pipeln.ex_mem.wb.signal == 1 && pipeln.ex_mem.wb.reg_rd != 0 && pipeln.ex_mem.wb.reg_rd == pipeln.id_ex.reg_rs)
        {
            //ex_mem forwarding
            pipeln.id_ex.forwarded.signal_rs = 1;
            pipeln.id_ex.forwarded.val_rs = pipeln.ex_mem.wb.val_rd;
        }
        else
            pipeln.id_ex.forwarded.signal_rs = 0;

        //rt
        if(pipeln.mem_wb.signal == 1 && pipeln.mem_wb.reg_rd != 0 && !(pipeln.ex_mem.wb.signal == 1 && pipeln.ex_mem.wb.reg_rd != 0 && pipeln.ex_mem.wb.reg_rd == pipeln.id_ex.reg_rt) && pipeln.mem_wb.reg_rd == pipeln.id_ex.reg_rt)
        {
            //mem_wb forwarding
            pipeln.id_ex.forwarded.signal_rt = 1;
            pipeln.id_ex.forwarded.val_rt = pipeln.mem_wb.val_rd;
        }
        else if(pipeln.ex_mem.wb.signal == 1 && pipeln.ex_mem.wb.reg_rd != 0 && pipeln.ex_mem.wb.reg_rd == pipeln.id_ex.reg_rt)
        {
            //ex_mem forwarding
            pipeln.id_ex.forwarded.signal_rt = 1;
            pipeln.id_ex.forwarded.val_rt = pipeln.ex_mem.wb.val_rd;
        }
        else
            pipeln.id_ex.forwarded.signal_rt = 0;
    }
}

uint32_t execute(int no_bp_set)
{
    pipeln.ex_mem.inst = pipeln.id_ex.inst;
    pipeln.ex_mem.pc = pipeln.id_ex.pc;

    instruction *instr = pipeln.id_ex.inst;
    if(instr.opcode == 0 && instr.func_code == 0 && instr.r_t.r_i.rs == 0){
        pipeln.ex_mem.wb.signal = 0;
        pipeln.ex_mem.mem.signal = 0;
    }
    else if(instr.opcode == 0){
        if(pipeln.id_ex.forwarded.signal_rs)
            uint32_t val_rs = pipeln.id_ex.forwarded.val_rs;
        else
            uint32_t val_rs = pipeln.id_ex.val_rs;

        if(pipeln.id_ex.forwarded.signal_rt)
            uint32_t val_rt = pipeln.id_ex.forwarded.val_rt;
        else
            uint32_t val_rt = pipeln.id_ex.val_rt;
        unsigned char sa = pipeln.id_ex.inst.r_t.r_i.r_i.r.shamt;

        pipeln.ex_mem.wb.signal = 1;
        pipeln.ex_mem.mem.signal = 0;

        pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.inst.r_t.r_i.r_i.r.rd;
        /*
        unsigned char rs = instr.r_t.r_i.rs;
        unsigned char rt = instr.r_t.r_i.rt;
        unsigned char rd = instr.r_t.r_i.r_i.r.rd;
        unsigned char sa = instr.r_t.r_i.r_i.r.shamt;
        */

        if(instr.func_code == 0x21){//addu
            pipeln.ex_mem.wb.val_rd = val_rs + val_rt;
        } else if (instr.func_code == 0x23){//subu
            pipeln.ex_mem.wb.val_rd = val_rs - val_rt;
        } else if (instr.func_code == 0x24){//and
            pipeln.ex_mem.wb.val_rd = val_rs & val_rt;
        } else if (instr.func_code == 0x27){//nor
            pipeln.ex_mem.wb.val_rd = ~(val_rs | val_rt);
        } else if (instr.func_code == 0x25){//or
            pipeln.ex_mem.wb.val_rd = (val_rs | val_rt);
        } else if (instr.func_code == 0x00){//sll
            pipeln.ex_mem.wb.val_rd = val_rt << sa;
        } else if (instr.func_code == 0x02){//srl
            pipeln.ex_mem.wb.val_rd = val_rt >> sa;
        } else if (instr.func_code == 0x2b){//sltu
            if(val_rs < val_rt){
                pipeln.ex_mem.wb.val_rd = 1;
            } else {
                pipeln.ex_mem.wb.val_rd = 0;
            }
        } else if (instr.func_code == 0x08){//jr
            return val_rs;
            //flush();
            //CURRENT_STATE.PC = val_rs - 4; //-4 cuz cycle() has +4 at the end
        }
    }
    else if(instr.opcode == 2 || instr.opcode == 3)
    {
        uint32_t target = pipeln.id_ex.inst.r_t.target << 2;
        if(instr.opcode == 3) // jal
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.wb.reg_rd = 31;
            pipeln.ex_mem.wb.val_rd = CURRENT_STATE.PC + 4;
        }
        else
            pipeln.ex_mem.wb.signal = 0;

        pipeln.ex_mem.mem.signal = 0;

        return target;
        //flush();
        //CURRENT_STATE.PC = target - 4; //-4 cuz cycle() has +4 at the end
    }  
    else
    {  
        if(pipeln.id_ex.forwarded.signal_rs)
            uint32_t val_rs = pipeln.id_ex.forwarded.val_rs;
        else
            uint32_t val_rs = pipeln.id_ex.val_rs;

        if(pipeln.id_ex.forwarded.signal_rt)
            uint32_t val_rt = pipeln.id_ex.forwarded.val_rt;
        else
            uint32_t val_rt = pipeln.id_ex.val_rt;

        short imm = pipeln.id_ex.inst.r_t.r_i.r_i.imm;

        if(instr.opcode == 9) // "addiu"
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            //fromBinary only gets unsigned.

            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.val_rd = val_rs + sign_ext_imm;
            //rt = rs + signextimm
        }
        else if(instr.opcode == 12) // "andi"
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.mem.signal = 0;

            int zero_ext_imm = imm;
            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.val_rd = val_rs & zero_ext_imm;
            //rt = rs & zeroextimm
        }
        else if(instr.opcode == 4) // "beq"
        {
            pipeln.ex_mem.wb.signal = 0;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            if(no_bp_set)
            {
                if(val_rs == val_rt)//branch taken
                    return pipeln.id_ex.pc + (sign_ext_imm << 2) + 4;
                    //CURRENT_STATE.PC = pipeln.id_ex.pc + (sign_ext_imm << 2); //-4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc + (sign_ext_imm << 2) - 4; //-4 cuz cycle() has +4 at the end
                    //pc = pc + 4 + imm
                else                //branch not taken
                    return pipeln.id_ex.pc + 4;
                    //CURRENT_STATE.PC = pipeln.id_ex.pc; //no +4 cuz cycle() has +4 at the end
            }
            else
            {
                if(val_rs != val_rt) 
                {
                    //bp is that it is always taken so flush when beq is not met
                    flush();
                    return pipeln.id_ex.pc + 4; //no +4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc; //no +4 cuz cycle() has +4 at the end
                }
            }
        }
        else if(instr.opcode == 5) // "bne"
        {
            pipeln.ex_mem.wb.signal = 0;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            if(no_bp_set)
            {
                if(val_rs != val_rt)//branch taken
                    return pipeln.id_ex.pc + (sign_ext_imm << 2) + 4;
                    //CURRENT_STATE.PC = pipeln.id_ex.pc + (sign_ext_imm << 2); //-4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc + (sign_ext_imm << 2) - 4; //-4 cuz cycle() has +4 at the end
                else                //branch not taken
                    return pipeln.id_ex.pc + 4; //no +4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc; //no +4 cuz cycle() has +4 at the end
            }
            else
            {
                if(val_rs == val_rt)
                {
                    flush();
                    return pipeln.id_ex.pc + 4; //no +4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc; //no +4 cuz cycle() has +4 at the end
                    //CURRENT_STATE.PC = pipeln.id_ex.pc + (sign_ext_imm << 2) - 4; //-4 cuz cycle() has +4 at the end
                    //pc = pc + 4 + imm
                }
            }
        }
        else if(instr.opcode == 15) // "lui"
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.val_rd = zero_ext_imm << 16;
        }
        /*lwlwlwlwlwlwlwlw*/
        else if(instr.opcode == 35) // "lw"
        {
            pipeln.ex_mem.wb.signal = 1; //lw needs wb
            pipeln.ex_mem.mem.signal = 1;

            uint32_t sign_ext_imm = sign_extend(imm);
            pipeln.ex_mem.mem.reg_rt = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt; //for forwarding sake (otherwise the conditionals for forwarding becomes too complicated)
            pipeln.ex_mem.mem.address = val_rs + sign_ext_imm;

            //pipeln.ex_mem.wb.val_rd = mem_read_32(CURRENT_STATE.REGS[rs] + );
            //rt = mem_read_32(rs + signextimm)
        }
        else if(instr.opcode == 13) // "ori"
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.val_rd = val_rs | zero_ext_imm;
        }
        else if(instr.opcode == 11) // "sltiu"
        {
            pipeln.ex_mem.wb.signal = 1;
            pipeln.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            pipeln.ex_mem.wb.reg_rd = pipeln.id_ex.reg_rt;
            pipeln.ex_mem.wb.val_rd = val_rs < zero_ext_imm ? 1 : 0;
        }
        else if(instr.opcode == 43) // "sw"
        {
            pipeln.ex_mem.wb.signal = 0;
            pipeln.ex_mem.mem.signal = 1;

            uint32_t sign_ext_imm = sign_extend(imm);
            pipeln.ex_mem.mem.val_rt = val_rt;
            pipeln.ex_mem.mem.address = val_rs + sign_ext_imm;

            //mem_write_32(CURRENT_STATE.REGS[rs] + sign_ext_imm, CURRENT_STATE.REGS[rt]);
        }
    }
    return pipeln.id_ex.pc + 4;
}

void memory()
{
    if(pipeln.ex_mem.mem.signal)
    {
        if(pipeln.ex_mem.wb.signal) //lw
        {
            pipeln.mem_wb.signal = 1;
            pipeln.mem_wb.reg_rd = pipeln.ex_mem.mem.reg_rt;
            pipeln.mem_wb.val_rd = mem_read_32(pipeln.ex_mem.mem.address);
        }
        else                        //sw
            mem_write_32(pipeln.ex_mem.mem.address, pipeln.ex_mem.mem.val_rt);
    }
    else
    {
        pipeln.mem_wb.signal = pipeln.ex_mem.wb.signal;
        pipeln.mem_wb.reg_rd = pipeln.ex_mem.wb.reg_rd;
        pipeln.mem_wb.val_rd = pipeln.ex_mem.wb.val_rd;
    }
    pipeln.mem_wb.inst = pipeln.ex_mem.inst;
    pipeln.mem_wb.pc = pipeln.ex_mem.pc;
}

void write_back()
{
    if(pipeln.mem_wb.signal)
        CURRENT_STATE.REGS[pipeln.mem_wb.reg_rd] = pipeln.mem_wb.val_rd;
}

void flush()
{
    pipeln.if_id.flushed = 1;
    pipeln.id_ex.flushed = 1;

    //reset if_id
    pipeln.if_id.pc = 0;
    pipeln.if_id.binary_inst = 0;

    //reset id_ex
    pipeln.id_ex.pc = 0;
    pipeln.id_ex.reg_rs = 0;
    pipeln.id_ex.val_rs = 0;
    pipeln.id_ex.reg_rt = 0;
    pipeln.id_ex.val_rt = 0;
    pipeln.id_ex.forwarded.signal_rs = 0;
    pipeln.id_ex.forwarded.signal_rt = 0;
    pipeln.id_ex.forwarded.val_rs = 0;
    pipeln.id_ex.forwarded.val_rt = 0;

    //reset id_ex.inst
    pipeln.id_ex.inst.value = 0;
    pipeln.id_ex.inst.opcode = 0;
    pipeln.id_ex.inst.func_code = 0;
    pipeln.id_ex.inst.r_t.r_i.rs = 0;
    pipeln.id_ex.inst.r_t.r_i.rt = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.r.rd = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.imm = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.r.shamt = 0;
    pipeln.id_ex.inst.r_t.target = 0;
}

void stall()
{
    //reset id_ex
    pipeln.id_ex.pc = 0;
    pipeln.id_ex.reg_rs = 0;
    pipeln.id_ex.val_rs = 0;
    pipeln.id_ex.reg_rt = 0;
    pipeln.id_ex.val_rt = 0;
    pipeln.id_ex.forwarded.signal_rs = 0;
    pipeln.id_ex.forwarded.signal_rt = 0;
    pipeln.id_ex.forwarded.val_rs = 0;
    pipeln.id_ex.forwarded.val_rt = 0;

    //reset id_ex.inst
    pipeln.id_ex.inst.value = 0;
    pipeln.id_ex.inst.opcode = 0;
    pipeln.id_ex.inst.func_code = 0;
    pipeln.id_ex.inst.r_t.r_i.rs = 0;
    pipeln.id_ex.inst.r_t.r_i.rt = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.r.rd = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.imm = 0;
    pipeln.id_ex.inst.r_t.r_i.r_i.r.shamt = 0;
    pipeln.id_ex.inst.r_t.target = 0;
}

uint32_t sign_extend(short imm)
{
    uint32_t val = (0x0000FFFF & imm);
    int mask = 0x00008000;
    if(imm & mask)
        val += 0xFFFF0000;

    return val;
}
uint32_t zero_extend(short imm)
{
    uint32_t val = (0x0000FFFF & imm);
    return val;
}




/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/
void process_instruction(){
    instruction *inst;
    int i;		// for loop

    /* pipeline */
    for ( i = PIPE_STAGE - 1; i > 0; i--)
	CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
    CURRENT_STATE.PIPE[0] = CURRENT_STATE.PC;

    inst = get_inst_info(CURRENT_STATE.PC);
    CURRENT_STATE.PC += BYTES_PER_WORD;

    switch (OPCODE(inst))
    {
	case 0x9:		//(0x001001)ADDIU
	    CURRENT_STATE.REGS[RT (inst)] = CURRENT_STATE.REGS[RS (inst)] + (short) IMM (inst);
	    break;
	case 0xc:		//(0x001100)ANDI
	    CURRENT_STATE.REGS[RT (inst)] = CURRENT_STATE.REGS[RS (inst)] & (0xffff & IMM (inst));
	    break;
	case 0xf:		//(0x001111)LUI	
	    CURRENT_STATE.REGS[RT (inst)] = (IMM (inst) << 16) & 0xffff0000;
	    break;
	case 0xd:		//(0x001101)ORI
	    CURRENT_STATE.REGS[RT (inst)] = CURRENT_STATE.REGS[RS (inst)] | (0xffff & IMM (inst));
	    break;
	case 0xb:		//(0x001011)SLTIU 
	    {
		int x = (short) IMM (inst);

		if ((uint32_t) CURRENT_STATE.REGS[RS (inst)] < (uint32_t) x)
		    CURRENT_STATE.REGS[RT (inst)] = 1;
		else
		    CURRENT_STATE.REGS[RT (inst)] = 0;
		break;
	    }
	case 0x23:		//(0x100011)LW	
	    LOAD_INST (&CURRENT_STATE.REGS[RT (inst)], mem_read_32((CURRENT_STATE.REGS[BASE (inst)] + IOFFSET (inst))), 0xffffffff);
	    break;
	case 0x2b:		//(0x101011)SW
	    mem_write_32(CURRENT_STATE.REGS[BASE (inst)] + IOFFSET (inst), CURRENT_STATE.REGS[RT (inst)]);
	    break;
	case 0x4:		//(0x000100)BEQ
	    BRANCH_INST (CURRENT_STATE.REGS[RS (inst)] == CURRENT_STATE.REGS[RT (inst)], CURRENT_STATE.PC + IDISP (inst), 0);
	    break;
	case 0x5:		//(0x000101)BNE
	    BRANCH_INST (CURRENT_STATE.REGS[RS (inst)] != CURRENT_STATE.REGS[RT (inst)], CURRENT_STATE.PC + IDISP (inst), 0);
	    break;

	case 0x0:		//(0x000000)ADDU, AND, NOR, OR, SLTU, SLL, SRL, SUBU  if JR
	    {
		switch(FUNC (inst)){
		    case 0x21:	//ADDU
			CURRENT_STATE.REGS[RD (inst)] = CURRENT_STATE.REGS[RS (inst)] + CURRENT_STATE.REGS[RT (inst)];
			break;
		    case 0x24:	//AND
			CURRENT_STATE.REGS[RD (inst)] = CURRENT_STATE.REGS[RS (inst)] & CURRENT_STATE.REGS[RT (inst)];
			break;
		    case 0x27:	//NOR
			CURRENT_STATE.REGS[RD (inst)] = ~ (CURRENT_STATE.REGS[RS (inst)] | CURRENT_STATE.REGS[RT (inst)]);
			break;
		    case 0x25:	//OR
			CURRENT_STATE.REGS[RD (inst)] = CURRENT_STATE.REGS[RS (inst)] | CURRENT_STATE.REGS[RT (inst)];
			break;
		    case 0x2B:	//SLTU
			if ( CURRENT_STATE.REGS[RS (inst)] <  CURRENT_STATE.REGS[RT (inst)])
			    CURRENT_STATE.REGS[RD (inst)] = 1;
			else
			    CURRENT_STATE.REGS[RD (inst)] = 0;
			break;
		    case 0x0:	//SLL
			{
			    int shamt = SHAMT (inst);

			    if (shamt >= 0 && shamt < 32)
				CURRENT_STATE.REGS[RD (inst)] = CURRENT_STATE.REGS[RT (inst)] << shamt;
			    else
				CURRENT_STATE.REGS[RD (inst)] = CURRENT_STATE.REGS[RT (inst)];
			    break;
			}
		    case 0x2:	//SRL
			{
			    int shamt = SHAMT (inst);
			    uint32_t val = CURRENT_STATE.REGS[RT (inst)];

			    if (shamt >= 0 && shamt < 32)
				CURRENT_STATE.REGS[RD (inst)] = val >> shamt;
			    else
				CURRENT_STATE.REGS[RD (inst)] = val;
			    break;
			}
		    case 0x23:	//SUBU
			CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RS(inst)]-CURRENT_STATE.REGS[RT(inst)];
			break;

		    case 0x8:	//JR
			{
			    uint32_t tmp = CURRENT_STATE.REGS[RS (inst)];
			    JUMP_INST (tmp);

			    break;
			}
		    default:
			printf("Unknown function code type: %d\n", FUNC(inst));
			break;
		}
	    }
	    break;

	case 0x2:		//(0x000010)J
	    JUMP_INST (((CURRENT_STATE.PC & 0xf0000000) | TARGET (inst) << 2));
	    break;
	case 0x3:		//(0x000011)JAL
	    CURRENT_STATE.REGS[31] = CURRENT_STATE.PC;
	    JUMP_INST (((CURRENT_STATE.PC & 0xf0000000) | (TARGET (inst) << 2)));
	    break;

	default:
	    printf("Unknown instruction type: %d\n", OPCODE(inst));
	    break;
    }

    if (CURRENT_STATE.PC < MEM_REGIONS[0].start || CURRENT_STATE.PC >= (MEM_REGIONS[0].start + (NUM_INST * 4)))
	RUN_BIT = FALSE;
}
