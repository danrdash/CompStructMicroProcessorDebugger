#include "Simulator.h"
#include "led.h"
#include <time.h>
#include <stdlib.h>

/* ------------------------------------------------------------ */
/***    display_result
**
**  Parameters:
**      int res - the numerical number that is the outcome of the calculation
**      
**
**  Return Value:
**      
**
**  Description:
**      This function displays the result of the calculation.      
**          
*/
void display_result(int res){
    char str[10] = "       ";    
    sprintf(str, "number is: %d", res);
    
    LCD_DisplayClear();
    LCD_WriteStringAtPos("7th Fibonnacci", 0, 0);
    LCD_WriteStringAtPos(str, 1, 0);
    while(1)
    {    
        //set the LEDs value according to the SWT positions: LED is lit when the corresponding SWT is on
        //usage of the Set and Get group Value functions for both SWT and LED libraries
        LED_SetGroupValue(SWT_GetGroupValue());
        //usage of the GetValue function of the SWT library and the SetValue function of the RGBLed library
        if (BTN_GetValue(0))
        {
            //set Red color if SW0 is on - not the maximum value, 255, but 200, close
            RGBLED_SetValue(150,150,0);
        }
        if (BTN_GetValue(1))
        {
            //set Green color if SW0 is on
            RGBLED_SetValue(0, 150, 150);
        }
        if (BTN_GetValue(2))
        {
            //set Blue color if SW0 is on
            RGBLED_SetValue(150,0,150);
        }      
    }    
}

/* ------------------------------------------------------------ */
/***    memory_init
**
**  Parameters:
**      bool sw_7 - a boolean value that represents the position of switch number 7 at the
**      start of the program
**
**  Return Value:
**      
**
**  Description:
**      This function initializes the program's memory array to the right one -    
**      fibonnacci if sw_7 is false and leds program otherwise.
**
*/
void memory_init(bool sw_7) {
    int i;
    is_fib = !sw_7;
	for (i=0; i < SIZE_OF_MEMORY; i++)
        memory[i] = (sw_7 ? led_memory[i] : fib_memory[i]);
    return;
}

/* ------------------------------------------------------------ */
/***    IO_init
**
**  Parameters:
**
**
**  Return Value:
**      
**
**  Description:
**      This function initializes the program's IO modules, using the BASYS library init functions.
**
*/
void IO_init() {
    LCD_Init(); 
    LED_Init(); 
    SWT_Init();
    SSD_Init();
    BTN_Init();
    RGBLED_Init();
}

/* ------------------------------------------------------------ */
/***    timer4_init
**
**  Parameters:
**
**
**  Return Value:
**      
**
**  Description:
**      This function initializes the timer4, to be used for interrupts.
**
*/
void timer4_init() {
    static int fTimerInitialised = 0;
    if(!fTimerInitialised)
    {
        PR4 = 625000; // set period register, generates one interrupt every 62.5 ms
        TMR4 = 0;                           //    initialize count to 0
        T4CONbits.TCKPS = 3;                //    1:256 prescaler value
        T4CONbits.TGATE = 0;                //    not gated input (the default)
        T4CONbits.TCS = 0;                  //    PCBLK input (the default)
        IPC4bits.T4IP = 2;                  //    INT step 4: priority
        IPC4bits.T4IS = 0;                  //    subpriority
        IFS0bits.T4IF = 0;                  //    clear interrupt flag
        IEC0bits.T4IE = 1;                  //    enable interrupt
        T4CONbits.ON = 1;                   //    turn on Timer5
        macro_enable_interrupts();          //    enable interrupts at CPU
        fTimerInitialised = 1;
    }
}

/* ------------------------------------------------------------ */
/***    evaluate_hex_string
**
**  Parameters:
**      char str[5] - a hexadecimal string to be evaluated as an integer.
**
**
**  Return Value:
**      the integer value of the number represented by str in base 16.
**      
**
**  Description:
**      This function evaluates and returns the number represented by str in base 16.
**
*/
int evaluate_hex_string(char str[5]) {
	str[4] = '\0';
	return strtol(str, NULL, 16);
}

/* ------------------------------------------------------------ */
/***    handle_error
**
**  Parameters:
**
**
**  Return Value:
**      
**
**  Description:
**      This is a generic error handler, used mainly for debugging purposes.
**
*/
int handle_error() {
	perror("ERROR");
	exit(errno);
}

/* ------------------------------------------------------------ */
/***    assign_op
**
**  Parameters:
**      int code - the int representing the current op.
**      bool is_imm - a boolean value, specifying whether or not the current op uses an immediate value.
**
**
**  Return Value:
**      This function returns the proper op-code (an enum of type operation) given the op number.
**      
**
**  Description:
**      This function is used for decoding the opcode number into a more readable enum type.
**
*/
operation assign_op(int code, bool is_imm) {
	switch (code) {
	case 0:
		if (is_imm)
			return addi;
		return add;
	case 1:
		if (is_imm)
			return subi;
		return sub;
	case 2:
		return and;
	case 3:
		return or ;
	case 4:
		return sll;
	case 5:
		return sra;
	case 6:
		return ioreg;
	case 7:
		return beq;
	case 8:
		return bgt;
	case 9:
		return ble;
	case 10:
		return bne;
	case 11:
		return jal;
	case 12:
		return lw;
	case 13:
		return sw;
	case 14:
		return lhi;
	case 15:
		return halt;
	default: // if an invalid value was used as an opcode
		errno = EINVAL;
		printf("Invalid opcode!\n");
		handle_error();
		return add;
		break;
	}
}

/* ------------------------------------------------------------ */
/***    get_hex_int
**
**  Parameters:
**      const char* str - a string holding the chars we want to decode.
**      int num - the index of the char within str we want decoded.
**
**
**  Return Value:
**      An integer value (base 16) of the character in str[num].
**      
**
**  Description:
**      This function is used for decoding a hex character from within a hex string.
**
*/
int get_hex_int(const char* str, int num) {
	char s[2];
	s[1] = '\0';
	s[0] = str[num];
	return strtol(s, NULL, 16);
}

/* ------------------------------------------------------------ */
/***    execute_inst
**
**  Parameters:
**      operation op - an enum representing the current op to be exeecuted.
**      registers* reg_arr - a pointer to the register array.
**      int rd - the value rd from the current instruction.
**      int rs - the value rs from the current instruction.
**      int rt - the value rt from the current instruction.
**      int imm - the value of the immediate from the current instruction - is 0 if no immediate is used.
**      bool is_jump - this will be set to true if a jump or branch should be taken, otherwise false.
**      bool is_imm - this has 2 uses: 
**          1. if the op is ioreg - there is no use of immediate but it will be
**          set true because of the decode stage, so set to false before executing.
**          2. if the op is jal - set the return address register to pc+1 if false and to pc+2 if true.
**
**
**  Return Value:
**      returns 0 on success.
**      
**
**  Description:
**      This function executes the current instruction.
**
*/
int execute_inst(operation op, registers* reg_arr, int rd, int rs,
	int rt, int imm, bool* is_jump, bool* is_imm) {
	// setting the correct address for word instructions
	int address = (rs == 1 ? imm : (*reg_arr)[rs]) + (rt == 1 ? imm : (*reg_arr)[rt]);

	switch (op) {
	case addi:
		(*reg_arr)[rd] = ((rt == 1) ? imm : (*reg_arr)[rt]) + ((rs == 1) ? imm : (*reg_arr)[rs]);
		break;
	case add:
		(*reg_arr)[rd] = (*reg_arr)[rs] + (*reg_arr)[rt];
		break;
	case subi:
		(*reg_arr)[rd] = ((rs == 1) ? imm : (*reg_arr)[rs]) - ((rt == 1) ? imm : (*reg_arr)[rt]);
		break;
	case sub:
		(*reg_arr)[rd] = (*reg_arr)[rs] - (*reg_arr)[rt];
		break;
	case and:
		(*reg_arr)[rd] = (*reg_arr)[rs] & (*reg_arr)[rt];
		break;
	case or :
		(*reg_arr)[rd] = (*reg_arr)[rs] | (*reg_arr)[rt];
		break;
	case sll:
		(*reg_arr)[rd] = (*reg_arr)[rs] << (*reg_arr)[rt];
		break;
	case sra:
		(*reg_arr)[rd] = (*reg_arr)[rs] >> (*reg_arr)[rt];
		break;
    case ioreg:
        if (rt == 0) {
            (*reg_arr)[rd] = ioregisters[(*reg_arr)[rs]];
        }
        else {
            if ((*reg_arr)[rs] == 2)
                break;
            ioregisters[(*reg_arr)[rs]] = ((*reg_arr)[rd] & 0xff);
            if ((*reg_arr)[rs] == 1)
                LED_SetGroupValue(ioregisters[(*reg_arr)[rs]]);
        }
        *is_imm = false;
        break;
	case beq:
		if ((*reg_arr)[rs] == (*reg_arr)[rt]) {
			pc = (*reg_arr)[rd] & 0xFFFF;
			*is_jump = true;
		}
		break;
	case bgt:
		if ((*reg_arr)[rs] > (*reg_arr)[rt]) {
			pc = (*reg_arr)[rd] & 0xFFFF;
			*is_jump = true;
		}
		break;
	case ble:
		if ((*reg_arr)[rs] <= (*reg_arr)[rt]) {
			pc = (*reg_arr)[rd] & 0xFFFF;
			*is_jump = true;
		}
		break;
	case bne:
		if ((*reg_arr)[rs] != (*reg_arr)[rt]) {
			pc = (*reg_arr)[rd] & 0xFFFF;
			*is_jump = true;
		}
		break;
	case jal:
		(*reg_arr)[15] = is_imm ? pc + 2 : pc + 1;
		pc = ((rd == 1) ? imm : ((*reg_arr)[rd] & 0xFFFF));
		*is_jump = true;
		break;
	case lw:
		(*reg_arr)[rd] = memory[address & 0x7FF];
		break;
	case sw:
		memory[address & 0x7FF] = (*reg_arr)[rd];
		break;
	case lhi:
		(*reg_arr)[rd] &= 0xFFFF;
		(*reg_arr)[rd] += ((*reg_arr)[rs] << 16);
		break;
	case halt:
		printf("halting!\n");
		break;
	}
	return 0;
}

/* ------------------------------------------------------------ */
/***    check_switch_pos
**
**  Parameters:
**      int sw_num - the number of switch to be checked.
**      bool* switch_p - a pointer to the bool repressnting the state of the switch.
**
**
**  Return Value:
**      
**
**  Description:
**      This function checks if switch number sw_num is on
**
*/
void check_switch_pos(int sw_num, bool* switch_p) {
    *switch_p = SWT_GetValue(sw_num);
}

/* ------------------------------------------------------------ */
/***    bools_init
**
**  Parameters:
**      bool* sw0, sw1, ... , sw7 - boolean pointers fot states of switches 0 through 7.
**
**
**  Return Value:
**      
**
**  Description:
**      This function checks positions for switches 0 through 7 and initializes global booleans to false.
**
*/
void bools_init(bool* sw0, bool* sw1, bool* sw2, bool* sw3, bool* sw4, bool* sw5, bool* sw6, bool* sw7) {
    check_switch_pos(0, sw0);
    check_switch_pos(1, sw1);
    check_switch_pos(2, sw2);
    check_switch_pos(3, sw3);
    check_switch_pos(4, sw4);
    check_switch_pos(5, sw5);
    check_switch_pos(6, sw6);
    check_switch_pos(7, sw7);
    is_paused = false;
    is_update_display = false;
    is_button_down = false;
    is_update_address = false;
}

/* ------------------------------------------------------------ */
/***    display_data
**
**  Parameters:
**      unsigned char switches - a char representing the state of the switches - 1 bit per switch. The data
**          to be displayed varies according to the switch positions.
**      registers reg_arr - the register array.
**      bool is_imm - true if instruction uses an immediate value.
**      int imm - the current immediate value.
**      int eff_pc - the relevant pc to be displayed - used instead of actual pc because actual pc may already be
**          the pc of the next instruction when we want to display the current one.
**      int opcount - the number of instructions executed.
**
**
**  Return Value:
**      
**
**  Description:
**      This function outputs the correct data through the IO modules.
**
*/
void display_data(unsigned char switches, registers reg_arr, bool is_imm, int imm, int eff_pc, int opcount) {
    int state = switches & 3; // samples 2 LSBs
    int address_bits = (switches & 96) >> 5; // samples bits 5 and 6
    char first_row[100];
    int d1 = pc & 0xff;
    int d2 = pc & 0xff00;
    int d3 = pc & 0xff0000;
    int d4 = pc & 0xff000000;
    switch (state) {
        case 0:
            // display instruction, formatted like trace int the 1st project
            sprintf(first_row, "%04X%04X", is_imm ? memory[eff_pc + 1] : 0, memory[eff_pc]);
            break;
        case 1:
            // display: Rxx = yyyyyyyy, xx is the number of the register and yyyyyyyy is its content
            // pressing BTND increments xx and BTNU decrements it.
            sprintf(first_row, "R%02d = %08X", display_register, reg_arr[display_register]);
            break;
        case 2:
            // display memory contents - first row: MAAAA = DDDD, As for address and Ds for data
            // second row: RSP = YYYYYYYY, which is the stack pointer's content
            // pressing BTND increments address, and BTNU decrements it - loop on overflow
            switch (address_bits) {
                case 0:
                    // starting address: 0000
                    if (is_update_address) {
                        display_address = 0x0000;
                        is_update_address = false;
                    }
                    sprintf(first_row, "M%04X = %04X", display_address, memory[display_address]);
                    break;
                case 1:
                    // starting address: 0400
                    if (is_update_address) {
                        display_address = 0x0400;
                        is_update_address = false;
                    }
                    sprintf(first_row, "M%04X = %04X", display_address, memory[display_address]);
                    break;
                case 2:
                case 3:
                    // starting address: 07FF
                    if (is_update_address) {
                        display_address = 0x07FF;
                        is_update_address = false;
                    }
                    sprintf(first_row, "M%04X = %04X", display_address, memory[display_address]);
                    break;
            }
            break;
        case 3:
            sprintf(first_row, "%d", opcount);
            
    }
    
    if (BTN_GetValue(2) && !is_button_down) { // listen to click on BTNC
        is_button_down = true;
        ioregisters[2]++; // increment the BTNC counter
        set_random_rgbled();
    }
    
    LCD_DisplayClear();
    LCD_WriteStringAtPos(first_row, 0, 0);
    SSD_WriteDigitsGrouped(eff_pc, 0);
    if (is_fib)
        LED_SetGroupValue(SWT_GetGroupValue()); // light up leds if we are in fibonnacci execution
}

/* ------------------------------------------------------------ */
/***    paused_display
**
**  Parameters:
**      registers reg_arr - the register array.
**      bool is_imm - true if instruction uses an immediate value.
**      int imm - the current immediate value.
**      int effective_pc - the relevant pc to be displayed - used instead of actual pc because actual pc may already be
**          the pc of the next instruction when we want to display the current one.
**      int opcount - the number of instructions executed.
**
**
**  Return Value:
**      
**
**  Description:
**      This function performs multiple tasks - checks for BTNL clicks to pause/run the program,
**      makes sure every button press is counted only once (using is_button_down), checks for changes in the
**      switch positions to be updated and calls display_data whenever a display update is needed.
**
*/
void paused_display(registers reg_arr, bool is_imm, int imm, int effective_pc, int opcount){
    display_data(SWT_GetGroupValue(), reg_arr, is_imm, imm, effective_pc, opcount);
    unsigned char switches_then, switches_now;
    switches_then = SWT_GetGroupValue();
    
    if (is_button_down && !BTN_GetGroupValue()) // set is_button_down to false if no buttons are pressed
            is_button_down = false;  
    
    if (BTN_GetValue(1)) { // catch a BTNL click
        is_paused = true;
        is_button_down = true;
        while (BTN_GetValue(1)){} // pause only upon release of BTNL
    }
    
    while (is_paused) {
        switches_now = SWT_GetGroupValue();
        if (switches_now != switches_then) { // check if there has been a change in the switch positions
            is_update_display = true;
            is_update_address = ((switches_now & 96) ^ (switches_then & 96)); // check for change in address switches
            switches_then = SWT_GetGroupValue();
        }
        int i;
        if (is_update_display) { // if a display update is needed
            display_data(SWT_GetGroupValue(), reg_arr, is_imm, imm, effective_pc, opcount);
            is_update_display = false;
        }
        if (BTN_GetValue(1) && !is_button_down) { // catch a BTNL click to un-pause the program
            is_paused = false;
            is_update_display = true;
            is_button_down = true;
            while (BTN_GetValue(1)){} // un-pause only upon release of BTNL
        }
        if (BTN_GetValue(3) && !is_button_down) { // catch a BTNR click to run a single instruction
            is_update_display = true;
            is_button_down = true;
            while (BTN_GetValue(3)){} // run only upon release of BTNR
            break;
        }
        
        if (switches_now & 1) { // if we're in register display mode
            if (BTN_GetValue(0) && !is_button_down) { // BTNU is clicked
                is_button_down = true;
                is_update_display = true;
                display_register--;
                if (display_register < 0) // loop back over 16
                    display_register = 15;
            }
            if (BTN_GetValue(4) && !is_button_down) { // BTND is clicked
                is_button_down = true;
                is_update_display = true;
                display_register++;
                if (display_register > 15) // loop back under 0
                    display_register = 0;
            }
        }
        
        if (switches_now & 2) { // if we're in memory display mode
            if (BTN_GetValue(0) && !is_button_down) { // BTNU is clicked
                is_button_down = true;
                is_update_display = true;
                display_address--;
                if (display_address < 0) // loop back over 2047
                    display_address = 0x7FF;
            }
            if (BTN_GetValue(4) && !is_button_down) { // BTND is clicked
                is_button_down = true;
                is_update_display = true;
                display_address++;
                if (display_address > 0x7FF) // loop back under 0
                    display_address = 0;
            }
        }
        
        if (is_button_down && !BTN_GetGroupValue()) // set is_button_down to false if no buttons are pressed
            is_button_down = false;       
    }
}

/* ------------------------------------------------------------ */
/***    set_random_rgbled
**
**  Parameters:
**
**
**  Return Value:
**      
**
**  Description:
**      This function is a bonus feature, which sets the RGBled to a pseudo-random color with each click on BTNC.
**      We find it slightly biased towards shades of blue.
**
*/
void set_random_rgbled(){
    int r = (ioregisters[2] * 200) % 256;
    int g = (ioregisters[2] * 400) % 256;
    int b = (ioregisters[2] * 100) % 256;
    RGBLED_SetValue(r,g,b); // set RGBled to random number;
}