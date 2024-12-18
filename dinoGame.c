#include <stdio.h>
#include <string.h>
#include <time.h>
#include "minirisc/minirisc.h"
#include "minirisc/harvey_platform.h"
#include "support/uart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "xprintf/xprintf.h"
#include "task.h"
#include "font.h"
#include "sprite.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 480

extern const sprite_t dinoSpriteStatic;
extern const sprite_t dinoSpriteCrouched1;
extern const sprite_t dinoSpriteCrouched2;
extern const sprite_t dinoSpriteMoving1;
extern const sprite_t dinoSpriteMoving2;
extern const sprite_t dinoSpriteDead;

static SemaphoreHandle_t videoSem = NULL;

static uint32_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

static int velocity = 0; // initial vertical velocity is 0
static int deltaVelocity = -60;
static int gravity = 5;          // gravitational force
static int is_on_ground = 1;     // check if the dino is on the ground or not
static int is_crouched = 0;      // check if the dino is crouched or not (double gravity and change sprite)
static int dinoSpriteNumber = 1; // select correct dino Sprite number depending on is_crouched

uint32_t color = 0xFFFFFFFF;
int dinoLength = 96;
int dinoHeight = 98;
volatile int dinoX, dinoY;

void init_video()
{

    memset(frame_buffer, 0, sizeof(frame_buffer)); // clear frame buffer to black
    VIDEO->WIDTH = SCREEN_WIDTH;
    VIDEO->HEIGHT = SCREEN_HEIGHT;
    VIDEO->DMA_ADDR = frame_buffer;
    VIDEO->CR = VIDEO_CR_IE | VIDEO_CR_EN;
}

void init_variables()
{
    dinoX = SCREEN_WIDTH / 6;
    dinoY = SCREEN_HEIGHT;
    videoSem = xSemaphoreCreateBinary();

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        frame_buffer[i] = 0;
    }
}

void keyboard_interrupt_handler()
{
    uint32_t kdata;
    while (KEYBOARD->SR & KEYBOARD_SR_FIFO_NOT_EMPTY)
    {
        kdata = KEYBOARD->DATA;
        if (kdata & KEYBOARD_DATA_PRESSED)
        {
            // xprintf("key code: %d\n", KEYBOARD_KEY_CODE(kdata));
            switch (KEYBOARD_KEY_CODE(kdata))
            {
            case 113: // Q
                minirisc_halt();
                break;
            case 122: // Z
            // case 32:                        // Spacebar
            case 82:                        // UP arrow
                if (dinoY == SCREEN_HEIGHT) // if the dino ins't already jumping
                {
                    velocity = deltaVelocity; //
                    is_on_ground = 0;         // reset the is_on_ground flag
                }
                break;
            case 115: // S
            case 81:  // DOWN arrow
                is_crouched = 1;
                break;
            }
        }
        else // Key released
        {
            switch (KEYBOARD_KEY_CODE(kdata))
            {
            case 115: // S key released (uncrouch)
            case 81:
                is_crouched = 0;
                break;
            }
        }
    }
}

void video_interrupt_handler()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(videoSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    VIDEO->SR = 0;
}

void printScreen(void *arg)
{
    int frameCounter = 0;
    char score[100];
    while (1)
    {
        xSemaphoreTake(videoSem, portMAX_DELAY);
        frameCounter++;

        update_game(); // mise à jour du jeu à chaque frame

        memset(frame_buffer, 0, sizeof(frame_buffer)); // clear

        if (!is_on_ground)
        {
            draw_sprite(dinoX, dinoY - dinoHeight, &dinoSpriteStatic);
        }
        else if (!is_crouched)
        {
            if (frameCounter % 10 < 5)
            {
                draw_sprite(dinoX, dinoY - dinoHeight, &dinoSpriteMoving1);
            }
            else
            {
                draw_sprite(dinoX, dinoY - dinoHeight, &dinoSpriteMoving2);
            }
        }
        else
        {
            if (frameCounter % 10 < 5)
            {
                draw_sprite(dinoX, dinoY - dinoHeight, &dinoSpriteCrouched1);
            }
            else
            {
                draw_sprite(dinoX, dinoY - dinoHeight, &dinoSpriteCrouched2);
            }
        }
        sprintf(score, "Score: %d", frameCounter / 5);
        font_16x32_draw_text(20, 20, score, 0xfffffffff, 0x00000000);
    }
}

void update_game()
{
    // Apply gravity to the dino
    if (!is_on_ground)
    {
        velocity += gravity;
        dinoY += velocity;

        if (dinoY >= SCREEN_HEIGHT)
        {
            dinoY = SCREEN_HEIGHT;
            velocity = 0;
            is_on_ground = 1;
        }
    }
    else
    {
        // If the dinosaur is on the ground and the velocity is negative, it's jumping
        if (velocity < 0)
        {
            is_on_ground = 0;
        }
    }

    if (!is_crouched)
    {
        dinoSpriteNumber = 1;
        gravity = 5;
        dinoLength = 96;
        dinoHeight = 98;
    }
    else
    {
        dinoSpriteNumber = 0;
        gravity = 15;
        dinoLength = 122;
        dinoHeight = 71;
    }
}

int main()
{
    init_variables();
    init_uart();
    init_video();

    KEYBOARD->CR |= KEYBOARD_CR_IE;

    minirisc_enable_interrupt(VIDEO_INTERRUPT | KEYBOARD_INTERRUPT);

    xTaskCreate(printScreen, "Screen", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    return 0;
}
