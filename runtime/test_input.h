#ifndef DS_TEST_INPUT_H
#define DS_TEST_INPUT_H
#include "../probe/desert_pad.h"
void ds_test_input_pump(void);
void ds_test_input_sample(SDL_GameController *pad,const DS_PadConfig *config,DS_PadState *state);
#endif
