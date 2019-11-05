#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <sys/attribs.h>
#include "config.h"
#include "lcd.h"
#include "led.h"
#include "btn.h"
#include "swt.h"
#include "ssd.h"
#include "Simulator.h"

#include "string.h"
#include <stdbool.h>
#include <errno.h>

#pragma config JTAGEN = OFF     
#pragma config FWDTEN = OFF      

/* ------------------------------------------------------------ */
/*						Configuration Bits		                */
/* ------------------------------------------------------------ */

// Device Config Bits in  DEVCFG1:	
#pragma config FNOSC =	FRCPLL
#pragma config FSOSCEN =	OFF
#pragma config POSCMOD =	XT
#pragma config OSCIOFNC =	ON
#pragma config FPBDIV =     DIV_2

// Device Config Bits in  DEVCFG2:	
#pragma config FPLLIDIV =	DIV_2
#pragma config FPLLMUL =	MUL_20
#pragma config FPLLODIV =	DIV_1

void __ISR(_TIMER_4_VECTOR, ipl2auto) Timer4SR(void)
{
    IFS0bits.T4IF = 0;                  // clear interrupt flag
    next++;                             // increment the interrupt counter
    ioregisters[0]++;                   // increment ioregisters[0] as required
}

int main(int argc, char** argv) {
    
    ////////////////////////////////////////////////////////////////
	////////////////          My Simulator          ////////////////
	////////////////////////////////////////////////////////////////
    
    // setup stage - initializing all necessary variables
    int imm = 0, opcode = 0, rd = 0, rs = 0, rt = 0, opcount = 0, effective_pc = 0;
	registers register_arr = {0};
	char inst[5];
	operation op;
	bool is_imm, is_jump;
    ioregisters[2] = 1;
    unsigned char switch_positions;
    
    // libraries initialization for the used IO modules
    IO_init();
    timer4_init();
    
    // memory init - program is set to fibonnacci or leds according to switch 7's position
    memory_init(SWT_GetValue(7));
    
    while (opcode != 15) { // while the instruction is not HALT
        
            // Getting ready for new iteration
            is_imm = false;
            is_jump = false;
            effective_pc = pc;

            ////////////////////////////////////////////////////////////////
            ////////////////           Fetch Stage          ////////////////
            ////////////////////////////////////////////////////////////////

            // fetch next instruction from memory[pc]
            if ((sprintf(inst, "%04X", memory[pc]) < 0))
                handle_error();

            ////////////////////////////////////////////////////////////////
            ////////////////          Decode Stage          ////////////////
            ////////////////////////////////////////////////////////////////

            // read the relevant opceode, rd, rs and rt values from the current istruction
            opcode = get_hex_int(inst, 0);
            rd = get_hex_int(inst, 1);
            rs = get_hex_int(inst, 2);
            rt = get_hex_int(inst, 3);

            // If the instruction uses an immediate
            if (rd == 1 || rs == 1 || rt == 1) {
                // Get the immediate word from memory and assign it to imm
                imm = (short)memory[pc + 1];
                register_arr[1] = imm;
                is_imm = true;
            }
            else { // if no immediate is used - set imm and $imm to 0
                imm = 0;
                register_arr[1] = 0;
            }

            // get the proper op enum for the opcode
            op = assign_op(opcode, is_imm);

            ////////////////////////////////////////////////////////////////
            ////////////////          Execute Stage         ////////////////
            ////////////////////////////////////////////////////////////////

            // execute the instruction, using all the gathered values
            // NOTE: this function may change values of variables
            execute_inst(op, &register_arr, rd, rs, rt, imm, &is_jump, &is_imm);

            // End of iteration - make the necessary updates
            if (!is_jump) { // if no jump or branch was taken
                if (is_imm) // if the instruction used the immediate - incr. pc by one
                    pc++;
                pc++; // incr. pc by one
            }
            opcount++; // incr. opcount by one
        
            next=0;
            while (next==0) { // next is incremented by the timer interrupt, so while it's still not
                              // updated we want to output relevant information using the IO modules
                paused_display(register_arr, is_imm, imm, effective_pc, opcount); // output info
            }
    }  

    // display the outcome of the program
    display_result(memory[1025]);
    return (1);
}