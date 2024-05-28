#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdatomic.h>
#include <immintrin.h>
#include <threads/threads_api.h>

#define MIND_DECK_SIZE (100)
#define MIND_N_PLAYERS (3)
#define MIND_MAX_LEVEL (12)
#define MIND_MIN_SKILL 0.66f
#define MIND_MAX_SKILL 0.90f
#define MIND_AVERAGE_BEAT (100)
#define MIND_MAX_BEAT (MIND_AVERAGE_BEAT * 3)
#define MIND_MIN_BEAT (MIND_AVERAGE_BEAT / 3)
#ifdef DEBUG_BUILD
    #define MIND_DEBUG(x) do {x} while (0)
#else
    #define MIND_DEBUG(x)
#endif

// Declaration of all types //

typedef struct game_t game_t;
typedef struct player_t player_t;
typedef struct stack_t stack_t;
typedef enum mind_stack_type_t {
    DECK, PILE, HAND    
} mind_stack_type_t;

//---------------------------------//
//--------STACK DECLARATION--------//
//---------------------------------//

struct stack_t {
    uint8_t *cards;
    uint8_t *top;
    size_t allocd;
};

void stackCreate(stack_t *stack, size_t sz);
void stackDestroy(stack_t *stack);
uint32_t stackGetSize(stack_t *stack);
void stackPush(stack_t *stack, uint8_t value);
uint8_t stackPop(stack_t *stack);
void stackMove(stack_t *dst, stack_t *src);
void stackPushN(stack_t *stack, uint8_t *values, size_t n);
void stackPopN(stack_t *stack, uint8_t *res, size_t n);
void stackMoveN(stack_t *dst, stack_t *src, size_t n);
uint8_t stackPeek(stack_t *stack);
void stackPrint(stack_t *stack, const char *stack_name);

//--------------------------------//
//-------PLAYER DECLARATION-------//
//--------------------------------//

typedef enum player_effects {
    ADJUST, BORED, HESITATE, CONFUSED, mind_n_player_effects
} player_effects;


// threshold & skill are constant (as well as thread, n ofc) throughout the game.
struct player_t {
    stack_t hand;
    game_t *game;
    thread_t thread;
    float skill;                            // A constant between 0 and 1
    float focus;                            // A variable between 0 and 1
    uint32_t beat;                          // the player's internal time interval for synchronizing the game. May change during the game.
    uint32_t count;                         // number of beats since the round's start.
    uint32_t timeout[mind_n_player_effects]; // countdown for player effects that shouldn't repeat too often
    uint8_t pile_card;                       // the player keeps track of the pile's top card.
    uint8_t last_card_played;               // the last card that this player played
    uint8_t threshold;                      // the player's threshold for feeling like their smallest card should be played soon.
    uint8_t n;
};

void playerCreate(player_t *player, game_t *game);
void playerDeckShuffle(player_t *player);
void playTurn(player_t *player);
void playerAdjust(player_t *player, uint8_t top_card);
void playerBored(player_t *player);
void playerHesitate(player_t *player);
void playerConfused(player_t *player);
void playerFixFocus(player_t *player);
void playerFixBeat(player_t *player);
void playerTryPlay(player_t *player);
float playerGetError(player_t *player);
thread_return_t playGame(thread_arg_t _player);


//------------------------------//
//-------GAME DECLARATION-------//
//------------------------------//

struct game_t {
    stack_t deck;
    stack_t pile;
    mutex_t pile_mtx;
    mutex_t print_mtx;
    barrier_t barrier;
    struct player_t *players;
    struct {
        uint16_t n; // The level's number
        uint16_t n_cards;
        bool is_over; // true = over
        bool status; // true = win
    } level;
    uint8_t n_players;
};

void gameCreate(game_t *game, uint8_t n_players);
void gameDestroy(game_t *game);
void gameLevelSetup(game_t *game, uint8_t n_level);
void gameLevelNext(game_t *game);
void gameAssignBlame(game_t *game);
void gameLog(game_t *game, mind_stack_type_t type, ...);

//---------------------------//
//-----------UTILS-----------//
//---------------------------//

int reverseCompare (const void *arg1, const void *arg2);
void deckRuffle(stack_t *deck, player_t *player);
void deckMultiCut(stack_t *stack, player_t *player);
void deckShmush(stack_t *deck);
float randf(float min, float max);
uint32_t randi(uint32_t n, float err);
unsigned int trueRand(void);
