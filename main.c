#include <msp430fr2475.h>

#define BALL_START_X 4
#define BALL_START_Y 2

/*Screen functions */
void refresh();

/*Ball functions*/
int move_ball();
int about_to_collide_with_edge();
int about_to_collide_with_paddle();

/*Game Flow functions*/
void menu();
int play_game(int winning_score);
void endgame(int winner);

/*Game logic functions*/
void start_round();

/*Button functions*/
void init_buttons();
void enable_buttons();
void disable_buttons();
int parse_button_trig();


//Port 1.0-1.7 and 2.0-2.5 are rows
//Port 3.0-3.7 and 4.0-4.1 are columns
//Port 3.0-3.7 and 4.0-4.1 are columns
//Port 4.2 is button 1
//Port 4.3 is button 2
//Port 4.4 is button 3
//Port 4.5 is button 4
//Port 4.6 is button 5

/* Points to current display. Either gameplay display or winning animation */
int * display;

/*Vector storing column values when the corresponding row is on
 * Note: the matrix is inverted horizontally from the real screen
 */
int display_matrix [14] =  {0b0011100000, //Row 0
                            0b0000000000, //Row 1
                            0b0000000000, //Row 2
                            0b0000000000, //Row 3
                            0b0000000000, //Row 4
                            0b0000000000, //Row 5
                            0b0000000000, //Row 6
                            0b0000000000, //Row 7
                            0b0000000000, //Row 8
                            0b0000000000, //Row 9
                            0b0000000000, //Row 10
                            0b0000000000, //Row 11
                            0b0000000000, //Row 12
                            0b0111000000};//Row 13

int PONG [14] ={0b0111001111,
                0b1000110001,
                0b1000110001,
                0b1000101111,
                0b1000100001,
                0b1000100001,
                0b0111000001,

                0b0111010001, //Row 7
                0b1000110011 , //Row 8
                0b0000110101, //Row 9
                0b1110111001, //Row 10
                0b1000110001, //Row 11
                0b1000110001, //Row 12
                0b0111010001};//Row 13

int P1_Wins [14] = {0b0000000000, //Row 0
                    0b0010001111, //Row 1
                    0b0010001101, //Row 2
                    0b0010001111, //Row 3
                    0b0010000001, //Row 4
                    0b0010000001, //Row 5
                    0b0000000000, //Row 6
                    0b0000000000, //Row 7
                    0b1000110001, //Row 8
                    0b0101001010, //Row 9
                    0b0111001110, //Row 10
                    0b0010000100, //Row 11
                    0b0000000000, //Row 12
                    0b0000000000};//Row 13

int P2_Wins [14] = {0b0000000000, //Row 0
                    0b1111101111, //Row 1
                    0b1000001101, //Row 2
                    0b1111101111, //Row 3
                    0b0000100001, //Row 4
                    0b1111100001, //Row 5
                    0b0000000000, //Row 6
                    0b0000000000, //Row 7
                    0b1000110001, //Row 8
                    0b0101001010, //Row 9
                    0b0111001110, //Row 10
                    0b0010000100, //Row 11
                    0b0000000000, //Row 12
                    0b0000000000};//Row 13


int current_row = 0;
/*Ball location. Upper left corner is the origin (0,0)*/
int ball_x = BALL_START_X, ball_y = BALL_START_Y;     //ball location
/*Ball velocities. Down & right are positive, up and left are negative*/
int ball_x_v = 1, ball_y_v = 1;
int top_paddle_x = 4;
int bottom_paddle_x = 3;
int p1_score = 0, p2_score = 0;
int mode = 0; //0 - Menu; 1 - Gameplay; 2 - Endgame

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	PM5CTL0 &= ~LOCKLPM5;       //Turn off high-impedance mode

	display = PONG;

	//To turn an LED on, set column low, and row to high
	//Otherwise, set column high and row low

	P1DIR |= 0xFF; // configure P1 as output
	P2DIR |= 0xFF; // configure P2 as output; Unused pins are still output to minimize power consumption
	P1OUT = 0x00; // default rows to low
	P2OUT = 0x00; // default rows to low

    P3DIR |= 0xFF; // configure P3 as output (1=output)
    P4DIR |= 0b11; // configure 4.0-4.1 as output (1=output)
    P4DIR &= ~0b1111100; // configure 4.2-4.6 as input (0=input)
	P3OUT = 0xFF; // default columns to high
	P4OUT = 0x02; // default columns to high

    //Use Timer A0 for the screen refresh at 60 Hz rate
    TA0CTL = TASSEL_1 + MC_1;  // ACLK, upmode
    TA0CCR0 = 87;     // Refresh at 120 Hz to be seen by camera (32kHz clock, every 273 cycles)
    TA0CCTL0 = CCIE;    // CCR0 interrupt enabled

    //Timer A1 to update ball movement
    TA1CTL = TASSEL_1 + MC_1;  // ACLK, upmode
    TA1CCR0 = 16384;     // Move ball twice per second (32kHz clock, every 16384 cycles)
    TA1CCTL0 &= ~CCIE;    // CCR0 interrupt disabled, initially

    //TimerB0 for game music
    P5DIR |= BIT0; //Game audio; Output of 5.0
    P5SEL0 |= BIT0; //Select 5.0 as PWM output of TB0.2
    TB0CCR2 = 0;               //Init CCR as 0
    TB0CCTL2 = OUTMOD_7;      //Chose method of PWM
    TB0CTL = TASSEL_2 + MC_1; // SMCLK, upmode


    init_buttons();
    __bis_SR_register(GIE);
	while(1){
	    TA1CCTL0 &= ~CCIE;    // CCR0 interrupt disabled, initially
	    menu();
	    mode++;
	    int winner = play_game(2); //Best 2 of 3
	    mode++;
	    endgame(winner);
	    mode = 0;
	}
}


/*
 * Effects: Actual gameplay of Pong.
 * Return 1 on player 1 win, 2 on player 2 win.
 *
 * Requires:
 * winning_score specifies how many rounds are in the game
 */
int play_game(int winning_score){
    int result = 0;
    p1_score = p2_score = 0;
    TA1CCTL0 = CCIE;    // CCR0 interrupt enabled
    while(1){
            start_round();
            //Keep playing until a point is scored
            while((result = move_ball()) == 0){
                __low_power_mode_0();
            }

            /*Game continues in loop until a player exceeds winning threshold AND there is no tie*/
            //If player 1 is the winner (and not tied)
            if(p1_score >= winning_score && p1_score != p2_score){
                return 1;
            }
            //If player 2 is the winner (and not tied)
            else if(p2_score >= winning_score && p1_score != p2_score){
                return 2;
            }

    }

}

/*To the human eye, only 3 rows are on at a time
 * Iterate through the 3 needed rows instead of all
 * the rows to reduce power consumption, and increase
 * refresh speed, and decrease computational complexity.
 */
void refresh(void) {
    /*Update display*/
    //Turn everything off first so it doesn't look weird
    P1OUT = 0x00; // default rows to low
    P2OUT = 0x00; // default rows to low
    P3OUT |= 0xFF; // default columns to high
    P4OUT |= 0x02; // default columns to high

    //Sets all columns to corresponding values on (low), the rest to off (high)
    //Update rows 3.0-3.7
    char port_3_mask = display[current_row]; //Mask is only last 8 bits that are relevant
    P3OUT = ~(port_3_mask);

    //Update rows 4.0-4.1
    char port_4_mask = (~(display[current_row]) >> 8) &~(0b11111100); //Mask is only 2 lsb
    P4OUT = (P4OUT & ~(0b11)) + port_4_mask;

    //Update rows 1.0-1.7
    P1OUT = 0x01 << current_row;  //Sets 1 row, if any, to on (high), the rest to off (low)

    //Update rows on port 2.0-2.
    if (current_row > 7){ //Row on port 2 is on
        P2OUT = 0x01 << (current_row-8); //Turn off (low) all relevant pins on port 2
    }
    else { //The current row is on port 1; Turn off all LEDs on port 2
        P2OUT = P2OUT & ~(0b0111111);//Turn off (low) all relevant pins on port 2
    }

    /*Gameplay only requires 3 rows at a time*/
    if (mode == 1){
        //Iterate through 3 rows: top row, bottom row, and ball row.
        if (current_row == 13)
            current_row = 0;
        else if(current_row == 0 && ball_y > 0 && ball_y < 13)
            current_row = ball_y;
        else
            current_row = 13;
    }

    /*Endgame and menu requires all rows at a time*/
    if (mode == 2 || mode == 0){
        if (++current_row == 14)
            current_row = 0;
    }


}



int move_ball(){
    int result = 0;

    //If ball is off screen, p1 must have scored a point
    if(ball_y < 0){
        p1_score++;
        return 1;
    }
    //Else if ball is off screen on bottom, p2 must have scored a point
    else if(ball_y > 13){
        p2_score++;
        return 2;

    }



    //If ball is about to collide, bounce
    if(about_to_collide_with_edge() == 1){
        TB0CCR0 = (int) (3822);
        TB0CCR2 = ((int) (3822))>>1;
        ball_x_v = -ball_x_v ; //Bounce!
    }
    //Else, if ball is about to hit paddle directly
    else if((result = about_to_collide_with_paddle()) == 1){
        //Block button interrupts here
        TB0CCR0 = (int) (3822);
        TB0CCR2 = ((int) (3822))>>1;
        ball_y_v = -ball_y_v ; //Bounce!
    }
    //Else, if ball is about to hit the corner of the paddle
    else if(result == 2){
        //Block button interrupts here
        TB0CCR0 = 3822;
        TB0CCR2 = ((int) (3822))>>1;
        ball_y_v = -ball_y_v ; //Bounce!
        ball_x_v = -ball_x_v ; //Bounce!
    }

    __delay_cycles(1000);
    TB0CCR0 = 0;
    TB0CCR2 = 0;
    //Clear previous ball location
    display[ball_y] = display[ball_y] &~(0b1000000000 >> ball_x); //Update display matrix

    //Update ball location
    ball_x += ball_x_v;
    ball_y += ball_y_v;

    //If ball is on screen, then update place in display matrix
    if(ball_y >= 0 && ball_y <= 13){
        display[ball_y] = display[ball_y] | (0b1000000000 >> ball_x);
    }
    //Re-enable button interrupts here

    return 0;

}


/*
 * Returns 1 if ball is touching edge of screen and further movement in
 * the current direction will cause ball to collide with edge of screen.
 */
int about_to_collide_with_edge(){
    //If ball is on the left side of screen & moving left
    if(ball_x == 0 && ball_x_v == -1)
        return 1;
    //If ball is on the right side of screen & moving right
    else if(ball_x == 9 && ball_x_v == 1)
        return 1;
    //Otherwise, return false
    return 0;
}

/*
 * Returns 0 if ball is not touching edge of paddle.
 * Returns 1 if ball is touching edge of paddle and further movement in
 * the current direction will cause ball to collide with paddle.
 * Returns 2 if ball is touching corner of paddle and further movement in
 * the current direction will cause ball to collide with paddle.
 *
 */
int about_to_collide_with_paddle(){
    /*Top paddle collision*/
    //If the ball is under the paddle, moving up
    if(ball_y == 1 && ball_y_v == -1){
        //If ball is on right the corner, moving inwards
        if(ball_x-top_paddle_x == 2 && ball_x_v == -1){
            return 2;
        }

        //If ball is on left the corner, moving inwards
        else if(top_paddle_x-ball_x == 2 && ball_x_v == 1){
            return 2;
        }

        //If ball is directly under paddle
        else if(top_paddle_x-1 <= ball_x && ball_x <= top_paddle_x+1){
            return 1;
        }
    }

    /*Bottom paddle collision*/
    //If the ball is on top of paddle, moving down
    if (ball_y == 12 && ball_y_v == 1){
        //If ball is on right the corner, moving inwards
        if(ball_x-bottom_paddle_x == 2 && ball_x_v == -1){
            return 2;
        }

        //If ball is on left the corner, moving inwards
        else if(bottom_paddle_x-ball_x == 2 && ball_x_v == 1){
            return 2;
        }
        //If ball is directly on top of paddle
        else if(bottom_paddle_x-1 <= ball_x && ball_x <= bottom_paddle_x+1){
            return 1;
        }
    }

    return 0;
}


/*
 * Effects: Intializes ball position & display at the beginning
 * of a new round
 *
 * Requires: None
 */
void start_round(){
    display = display_matrix;
    //Clear previous ball location
    display[ball_y] = display[ball_y] &~(0b1000000000 >> ball_x); //Update display matrix

    ball_x_v = 1, ball_y_v = 1;
    ball_x = BALL_START_X, ball_y = BALL_START_Y;

    //Update to starting ball location
    display[ball_y] = display[ball_y] | (0b1000000000 >> ball_x); //Update display matrix
}

/*
 * Effects: Displays the previous screen until button 5 is
 * pressed. Upon start-up, this displays the menu screen.
 * After a game, this continues displaying the winner
 * animation
 *
 * Requires: None
 */
void menu(){
    __low_power_mode_0();
}

/*
 * Effects: Displays the appropriate
 * winner animation
 *
 * Requires: winner is the player number who won
 */
void endgame(int winner){
    //If player 1 wins
    if(winner == 1){
        display = P1_Wins;
    }
    //Otherwise, player 2 won
    else{
        display = P2_Wins;
    }

}


/*
 * Effects: Initializes button pins & enables interrupts
 *
 * Requires: None
 */
void init_buttons() {

    //Port 4.2-6 as buttons 1-5
    P4DIR &= ~(BIT2 + BIT3 + BIT4 + BIT5+ BIT6); // set to input (0)
    /*Button capacitors may be charged/discharged based on the previous run.
     * The pullup/down reistors should be set accordingly*/
    P4OUT |= (BIT5+ BIT3); // set resistors to pull up (1)
    P4OUT &= ~(BIT2+ BIT4 + BIT6); // set resistors to pull down (0)
    P4REN |= (BIT2 + BIT3 + BIT4 + BIT5+ BIT6); // enable pullup/down resistors (1)


    enable_buttons();
    P4IFG = 0X0; // clear any pending interrupts
}

/*
 * Effects: Enables button interrupts
 *
 */
void enable_buttons(){
    P4IES |= BIT2 + BIT3 + BIT4 + BIT5+ BIT6; // listen for high to low transitions (press)
    P4IE |= BIT2 + BIT3 + BIT4 + BIT5+ BIT6; // enable interrupts for these pins
}

/*
 * Effects: Disables button interrupts
 *
 * Requires:
 * None
 */
void disable_buttons(){
    P4IE &= ~(BIT2 + BIT3 + BIT4 + BIT5+ BIT6); // disable interrupts for these pins
}

/*
 * Effects: Returns the corresponding number of the button that triggered
 * the button ISR
 *
 * Requires: None
 */
int parse_button_trig(){
    /*Note: Writing to PxOUT can result in setting an IFG. Since
     * 4.0 and 4.1 control LEDs, they get set often and will trigger
     * port 4 interrupts. Identify these first to exit ISR asap*/

   if((P4IFG & BIT2) == BIT2){ //Bit 2 - button 1
        return 1;
    }
    else if((P4IFG & BIT3) == BIT3){ //Bit 3 - button 2
        return 2;
    }
    else if((P4IFG & BIT4) == BIT4){ //Bit 4 - button 3
        return 3;
    }
    else if((P4IFG & BIT5) == BIT5){ //Bit 5 - button 4
        return 4;
    }
    else if((P4IFG & BIT6) == BIT6){ //Bit 6 - button 5
        return 5;
    }
    return 0; //ERROR
}


// Button interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR
__interrupt void button_isr(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) button_isr (void)
#else
#error Compiler not supported!
#endif
{
    int butt = parse_button_trig();

    /*Weird fix because the debouncing capacitors on the board were
     * placed in series. Each time a button is pressed,
     * toggle between a pull up & pull down resistor, to charge/discharge
     * capacitor and actually trigger an interrupt the next press :( */
    //Wait for button to stop being pressed
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    __delay_cycles(30000);
    //Disable pullup/pull down resistors
    P4REN ^= 0x01 << (1+butt);
    //__delay_cycles(30000);
    //Toggle pull up/pull down resistor for the button just now pressed
    P4OUT ^= 0x01 << (1+butt);
   //__delay_cycles(30000);
    //Re-enable pullup/pulldown resistor
    P4REN ^= 0x01 << (1+butt);

    //start game from menu mode!
    if(mode == 0){
        //Button 5 - Start button
        if (butt == 5){
            __low_power_mode_off_on_exit();
        }
    }

    //Handle paddle movement during game mode
    if (mode == 1){
        /*Move top paddle*/
        //Button 4 - Player 2 left movement (actually right)
        if (butt == 4){
            //Only shift paddle if there's room to move
            if(top_paddle_x > 1){
                display[0] = display[0] << 1; //Bitshift matrix display
                top_paddle_x--;
            }

        }
        //Button 3 - Player 2 right movement
        else if (butt == 3){
            //Only shift paddle if there's room to move
            if(top_paddle_x  < 8){
                display[0] = display[0] >> 1; //Bitshift matrix display
                top_paddle_x++;
            }
        }

        /*Move bottom paddle*/
        //Button 2 - Player 1 looks like right movement, actually left
        else if (butt == 2){
            //Only shift paddle if there's room to move
            if(bottom_paddle_x > 1){
                display[13] = display[13] << 1; //Bitshift matrix display
                bottom_paddle_x--;
            }
        }
        //Button 1 - Player 1 left movement
        else if (butt == 1){
            //Only shift paddle if there's room to move
            if(bottom_paddle_x < 8){
                display[13] = display[13] >> 1; //Bitshift matrix display
                bottom_paddle_x++;
            }
        }
    }

    P4IFG = 0x0; //Clear pending interrupts
}


// Timer 0 interrupt service routine (refresh)
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A_0 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A_0 (void)
#else
#error Compiler not supported!
#endif
{
    refresh();
}

// Timer 1 interrupt service routine (move ball)
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A_1 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) Timer_A_1 (void)
#else
#error Compiler not supported!
#endif
{
    __low_power_mode_off_on_exit(); // equivalent to __bic_SR_register_on_exit(LPM3_bits);
}
