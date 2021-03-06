/* main.c
 *
 * ECE348, Spring 2015
 * Group C4
 * Brandon Perez bmperez
 * Rohit Banerjee rohitban
 *
 * Target: MC9S12C128
 * CodeWarrior Version: 5.1
 *
 * Created: Sat 25 Apr 2015 06:24:57 PM EDT
 * Last Modified: Thu 07 May 2015 08:41:37 PM EDT
 *
 * Snake Game
 *
 * This project is a simple implementation of a snake game for the MC9S12C128
 * Freescale module. The game runs on an 8x8 LED matrix on the project board,
 * and is fully controlled by the module. The module outputs row and column
 * numbers to two decoders, which drive a single LED at a time. The column
 * decocder enable is driven by the module's PWM signal, to control the
 * brightness of the LED's. The row enable serves to activate a single row.
 * A photodiode measures the current ambient brightness of the room, and that
 * is used to determine the duty cycle of the PWM signal, controlling the
 * brightness.
 *
 * The snake game goes by the standard rules. The snake attempts to eat as many
 * food pieces on the board as it can. Each time a food is eaten, the score
 * increases by 1, as does the size of the snake. This game has no walls on the
 * board, so the snake can wrap around the board. The game ends whenever the
 * snake intersects with itself. The game is controlled via a serial interface
 * with the computer. The computer can change the direction of the snake with
 * the old fashion directional controls: 'wasd'.
 *
 * Externally Visible Items:
 *     void main()
 *
 * Pin Connections:
 *     Port AD[7] to Photodiode Output
 *     Port B[0] to PB1
 *     Port B[1] to PB2
 *     Port P[0] to the Enable Input of the Row Decoder
 *     Port T[4:2] to Select Input of the Row Decoder
 *     Port T[7:5] to Select Input of Column Decoder
 *     Port P[0] to Enable Input of the Row Decoder
 */

// Freescale libraries
#include <hidef.h>
#include <mc9s12c128.h>
#include <stdio.h>
#pragma LINK_INFO DERIVATIVE "mc9s12c128"

// 348 libraries
#include "modclock.h"
#include "lcd_lib.h"

// Project libraries
#include "stdbool.h"
#include "stdint.h"
#include "bit_help.h"
#include "setup.h"
#include "snake_game.h"

/*----------------------------------------------------------------------------
 * Internal Definitions
 *----------------------------------------------------------------------------*/

// The values written to ARMCOP to prevent a watchdog (COP) reset
#define WATCHDOG_VAL1   0x55
#define WATCHDOG_VAL2   0xAA

// Watchdog flags for each task, indicating that they are still alive
#define MAINLOOP_ALIVE   1
#define TIMER_ALIVE      2
#define ATD_ALIVE        4

// The value of the watchdog flag whenever all tasks are alive
#define ALL_TASKS_ALIVE 0x7

// The minimum brightness for the LED's
#define MIN_BRIGHTNESS  0x3

// The bit positions for the row and column, inclusive
#define ROW_START        2
#define ROW_END          4
#define COLUMN_START     5
#define COLUMN_END       7

// How often to move a snake, in the number of timer interrupts (ticks)
#define MOVE_QUANTUM    76

// The size of the buffer to store strings
#define BUFSIZE         10

// The flag read by the watchdog
static volatile uint8_t watch_flag;

// The state of the snake game
static volatile snake_game_t game;

// Indicates that the game needs to be restarted
static volatile bool restart_game;

// The number of timer interrupts that have occurred
static int num_ticks;

// The measured brightness from the photodiode
static volatile uint8_t brightness;

// The row and column of the LED currently being drawn
static uint8_t row;
static uint8_t col;

// Watchdog function prototype
static void kick_watchdog(void);

/*----------------------------------------------------------------------------
 * Main Routine
 *----------------------------------------------------------------------------*/

/* main
 *
 * The main routine.
 *
 * The main routine runs all of the setup an initialization code, then runs in
 * a loop forever. In the loop, it reads the paused and restart game buttons,
 * and update the state appropiately. Also, it displays the current score on the
 * LCD screen, along with the A/D input value.
 */
void main()
{
    bool pause_last_press, reset_last_press;
    char score_buf[BUFSIZE];
    char atd_buf[BUFSIZE];

    // Setup the clock and LCD
    clockSetup();
    lcdSetup();

    // Setup the watchdog timer
    watch_flag = 0;
    setup_watchdog();

    // Setup the ports, serial communication, PWM, A/D, and timer
    setup_ports();
    setup_serial();
    setup_pwm();
    setup_atd();
    setup_timer();

    // Initialize the game state
    pause_last_press = false;
    reset_last_press = false;
    num_ticks = 0;
    row = 0;
    col = 0;
    restart_game = false;
    brightness = PWM_INIT_DUTY;
    snake_init((snake_game_t *)&game);

    // Enable all interrupts
    EnableInterrupts;

    /* Loop forever, reading the user input, and updating the game
     * state appropiately. */
    for (;;)
    {
        // Check if the pause button was pressed, and toggle the pause
        if (!pause_last_press && !PORTB_BIT0) {
            game.paused = !game.paused;
        }
        pause_last_press = !PORTB_BIT0;

        // Check if the reset button was pressed
        if (!reset_last_press && !PORTB_BIT1) {
            restart_game = true;
        }
        reset_last_press = !PORTB_BIT1;

        // Format the strings for the score and brightness
        (void)sprintf(score_buf, "Score:%2d", game.score);
        (void)sprintf(atd_buf, "Bg: 0x%02x", brightness);

        // Display the score and brightness A/D conversion to the LED's
        lcdWriteLine(1, score_buf);
        lcdWriteLine(2, atd_buf);

        /* Indicate that the main loop task is alive, and check if all tasks
         * have indicated that they're alive. If so, kick the watchdog. */
        DisableInterrupts;
        watch_flag |= MAINLOOP_ALIVE;
        if (watch_flag == ALL_TASKS_ALIVE) {
            watch_flag = 0;
            EnableInterrupts;
            kick_watchdog();
        }
        EnableInterrupts;
    }
}

/* kick_watchdog
 *
 * "Kick" the on-board watchdog, preventing a watchdog reset from occuring.
 * This indicates that the program is making normal progress, and is not stuck
 * in an infinite loop.
 */
static void kick_watchdog(void)
{
    ARMCOP = WATCHDOG_VAL1;
    ARMCOP = WATCHDOG_VAL2;

    return;
}


/*----------------------------------------------------------------------------
 * Interrupts
 *----------------------------------------------------------------------------*/

/* sci_interrupt
 *
 * This function handles interrupts from the onboard SCI (serial) module,
 * specifically whenever a new data byte is received. This function reads the
 * byte as a character, then updates the snake direction appropiately. 'w'
 * is up, 'a' is left, 's' is down, and 'd' is right.
 */
void interrupt VectorNumber_Vsci sci_interrupt()
{
    volatile char received_char;

    // Acknowledge serial interrupt and read the character sent
    received_char = SCISR1_RDRF;
    received_char = SCIDRL;

    /* Update the snake direction based on the received input. 'w' is up,
     * 'a' is left, 's' is down, 'd' is right. Due to the orientation of the
     * LED matrix, left and right are mirrored. */
    switch (received_char)
    {
        case 'W':
        case 'w':
            game.drow = 0;
            game.dcol = 1;
            break;
        case 'A':
        case 'a':
            game.drow = -1;
            game.dcol = 0;
            break;
        case 'S':
        case 's':
            game.drow = 0;
            game.dcol = -1;
            break;
        case 'D':
        case 'd':
            game.drow = 1;
            game.dcol = 0;
            break;
    }

    return;
}

/* atd_interrupt
 *
 * This function handles interrupts from the A/D converter module, which
 * occurs whenever a new conversion is ready. This function simply reads
 * the conversion as the ambient brightness, then updates the duty cycle
 * of the PWM to counteract this brightness.
 */
void interrupt VectorNumber_Vatd0 atd_interrupt()
{
    // Ackowledge the ATD interrupt
    ATDSTAT0_SCF = 0x1;

    // Read out new brightness value, and update the duty cycle
    brightness = ATDDR0H;
    PWMDTY0 = (brightness < MIN_BRIGHTNESS) ? MIN_BRIGHTNESS : brightness;

    // Indicate that the A/D task is alive
    watch_flag |= ATD_ALIVE;

    return;
}

/* tc7_interrupt
 *
 * This function handles interrupts from the Timer module, which occurs
 * whenever the timer counter value is equal to the comparison value on
 * channel 7 (every 0.32 ms). This function draws the next frame on the board,
 * and adjusts the PWM appropiately. After MOVE_QUANTUM timer ticks occur
 * (250 ms), the snake is moved based on the game state. If the user
 * pressed the reset game button, then the game is reinitialized to its
 * starting state.
 */
void interrupt VectorNumber_Vtimch7 tc7_interrupt()
{
    // Acknowledge the timer interrupt
    TFLG1_C7F = 0x1;

    // Select the LED at (row, col)
    PTT = set_bits(PTT, row, ROW_START, ROW_END);
    PTT = set_bits(PTT, col, COLUMN_START, COLUMN_END);

    /* Drive the selected LED only if it is the snake or food. Reset the
     * PWM period counter so that we don't get an irregular period. */
    if (game.board[row][col] == SNAKE_EMPTY) {
        PWME_PWME0 = 0x0;
    } else {
        PWMCNT0 = 0;
        PWME_PWME0 = 0x1;
    }

    col = mod_8(col + 1, SNAKE_COLUMNS);
    if (col == 0) {
        row = mod_8(row + 1, SNAKE_ROWS);
    }

    /* If the game is not paused, then increment the number of timer ticks.
     * Otherwise, setup the ticks so that movement occurs upon unpausing. */
    if (row == 0 && game.paused) {
        num_ticks = MOVE_QUANTUM-1;
    } else if (row == 0) {
        num_ticks += 1;
    }

    /* Check if the user requested a reset. If so, reset the game.
     * Otherwise, move the snake. */
    if (row == 0 && restart_game) {
        snake_init((snake_game_t *)&game);
        restart_game = false;
    } else if (row == 0 && num_ticks == MOVE_QUANTUM) {
        num_ticks = 0;
        move_snake((snake_game_t *)&game);
    }

    // Indicate that the timer/game update task is alive
    watch_flag |= TIMER_ALIVE;

    return;
}

/* watchdog_interrupt
 *
 * This function handles interrupts whenver a COP (or watchdog) reset occurs.
 * This happens whenever the watchdog is not "kicked" in time. This simply
 * notifies the user that a watchdog error occured on the LCD display. This
 * function never returns.
 */
void interrupt VectorNumber_Vcop watchdog_interrupt()
{
    // Defined in hidef.h, initializes the stack pointer properly
    INIT_SP_FROM_STARTUP_DESC();

    // Setup the LCD screen, and indicate a watchdog error.
    lcdSetup();
    lcdWriteLine(1, "Watchdog");
    lcdWriteLine(2, "Error");

    // Loop forever, wait until the user presses the reset button
    for (;;);
}
