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
instruction* get_inst_info(uint32_t pc) 
{ 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

void fetch()
{
    if(PIPELN.if_id.flushed)
        PIPELN.if_id.flushed = 0;
    else if(STALLS > 0)
    {
        CURRENT_STATE.PIPE[0] = 0;
        STALLS -= 1;
    }
    else
    {
        PIPELN.if_id.inst = INST_INFO[(CURRENT_STATE.PC - MEM_TEXT_START) >> 2];
        PIPELN.if_id.pc = CURRENT_STATE.PC;
    }
}

uint32_t decode(int no_bp_set, int forwarding_set)
{
/*
    printf("if_id------------------ \n");
    printf("if_id.pc: 0x%08x \n", PIPELN.if_id.pc);
    printf("if_id.flushed: %i \n", PIPELN.if_id.flushed);
    printf("if_id.inst.opcode: %i \n", PIPELN.if_id.inst.opcode);
    printf("if_id.inst.func_code: %i \n", PIPELN.if_id.inst.func_code);
    */

    //hazard unit
    uint32_t new_pc = CURRENT_STATE.PC + 4;
    instruction instr = PIPELN.if_id.inst;
    if(forwarding_set)
    {
        if(PIPELN.id_ex.mem_read.signal == 1 && ((PIPELN.id_ex.reg_rt == instr.r_t.r_i.rs) || (PIPELN.id_ex.reg_rt == instr.r_t.r_i.rt)))
        {
            new_pc = CURRENT_STATE.PC;
            return new_pc;
        }
    }
    else
    {
        if(PIPELN.mem_wb.signal == 1 && PIPELN.mem_wb.reg_rd != 0 && (PIPELN.mem_wb.reg_rd == PIPELN.id_ex.reg_rs || PIPELN.mem_wb.reg_rd == PIPELN.id_ex.reg_rt))
        {
            new_pc = CURRENT_STATE.PC + 4;
            flush(2);
            STALLS = 1;
            return new_pc;
        }
        else if(PIPELN.ex_mem.wb.signal == 1 && PIPELN.ex_mem.wb.reg_rd != 0 && (PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rs || PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rt))
        {
            new_pc = CURRENT_STATE.PC + 4;
            flush(2);
            STALLS = 2;
            return new_pc;
        }
    }

    if(PIPELN.id_ex.flushed)
    {
        PIPELN.id_ex.flushed = 0;
        return new_pc;
    }
    else
    {
        PIPELN.id_ex.inst = PIPELN.if_id.inst;
        PIPELN.id_ex.pc = PIPELN.if_id.pc;

        if(PIPELN.id_ex.inst.opcode == 35) // "lw"
            PIPELN.id_ex.mem_read.signal = 1;
        else
            PIPELN.id_ex.mem_read.signal = 0;

        if(forwarding_set)
        {
            //forwarding unit
            //rs
            PIPELN.id_ex.reg_rs = PIPELN.id_ex.inst.r_t.r_i.rs;
            PIPELN.id_ex.val_rs = CURRENT_STATE.REGS[PIPELN.id_ex.reg_rs];
            PIPELN.id_ex.reg_rt = PIPELN.id_ex.inst.r_t.r_i.rt;
            PIPELN.id_ex.val_rt = CURRENT_STATE.REGS[PIPELN.id_ex.reg_rt];

            //ex forwarding
            if(PIPELN.mem_wb.signal == 1 && PIPELN.mem_wb.reg_rd != 0 && !(PIPELN.ex_mem.wb.signal == 1 && PIPELN.ex_mem.wb.reg_rd != 0 && PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rs) && PIPELN.mem_wb.reg_rd == PIPELN.id_ex.reg_rs)
            {
                //mem_wb forwarding
                PIPELN.id_ex.forwarded.signal_rs = 1;
                PIPELN.id_ex.forwarded.val_rs = PIPELN.mem_wb.val_rd;
            }
            else if(PIPELN.ex_mem.wb.signal == 1 && PIPELN.ex_mem.wb.reg_rd != 0 && PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rs)
            {
                //ex_mem forwarding
                //printf("rs:ex_mem to ex triggered------------------------------------------------------------\n");
                PIPELN.id_ex.forwarded.signal_rs = 1;
                PIPELN.id_ex.forwarded.val_rs = PIPELN.ex_mem.wb.val_rd;
                //printf("val_rs: 0x%08x \n", PIPELN.id_ex.forwarded.val_rs);
            }
            else
                PIPELN.id_ex.forwarded.signal_rs = 0;

            //rt
            if(PIPELN.mem_wb.signal == 1 && PIPELN.mem_wb.reg_rd != 0 && !(PIPELN.ex_mem.wb.signal == 1 && PIPELN.ex_mem.wb.reg_rd != 0 && PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rt) && PIPELN.mem_wb.reg_rd == PIPELN.id_ex.reg_rt)
            {
                //mem_wb forwarding
                PIPELN.id_ex.forwarded.signal_rt = 1;
                PIPELN.id_ex.forwarded.val_rt = PIPELN.mem_wb.val_rd;
            }
            else if(PIPELN.ex_mem.wb.signal == 1 && PIPELN.ex_mem.wb.reg_rd != 0 && PIPELN.ex_mem.wb.reg_rd == PIPELN.id_ex.reg_rt)
            {
                //ex_mem forwarding
                PIPELN.id_ex.forwarded.signal_rt = 1;
                PIPELN.id_ex.forwarded.val_rt = PIPELN.ex_mem.wb.val_rd;
            }
            else
                PIPELN.id_ex.forwarded.signal_rt = 0;

            //mem forwarding
            if(PIPELN.mem_wb.signal == 1 && PIPELN.mem_wb.reg_rd != 0 && PIPELN.mem_wb.reg_rd == PIPELN.ex_mem.mem.reg_rt)
            {
                PIPELN.ex_mem.forwarded.signal = 1;
                PIPELN.ex_mem.forwarded.val_rt = PIPELN.mem_wb.val_rd;
            }
        }
    }

    if(instr.opcode == 2 || instr.opcode == 3) //j and jal
    {
        uint32_t target = instr.r_t.target << 2;
        new_pc = target;
        flush(1);
        return new_pc;
    }

    if(instr.opcode == 0 && instr.func_code == 0x08) //jr
    {
        uint32_t val_rs;
        if(PIPELN.id_ex.forwarded.signal_rs)
            val_rs = PIPELN.id_ex.forwarded.val_rs;
        else
            val_rs = PIPELN.id_ex.val_rs;

        new_pc = val_rs;
        flush(1);
        return new_pc;
    }

    if(instr.opcode == 4 || instr.opcode == 5) //beq or bne
    {
        if(no_bp_set)   //bp is not present
        {
            new_pc = CURRENT_STATE.PC + 4;
            flush(1);
            STALLS = 2;
            return new_pc;
        }
        else            //bp is present
        {
            short imm = instr.r_t.r_i.r_i.imm;
            uint32_t sign_ext_imm = sign_extend(imm);
            new_pc = PIPELN.if_id.pc + (sign_ext_imm << 2) + 4;
            flush(1);
            return new_pc;
        }
    }
    if(STALLS)
        return CURRENT_STATE.PC; // new_pc - 4
    return new_pc;
}

void execute(int no_bp_set)
{
    /*
    printf("id_ex------------------ \n");
    printf("pc: 0x%08x \n", PIPELN.id_ex.pc);
    //printf("pc: 0x%08x \n", CURRENT_STATE.PC);
    printf("flushed: %i \n", PIPELN.id_ex.flushed);
    printf("reg_rs: 0x%08x \n", PIPELN.id_ex.reg_rs);
    printf("val_rs: 0x%08x \n", PIPELN.id_ex.val_rs);
    printf("reg_rt: 0x%08x \n", PIPELN.id_ex.reg_rt);
    printf("val_rt: 0x%08x \n", PIPELN.id_ex.val_rt);
    printf("opcode: %i \n", PIPELN.id_ex.inst.opcode);
    printf("func_code: %i \n", PIPELN.id_ex.inst.func_code);
    */
    PIPELN.ex_mem.inst = PIPELN.id_ex.inst;
    PIPELN.ex_mem.pc = PIPELN.id_ex.pc;

    instruction instr = PIPELN.id_ex.inst;
    if(instr.opcode == 0 && instr.func_code == 0 && instr.r_t.r_i.rt == 0){
        PIPELN.ex_mem.wb.signal = 0;
        PIPELN.ex_mem.mem.signal = 0;
    }
    else if(instr.opcode == 0){
        uint32_t val_rs;
        uint32_t val_rt;
        if(PIPELN.id_ex.forwarded.signal_rs)
            val_rs = PIPELN.id_ex.forwarded.val_rs;
        else
            val_rs = PIPELN.id_ex.val_rs;

        if(PIPELN.id_ex.forwarded.signal_rt)
            val_rt = PIPELN.id_ex.forwarded.val_rt;
        else
            val_rt = PIPELN.id_ex.val_rt;
        unsigned char sa = PIPELN.id_ex.inst.r_t.r_i.r_i.r.shamt;

        PIPELN.ex_mem.wb.signal = 1;
        PIPELN.ex_mem.mem.signal = 0;

        PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.inst.r_t.r_i.r_i.r.rd;
        /*
        unsigned char rs = instr.r_t.r_i.rs;
        unsigned char rt = instr.r_t.r_i.rt;
        unsigned char rd = instr.r_t.r_i.r_i.r.rd;
        unsigned char sa = instr.r_t.r_i.r_i.r.shamt;
        */

        if(instr.func_code == 0x21){//addu
            PIPELN.ex_mem.wb.val_rd = val_rs + val_rt;
        } else if (instr.func_code == 0x23){//subu
            PIPELN.ex_mem.wb.val_rd = val_rs - val_rt;
        } else if (instr.func_code == 0x24){//and
            PIPELN.ex_mem.wb.val_rd = val_rs & val_rt;
        } else if (instr.func_code == 0x27){//nor
            PIPELN.ex_mem.wb.val_rd = ~(val_rs | val_rt);
        } else if (instr.func_code == 0x25){//or
            PIPELN.ex_mem.wb.val_rd = (val_rs | val_rt);
        } else if (instr.func_code == 0x00){//sll
            PIPELN.ex_mem.wb.val_rd = val_rt << sa;
        } else if (instr.func_code == 0x02){//srl
            PIPELN.ex_mem.wb.val_rd = val_rt >> sa;
        } else if (instr.func_code == 0x2b){//sltu
            if(val_rs < val_rt){
                PIPELN.ex_mem.wb.val_rd = 1;
            } else {
                PIPELN.ex_mem.wb.val_rd = 0;
            }
        } else if (instr.func_code == 0x08){//jr
            //return val_rs;
            //flush();
            //CURRENT_STATE.PC = val_rs - 4; //-4 cuz cycle() has +4 at the end
        }
    }
    else if(instr.opcode == 2 || instr.opcode == 3)
    {
        uint32_t target = PIPELN.id_ex.inst.r_t.target << 2;
        if(instr.opcode == 3) // jal
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.wb.reg_rd = 31;
            PIPELN.ex_mem.wb.val_rd = PIPELN.id_ex.pc + 4;
        }
        else
            PIPELN.ex_mem.wb.signal = 0;

        PIPELN.ex_mem.mem.signal = 0;

        //return target;
        //flush();
        //CURRENT_STATE.PC = target - 4; //-4 cuz cycle() has +4 at the end
    }  
    else
    {  
        uint32_t val_rs;
        uint32_t val_rt;
        if(PIPELN.id_ex.forwarded.signal_rs)
            val_rs = PIPELN.id_ex.forwarded.val_rs;
        else
            val_rs = PIPELN.id_ex.val_rs;

        if(PIPELN.id_ex.forwarded.signal_rt)
            val_rt = PIPELN.id_ex.forwarded.val_rt;
        else
            val_rt = PIPELN.id_ex.val_rt;

        //printf("val_rs: 0x%08x \n", val_rs);
        short imm = PIPELN.id_ex.inst.r_t.r_i.r_i.imm;

        if(instr.opcode == 9) // "addiu"
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            //fromBinary only gets unsigned.

            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.val_rd = val_rs + sign_ext_imm;
            //rt = rs + signextimm
        }
        else if(instr.opcode == 12) // "andi"
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.mem.signal = 0;

            int zero_ext_imm = imm;
            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.val_rd = val_rs & zero_ext_imm;
            //rt = rs & zeroextimm
        }
        else if(instr.opcode == 4) // "beq"
        {
            PIPELN.ex_mem.wb.signal = 0;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            if(no_bp_set)
            {
                if(val_rs == val_rt)//branch taken
                {
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + (sign_ext_imm << 2) + 4;
                    PIPELN.ex_mem.updated_flush = 0;
                }
                else                //branch not taken
                {
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + 4;
                    PIPELN.ex_mem.updated_flush = 0;
                }
            }
            else
            {
                if(val_rs != val_rt) 
                {
                    //bp is that it is always taken so flush when beq is not met
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + 4;
                    PIPELN.ex_mem.updated_flush = 2;
                }
            }
        }
        else if(instr.opcode == 5) // "bne"
        {
            PIPELN.ex_mem.wb.signal = 0;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t sign_ext_imm = sign_extend(imm);
            if(no_bp_set)
            {
                if(val_rs != val_rt)//branch taken
                {
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + (sign_ext_imm << 2) + 4;
                    PIPELN.ex_mem.updated_flush = 0;
                }
                else                //branch not taken
                {
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + 4;
                    PIPELN.ex_mem.updated_flush = 0;
                }
            }
            else
            {
                if(val_rs == val_rt)
                {
                    PIPELN.ex_mem.update_pc = 1;
                    PIPELN.ex_mem.updated_pc = PIPELN.id_ex.pc + 4;
                    PIPELN.ex_mem.updated_flush = 2;
                }
            }
        }
        else if(instr.opcode == 15) // "lui"
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.val_rd = zero_ext_imm << 16;
        }
        /*lwlwlwlwlwlwlwlw*/
        else if(instr.opcode == 35) // "lw"
        {
            PIPELN.ex_mem.wb.signal = 1; //lw needs wb
            PIPELN.ex_mem.mem.signal = 1;

            uint32_t sign_ext_imm = sign_extend(imm);
            PIPELN.ex_mem.mem.reg_rt = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt; //for forwarding sake (otherwise the conditionals for forwarding becomes too complicated)
            PIPELN.ex_mem.mem.address = val_rs + sign_ext_imm;

            //PIPELN.ex_mem.wb.val_rd = mem_read_32(CURRENT_STATE.REGS[rs] + );
            //rt = mem_read_32(rs + signextimm)
        }
        else if(instr.opcode == 13) // "ori"
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.val_rd = val_rs | zero_ext_imm;
        }
        else if(instr.opcode == 11) // "sltiu"
        {
            PIPELN.ex_mem.wb.signal = 1;
            PIPELN.ex_mem.mem.signal = 0;

            uint32_t zero_ext_imm = zero_extend(imm);
            PIPELN.ex_mem.wb.reg_rd = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.wb.val_rd = val_rs < zero_ext_imm ? 1 : 0;
        }
        else if(instr.opcode == 43) // "sw"
        {
            PIPELN.ex_mem.wb.signal = 0;
            PIPELN.ex_mem.mem.signal = 1;

            uint32_t sign_ext_imm = sign_extend(imm);
            PIPELN.ex_mem.mem.reg_rt = PIPELN.id_ex.reg_rt;
            PIPELN.ex_mem.mem.val_rt = val_rt;
            PIPELN.ex_mem.mem.address = val_rs + sign_ext_imm;

            //mem_write_32(CURRENT_STATE.REGS[rs] + sign_ext_imm, CURRENT_STATE.REGS[rt]);
        }
    }
}

uint32_t memory()
{
    /*
    printf("ex_mem--------------------- \n");
    printf("pc: 0x%08x \n", PIPELN.ex_mem.pc);
    //printf("pc: 0x%08x \n", CURRENT_STATE.PC);
    printf("reg_rs: 0x%08x \n", PIPELN.ex_mem.reg_rs);
    printf("val_rs: 0x%08x \n", PIPELN.ex_mem.val_rs);
    printf("reg_rt: 0x%08x \n", PIPELN.ex_mem.reg_rt);
    printf("val_rt: 0x%08x \n", PIPELN.ex_mem.val_rt);
    printf("opcode: %i \n", PIPELN.ex_mem.inst.opcode);
    printf("func_code: %i \n", PIPELN.ex_mem.inst.func_code);
    */

    uint32_t new_pc = CURRENT_STATE.PC + 4;

    uint32_t val_rt;
    if(PIPELN.ex_mem.forwarded.signal)
        val_rt = PIPELN.ex_mem.forwarded.val_rt;
    else
        val_rt = PIPELN.ex_mem.mem.val_rt;

    if(PIPELN.ex_mem.mem.signal)
    {
        if(PIPELN.ex_mem.wb.signal) //lw
        {
            PIPELN.mem_wb.signal = 1;
            PIPELN.mem_wb.reg_rd = PIPELN.ex_mem.mem.reg_rt;
            PIPELN.mem_wb.val_rd = mem_read_32(PIPELN.ex_mem.mem.address);
        }
        else                        //sw
            mem_write_32(PIPELN.ex_mem.mem.address, val_rt);
    }
    else
    {
        PIPELN.mem_wb.signal = PIPELN.ex_mem.wb.signal;
        PIPELN.mem_wb.reg_rd = PIPELN.ex_mem.wb.reg_rd;
        PIPELN.mem_wb.val_rd = PIPELN.ex_mem.wb.val_rd;
    }
    PIPELN.mem_wb.inst = PIPELN.ex_mem.inst;
    PIPELN.mem_wb.pc = PIPELN.ex_mem.pc;

    if(PIPELN.ex_mem.update_pc)
    {
        new_pc = PIPELN.ex_mem.updated_pc;
        flush(PIPELN.ex_mem.updated_flush);

        PIPELN.ex_mem.update_pc = 0;
        PIPELN.ex_mem.updated_pc = 0;
        PIPELN.ex_mem.updated_flush = 0;

        return new_pc;
    }

    return new_pc;
}

void write_back()
{
    /*
    printf("mem_wb------------------- \n");
    printf("pc: 0x%08x \n", PIPELN.mem_wb.pc);
    //printf("pc: 0x%08x \n", CURRENT_STATE.PC);
    printf("signal: %i \n", PIPELN.mem_wb.signal);
    printf("reg_rd: 0x%08x \n", PIPELN.mem_wb.reg_rd);
    printf("val_rd: 0x%08x \n", PIPELN.mem_wb.val_rd);
    printf("opcode: %i \n", PIPELN.mem_wb.inst.opcode);
    printf("func_code: %i \n", PIPELN.mem_wb.inst.func_code);
    */
    if(PIPELN.mem_wb.signal)
    {
        //printf("write_back() if entered!!!");
        CURRENT_STATE.REGS[PIPELN.mem_wb.reg_rd] = PIPELN.mem_wb.val_rd;
    }
}

void flush(int stage)
{
    if(stage == 1)
    {
        PIPELN.if_id.flushed = 1;

        //reset if_id
        PIPELN.if_id.pc = 0;
        //PIPELN.if_id.binary_inst = 0;

        //reset if_id.inst
        PIPELN.if_id.inst.value = 0;
        PIPELN.if_id.inst.opcode = 0;
        PIPELN.if_id.inst.func_code = 0;
        PIPELN.if_id.inst.r_t.r_i.rs = 0;
        PIPELN.if_id.inst.r_t.r_i.rt = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.r.rd = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.imm = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.r.shamt = 0;
        PIPELN.if_id.inst.r_t.target = 0;
    }
    else if(stage == 2)
    {
        PIPELN.if_id.flushed = 1;
        PIPELN.id_ex.flushed = 1;

        //reset if_id
        PIPELN.if_id.pc = 0;
        //PIPELN.if_id.binary_inst = 0;

        //reset if_id.inst
        PIPELN.if_id.inst.value = 0;
        PIPELN.if_id.inst.opcode = 0;
        PIPELN.if_id.inst.func_code = 0;
        PIPELN.if_id.inst.r_t.r_i.rs = 0;
        PIPELN.if_id.inst.r_t.r_i.rt = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.r.rd = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.imm = 0;
        PIPELN.if_id.inst.r_t.r_i.r_i.r.shamt = 0;
        PIPELN.if_id.inst.r_t.target = 0;

        //reset id_ex
        PIPELN.id_ex.pc = 0;
        PIPELN.id_ex.reg_rs = 0;
        PIPELN.id_ex.val_rs = 0;
        PIPELN.id_ex.reg_rt = 0;
        PIPELN.id_ex.val_rt = 0;
        PIPELN.id_ex.forwarded.signal_rs = 0;
        PIPELN.id_ex.forwarded.signal_rt = 0;
        PIPELN.id_ex.forwarded.val_rs = 0;
        PIPELN.id_ex.forwarded.val_rt = 0;

        //reset id_ex.inst
        PIPELN.id_ex.inst.value = 0;
        PIPELN.id_ex.inst.opcode = 0;
        PIPELN.id_ex.inst.func_code = 0;
        PIPELN.id_ex.inst.r_t.r_i.rs = 0;
        PIPELN.id_ex.inst.r_t.r_i.rt = 0;
        PIPELN.id_ex.inst.r_t.r_i.r_i.r.rd = 0;
        PIPELN.id_ex.inst.r_t.r_i.r_i.imm = 0;
        PIPELN.id_ex.inst.r_t.r_i.r_i.r.shamt = 0;
        PIPELN.id_ex.inst.r_t.target = 0;
    }
}

void stall()
{
    //reset id_ex
    PIPELN.id_ex.pc = 0;
    PIPELN.id_ex.reg_rs = 0;
    PIPELN.id_ex.val_rs = 0;
    PIPELN.id_ex.reg_rt = 0;
    PIPELN.id_ex.val_rt = 0;
    PIPELN.id_ex.forwarded.signal_rs = 0;
    PIPELN.id_ex.forwarded.signal_rt = 0;
    PIPELN.id_ex.forwarded.val_rs = 0;
    PIPELN.id_ex.forwarded.val_rt = 0;

    //reset id_ex.inst
    PIPELN.id_ex.inst.value = 0;
    PIPELN.id_ex.inst.opcode = 0;
    PIPELN.id_ex.inst.func_code = 0;
    PIPELN.id_ex.inst.r_t.r_i.rs = 0;
    PIPELN.id_ex.inst.r_t.r_i.rt = 0;
    PIPELN.id_ex.inst.r_t.r_i.r_i.r.rd = 0;
    PIPELN.id_ex.inst.r_t.r_i.r_i.imm = 0;
    PIPELN.id_ex.inst.r_t.r_i.r_i.r.shamt = 0;
    PIPELN.id_ex.inst.r_t.target = 0;
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
