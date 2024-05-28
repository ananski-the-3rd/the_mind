// #include <memdbg/include/memdbg.h>
#include "mind.h"


int main(void) {
    srand(trueRand());

    game_t game;
    gameCreate(&game, MIND_N_PLAYERS);

    for (uint8_t i = 0; i < game.n_players; i++) {
        THREAD_CREATE(game.players[i].thread, playGame, &game.players[i]);
    }

    for (uint8_t i = 0; i < game.n_players; i++) {
        THREAD_JOIN(game.players[i].thread);
    }
    printf("\n~~~~~~~~~~~~~~~~~~~\n~~~~~GAME WON!~~~~~\n~~~~~~~~~~~~~~~~~~~\n");
    gameDestroy(&game);
    return 0;
}


//--------------------------------//
//------STACK IMPLEMENTATION------//
//--------------------------------//


/// @brief Create stack struct (malloc called once)
/// @param stack pointer to a stack struct. The shallow memory of the stack struct is managed by the caller
/// @param sz The stack's number of elements (cards)
void stackCreate(stack_t *stack, size_t sz) {
    
    stack->cards = malloc(sizeof(*stack->cards) * sz);
    if (stack->cards == NULL) {
        fprintf(stderr, "Out of memory!");
        exit(1);
    }
    stack->allocd = sz;
    stack->top = stack->cards;

    return;
}

/// @brief Frees the memory allocated for the stack's cards by 'stackCreate'
/// @param stack pointer to a stack struct. The shallow memory of the stack struct is managed by the caller
void stackDestroy(stack_t *stack) {
    if (stack->cards != NULL)
        free(stack->cards);
    return;
}

/// @brief Returns the size of a stack struct
/// @param stack pointer to a stack struct
/// @return The number of elements (cards) currently in the stack
uint32_t stackGetSize(stack_t *stack) {
    return (uint32_t)(stack->top - stack->cards);
}

/// @brief Adds a card to a stack
/// @param stack pointer to a stack struct
/// @param value The added card's value
void stackPush(stack_t *stack, uint8_t value) {
    stackPushN(stack, &value, 1);
    return;
}

/// @brief Removes a card from a stack
/// @param stack pointer to a stack struct
/// @return The removed card's value
uint8_t stackPop(stack_t *stack) {
    uint8_t res;
    stackPopN(stack, &res, 1);
    return res;
}

/// @brief Moves a card from one stack to another
/// @param dst destination stack
/// @param src source stack
void stackMove(stack_t *dst, stack_t *src) {
    stackMoveN(dst, src, 1);
    return;
}

/// @brief Adds multiple cards to a stack
/// @param stack pointer to a stack struct
/// @param values a pointer to an array of values
/// @param n the number of cards to be added
void stackPushN(stack_t *stack, uint8_t *values, size_t n) {
    uint32_t sz = stackGetSize(stack);
    if (sz > (stack->allocd - n)) {
        _threads_api_Panik("Not enough memory!");
    }
    memcpy_s(stack->top, stack->allocd - sz, values, n);
    stack->top += n;

    return;
}

/// @brief Removes multiple cards from a stack
/// @param stack pointer to a stack struct
/// @param res a pointer to an array where the resulting values should be stored
/// @param n the number of cards to be removed
void stackPopN(stack_t *stack, uint8_t *res, size_t n) {
    uint32_t sz = stackGetSize(stack);
    if (sz < n) {
        _threads_api_Panik("Not enough items!");
    }
    memcpy_s(res, sz, stack->top - n, n);
    stack->top -= n;

    return;
}

/// @brief Moves multiple cards from one stack to another
/// @param dst destination stack
/// @param src source stack
/// @param n the number of cards to be moved
void stackMoveN(stack_t *dst, stack_t *src, size_t n) {
    if (stackGetSize(dst) > (dst->allocd - n)) {
        _threads_api_Panik("Not enough memory!");
    }
    stackPopN(src, dst->top, n);
    dst->top += n;
    return;
}

/// @brief Outputs the top of the stack without removing it from the stack
/// @param stack pointer to a stack struct
/// @return the stack's top card
uint8_t stackPeek(stack_t *stack) {
    return *(stack->top - 1);
}

/// @brief Prints a stack as an array (format: "0, 1, 2, ... n.")
/// @param stack pointer to a stack struct
/// @param stack_name title to print before printing the values
void stackPrint(stack_t *stack, const char *stack_name) {
    printf("~~~~~%s~~~~~\n", stack_name);
    size_t sz = stackGetSize(stack);
    size_t i;

    for (i = 1; i < sz; i++) {
        printf("%d, ", *(stack->top - i));
    }
    printf("%d.\n", *(stack->cards));

    sz = strlen(stack_name) + 10;
    if (sz > 0x80) {
        _threads_api_Panik("stack_name too long or you forgot to terminate it with a null char!");
    }

    char buffer[0x80];
    for (i = 0; i < sz; i++) {
        buffer[i] = '~';
    }
    buffer[i] = '\0';
    printf("%s\n\n", buffer);
    return;
}


//---------------------------------//
//------PLAYER IMPLEMENTATION------//
//---------------------------------//

/// @brief Creates a player struct (no mallocs)
/// @param player pointer to a player struct. The shallow memory of the player struct is managed by the caller
/// @param game pointer to the game struct
void playerCreate(player_t *player, game_t *game) {

    *player = (player_t) {
        .game = game,
        .skill = randf(MIND_MIN_SKILL, MIND_MAX_SKILL),
        .n = (uint8_t)(player - game->players) + 1,
        .beat = randi(MIND_AVERAGE_BEAT, 0.15f)
    };

    stackCreate(&player->hand, MIND_MAX_LEVEL);
    
    return;
}

/// @brief Dictates the shuffling routine of a player
/// @param player pointer to a player struct
void playerDeckShuffle(player_t *player) {
    for (uint8_t i = 0; i < 7; i++) {
        deckRuffle(&player->game->deck, player);
        deckMultiCut(&player->game->deck, player);
        deckShmush(&player->game->deck);
    }
    
    return;
}

/// @brief The thread function at the heart of this program
/// @param arg pointer to a player struct
/// @return Players will try to win until successful and then return 0. Defeat is not an option!
thread_return_t playGame(thread_arg_t arg) {
    player_t *player = arg;
    game_t *game = player->game;
    static atomic_uint_least32_t n_players_ready = 0;
    static atomic_flag should_wait_for_setup = ATOMIC_FLAG_INIT;
    
    BARRIER_WAIT(game->barrier); // all threads
    while (game->level.n) {
        // Play
        while (!game->level.is_over && stackGetSize(&player->hand)) {
            playTurn(player);
            SLEEP(player->beat * (game->level.n / 4 + 1));
            player->count++;
        }
        
        // Only one thread will do the setup for the next level
        if (atomic_flag_test_and_set(&should_wait_for_setup)) {
            n_players_ready++;
            BARRIER_WAIT(game->barrier); // rest of threads
        } else {
            // Do the setup
            BARRIER_WAIT(game->barrier); // 1 thread
            gameLevelNext(game);
            n_players_ready = 0;
            atomic_flag_clear(&should_wait_for_setup);
        }
        
        BARRIER_WAIT(game->barrier); // all threads
    }
    
    return 0;
}

/// @brief Handles most of the internal logic
/// @param player pointer to a player struct
void playTurn(player_t *player) {
    game_t *game = player->game;  
    uint32_t pile_card = (stackGetSize(&game->pile))? stackPeek(&game->pile): 0;
    uint32_t lowest_card = stackPeek(&player->hand);

    // check that we haven't lost
    if (lowest_card < pile_card) {
        MUTEX_CHECKLOCK(game->pile_mtx, game->level.is_over);
        if (game->level.is_over) return;

        game->level.is_over = true;
        gameAssignBlame(game);

        MUTEX_UNLOCK(game->pile_mtx);
        return;
    }

    // player should adjust the current count and their beat according to the top card
    if (player->pile_card < pile_card) {
        MUTEX_CHECKLOCK(game->pile_mtx, game->level.is_over);
        if (game->level.is_over) return;
        playerAdjust(player, pile_card);
        MUTEX_UNLOCK(game->pile_mtx);
    }
    player->pile_card = pile_card;
    
    
    if (lowest_card < pile_card + player->threshold) {
        // player gets hasitant if close (higher focus, slower beat)
        playerHesitate(player);
    } else if ((randf(0.0F, 1.0F) * (1.0F - player->skill)) > 0.8F) {
        // player gets randomly confused - loses count
        playerConfused(player); // Don't make him an account
    } else {
        // player's default state is that of boredom/impatience (lower focus, higher beat)
        playerBored(player);
    }
    
    MUTEX_CHECKLOCK(game->pile_mtx, game->level.is_over);
    if (game->level.is_over) return;

    // player checks if they should play and if they should - they do.
    playerTryPlay(player);

    // Check if this is the last card (=Win!)
    if (game->level.n_cards == 0) {
        game->level.is_over = true;
        game->level.status = true;
    }
    MUTEX_UNLOCK(player->game->pile_mtx);

    return;
}

/// @brief Adjust the player's beat such that their count since the previous card played would have reached req_card. Adjustment may be in either direction
/// @param player pointer to a player struct
/// @param req_card the card that the player should adjust for
void playerAdjust(player_t *player, uint8_t req_card) {
    if (player->timeout[ADJUST]) {
        player->timeout[ADJUST]--;
        return;
    }
    player->timeout[ADJUST] = 2;

    float n_beats_passed = player->count - player->pile_card;
    float n_beats_should_have_passed = req_card - player->pile_card;
    float old_beat = (float)player->beat;
    float new_beat = old_beat * n_beats_passed / n_beats_should_have_passed;
    uint32_t avg_beat = (uint32_t)((new_beat + old_beat) * 0.5F);
    player->beat = randi(avg_beat, playerGetError(player) * 0.5F);
    playerFixBeat(player);
    
    return;
}

/// @brief Status effect - decreases focus and accelerates beat
/// @param player pointer to a player struct
void playerBored(player_t *player) {
    if (player->timeout[BORED]) {
        player->timeout[BORED]--;
        return;
    }
    player->timeout[BORED] = player->threshold;

    player->focus *= 0.95F;
    float err = playerGetError(player) * 0.25F;
    player->beat = randi(player->beat * (1.0F - err), err / (1.0F + err));

    playerFixBeat(player);
    playerFixFocus(player);
    return;
}

/// @brief Status effect - increases focus and slows beat
/// @param player pointer to a player struct
void playerHesitate(player_t *player) {
    if (player->timeout[HESITATE]) {
        player->timeout[HESITATE]--;
        return;
    }
    player->timeout[HESITATE] = player->threshold;

    player->focus += 0.01;
    player->focus *= 1.05F;
    float err = playerGetError(player) * 0.25F;
    player->beat = randi(player->beat * (1.0F + err), err / (1.0F + err));

    playerFixBeat(player);
    playerFixFocus(player);
    return;
}

/// @brief Status effect - greatly decreases focus and messes with count
/// @param player pointer to a player struct
void playerConfused(player_t *player) {
    if (player->timeout[CONFUSED]) {
        player->timeout[CONFUSED]--;
        return;
    }
    player->timeout[CONFUSED] = 3;

    player->focus *= 0.9F;
    player->count = randi(player->count, playerGetError(player) * 0.5F);
    playerFixFocus(player);
    
    return;
}

/// @brief Make sure that 0 < focus < 1
/// @param player pointer to a player struct
void playerFixFocus(player_t *player) {
    if (player->focus > 0.99F) {
        player->focus = 0.9F;
    } else if (player->focus < 0.01F) {
        player->focus = 0.1F;
    }
    return;
}

/// @brief Make sure that MIND_MIN_BEAT < beat < MIND_MAX_BEAT
/// @param player pointer to a player struct
void playerFixBeat(player_t *player) {
    if (player->beat < MIND_MIN_BEAT) {
        player->beat = MIND_MIN_BEAT;
    } else if (player->beat > MIND_MAX_BEAT) {
        player->beat = MIND_MAX_BEAT;
    }
    
    return;
}


/// @brief Decide if player should play and handle the playing itself
/// @param player pointer to a player struct
void playerTryPlay(player_t *player) {
    uint32_t lowest_card = stackPeek(&player->hand);
    game_t *game = player->game;

    if (game->level.n_cards > 1 &&                                     // always play the round's final card instantly
        (player->count < lowest_card ||                                // play if count reaches player's lowest card -
         (lowest_card + game->level.n_cards - 1) > MIND_DECK_SIZE)) {  // - unless card is so high that there are definitely lower cards
        return;
    }

    stackMove(&game->pile, &player->hand);
    player->last_card_played = lowest_card;
    printf("P%02d plays %d\n", player->n + 1, lowest_card);
    player->count = lowest_card;
    player->pile_card = lowest_card;
    game->level.n_cards--;
    
    return;
}

/// @brief Use player's skill and focus to calculate error, for use in calculations involving randomness.
/// @param player pointer to a player struct
/// @return float between 0 and 1 denoting error %
float playerGetError(player_t *player) {
    return ((1.0F - player->skill) * (1.0F - player->focus));
}


//---------------------------------//
//-------GAME IMPLEMENTATION-------//
//---------------------------------//

/// @brief Creates the game struct
/// @param game pointer to the game struct. The shallow memory of the game struct is managed by the caller
/// @param n_players the number of players in the game (constant)
void gameCreate(game_t *game, uint8_t n_players) {
    *game = (game_t) {
        .n_players = n_players,
        .players = malloc(sizeof(player_t) * n_players)
    };

    for (player_t *player = game->players; player < game->players + game->n_players; player++) {
        playerCreate(player, game);
    }

    stackCreate(&game->deck, MIND_DECK_SIZE);
    for (uint8_t i = MIND_DECK_SIZE; i > 0; i--) {
        stackPush(&game->deck, i);
    }

    stackCreate(&game->pile, MIND_DECK_SIZE);
    
    MUTEX_INIT(game->pile_mtx);
    MUTEX_INIT(game->print_mtx);
    BARRIER_INIT(game->barrier, n_players);

    gameLevelSetup(game, 1);

    return;
}

/// @brief Destroys game struct, freeing all the internal memory allocated for its children
/// @param game pointer to the game struct 
void gameDestroy(game_t *game) {
    stackDestroy(&game->deck);
    stackDestroy(&game->pile);
    
    for (size_t i = 0 ; i < game->n_players; i++) {
        stackDestroy(&game->players[i].hand);
    }

    free(game->players);
    MUTEX_DESTROY(game->pile_mtx);
    MUTEX_DESTROY(game->print_mtx);
    BARRIER_DESTROY(game->barrier);
    return;
}

/// @brief Announce the level, deal cards, handle params
/// @param game pointer to the game struct 
/// @param n_level the level we are on (starts with 1; 0 is a signal to end the game)
void gameLevelSetup(game_t *game, uint8_t n_level) {

    // Each round, a different player shuffles the deck
    playerDeckShuffle(&game->players[n_level % game->n_players]);
    
    // Print level and deck
    printf("~~~~~~~~~~~~~~~~~~\n"    \
           "~~~~~LEVEL %02d~~~~~\n" \
           "~~~~~~~~~~~~~~~~~~\n\n"  ,
           n_level);
    gameLog(game, DECK);

    size_t deck_size = stackGetSize(&game->deck);
    // Handing cards to players, sorted descending; set player counts etc.
    uint8_t *temp_buffer = malloc(sizeof(*temp_buffer) * n_level);
    for (player_t *player = game->players; player < game->players + game->n_players; player++) {
        stackPopN(&game->deck, temp_buffer, n_level);
        qsort(temp_buffer, n_level, sizeof(temp_buffer[0]), reverseCompare);
        stackPushN(&player->hand, temp_buffer, n_level);
        player->threshold = randi(deck_size / (n_level * game->n_players), playerGetError(player));
        player->focus = 0.5F;
        player->count = 0;
        memset(player->timeout, 0, mind_n_player_effects * sizeof(player->timeout[0]));
        player->pile_card = 0;
        player->last_card_played = 0;
        player->n = player - game->players;

        gameLog(game, HAND, player->n);
    }
    free(temp_buffer);

    game->level.n = n_level;
    game->level.n_cards = game->level.n * game->n_players;
    game->level.is_over = false;
    game->level.status = false;
    
    return;
}

/// @brief Report last level's status, return player hands to the deck, check win condition, setup next level
/// @param game pointer to the game struct 
void gameLevelNext(game_t *game) {
    
    if (game->level.status) {
        printf("\nLEVEL %02d WON!\n", game->level.n);
    } else {
        printf("\nLEVEL %02d LOST! resetting...\n", game->level.n);
    }
    SLEEP(3000);

    for (player_t *player = game->players; player < game->players + game->n_players; player++) {
        stackMoveN(&game->deck, &player->hand, stackGetSize(&player->hand));
    }
    stackMoveN(&game->deck, &game->pile, stackGetSize(&game->pile));

    if (game->level.n == MIND_MAX_LEVEL && game->level.status) {
        game->level.n = 0; // signal for win
        return;
    }
    // If we lost, reset to level 1, otherwise advance to the next level
    gameLevelSetup(game, (game->level.n * game->level.status) + 1);
    return;
}

/// @brief Ooooo, Ahhhh! Adjust the two players that are the most responsible for the loss. TODO: make the adjustment more dynamic and on more players
/// @param game pointer to the game struct 
void gameAssignBlame(game_t *game) {
    uint8_t pile_card = stackPeek(&game->pile);
    // find the lowest card in hands that is lower than the pile's card
    uint8_t lowest = MIND_DECK_SIZE;
    uint8_t i_slow_player = 0;
    for (size_t i = 0; i < game->n_players; i++) {
        player_t *player = &game->players[i];
        if (stackGetSize(&player->hand) == 0) continue;
        uint8_t card = stackPeek(&player->hand);
        if (card < lowest) {
            i_slow_player = i;
            lowest = card;
        }
    }

    uint8_t first = pile_card;
    uint8_t i_fast_player = 0;
    // find the player who played the first card that is higher than the lowest
    for (size_t i = 0; i < game->n_players; i++) {
        uint8_t card = game->players[i].last_card_played;
        if (card > lowest && card < first) {
            i_fast_player = i;
            first = card;
        }
    }
    playerAdjust(&game->players[i_slow_player], first);
    playerAdjust(&game->players[i_fast_player], lowest);
    return;
}


/// @brief A logging function that prints either deck, pile or player hand to stdout
/// @param game pointer to the game struct
/// @param type DECK, PILE or HAND
/// @param n_player (uint32_t) (optional): in case HAND was chosen, n_player specifies which player's hand is to be printed.
void gameLog(game_t *game, mind_stack_type_t type, ...) {
    va_list args;
    va_start(args, type);
    char stack_name[0x80] = {0};
    stack_t *stack;
    switch (type) {
        case DECK:
            strcpy_s(stack_name, 0x80, "DECK");
            stack = &game->deck;
            break;
        case PILE:
            strcpy_s(stack_name, 0x80, "PILE");
            stack = &game->pile;
            break;
        case HAND:
            ; // c is dumb
            uint32_t n_player = va_arg(args, uint32_t);
            sprintf_s(stack_name, 0x80, "PLAYER %02d HAND", n_player + 1);
            stack = &game->players[n_player].hand;
            break;
        default:
            _threads_api_Panik("Unknown mind_stack_type_t. Should be DECK, PILE or HAND");
    }
    MUTEX_LOCK(game->print_mtx);
    stackPrint(stack, stack_name);
    MUTEX_UNLOCK(game->print_mtx);
    return;
}

//---------------------------//
//-----------UTILS-----------//
//---------------------------//

/// @brief Helper function for sorting the cards in players' hands
/// @param arg1 card
/// @param arg2 card 
/// @return a negative number if arg1 > arg2, a positive if arg2 > arg1, 0 if arg1 == arg2
int reverseCompare (const void *arg1, const void *arg2) {
    int a = *(const uint8_t *)arg1;
    int b = *(const uint8_t *)arg2;
    return b - a;
}

/// @brief Shuffles by interleaving two halfs of the deck. Accuracy depends on player's skill
/// @param deck The deck of cards containing numbers 1 to 100
/// @param player pointer to a player struct
void deckRuffle(stack_t *deck, player_t *player) {
    uint32_t sz = stackGetSize(deck);
    uint32_t half_deck = randi(sz / 2, playerGetError(player));
    uint8_t temp_deck[MIND_DECK_SIZE];
     
    uint8_t *halfs[] = {deck->cards, deck->cards + half_deck};
    uint8_t i_halfs = 0;

    for (uint8_t i = 0; i < sz; i++) {
        // stop shuffling if one of the halfs is finished
        if (halfs[0] >= deck->cards + half_deck) {
            memcpy_s(&temp_deck[i], sz - i, halfs[1], deck->top - halfs[1]);
            break;
        } else if (halfs[1] >= deck->top) {
            memcpy_s(&temp_deck[i], sz - i, halfs[0], deck->cards + half_deck - halfs[0]);
            break;
        }

        // the actual shuffle
        i_halfs += (randf(0, 1) < player->skill);
        i_halfs &= 1;
        temp_deck[i] = *halfs[i_halfs]++;
    }

    memcpy_s(deck->cards, MIND_DECK_SIZE, temp_deck, sz);
    return;
}

/// @brief Move a small packet of cards from the top to the bottom of the deck, multiple times in a row
/// @param deck The deck of cards containing numbers 1 to 100
/// @param player pointer to a player struct
void deckMultiCut(stack_t *deck, player_t *player) {
    static const uint32_t MIN_REPS = 2;
    static const uint32_t MAX_REPS = 6;

    uint32_t sz = stackGetSize(deck);
    uint8_t temp_deck[MIND_DECK_SIZE];
    uint8_t n_reps = rand() % (MAX_REPS - MIN_REPS) + MIN_REPS;
    uint32_t half_deck = randi(sz / n_reps, playerGetError(player));
    uint32_t acc;

    for (acc = half_deck; acc < sz; acc += half_deck) {
         memcpy_s(temp_deck + sz - acc, sz, deck->cards + acc - half_deck, half_deck);
         half_deck = randi(sz / n_reps, playerGetError(player));
    }
    acc -= half_deck;
    memcpy_s(temp_deck, sz, deck->cards + acc, sz - acc);

    memcpy_s(deck->cards, sz, temp_deck, sz);
    return;
}

/// @brief Randomly smear the cards on the table. Amounts to randomly transfering packets from anywhere to anywhere in the pile.
/// @param deck The deck of cards containing numbers 1 to 100
void deckShmush(stack_t *deck) {
    static const uint32_t MIN_REPS = 8;
    static const uint32_t MAX_REPS = 16;
    
    uint32_t sz = stackGetSize(deck);
    uint8_t temp_deck[MIND_DECK_SIZE];
    uint8_t n_reps = rand() % (MAX_REPS - MIN_REPS) + MIN_REPS;

    for (uint32_t i = 0; i < n_reps; i++) {
        uint32_t n = ((rand() % (sz / 8)) + 8) * sizeof(*temp_deck); // from 8 to 20 cards in each shmush
        uint8_t *src = deck->cards + rand() % (sz - n);
        uint8_t *dst = deck->cards + rand() % (sz - (3 * n));
        uintptr_t diff = (src > dst)? src - dst: dst - src;
        if (diff < n) {
            dst += 2 * n;
        }
        
        memcpy_s(temp_deck, sz, dst, n);
        memcpy_s(dst, sz, src, n);
        memcpy_s(src, sz, temp_deck, n);
    }

    return;
}

/// @brief Generates a float between two numbers
/// @param a min
/// @param b max
/// @return a float f such that a < f < b
float randf(float a, float b) {
   return ((b - a) * ((float)rand() / (RAND_MAX + 1))) + a;
}

/// @brief Generates an integer around n
/// @param n the average number to be generated
/// @param err the % around n that may be generated in each direction
/// @return an integer m such that n * (1 - err) < m < n * (1 + err)
uint32_t randi(uint32_t n, float err) {
    uint32_t e = n * err;
    if (!e) return n;
    return ((rand() % (2 * e)) + n - e);
}


/// @brief Generates a truly random number for seeding into rand()
unsigned int trueRand(void) {
    unsigned int res = 0;
    int n_fails = 0;
    while (!_rdseed32_step(&res)) {
        n_fails++;
        if (n_fails > 1000) {
            exit(EXIT_FAILURE);
        }
    }
    return res;
}