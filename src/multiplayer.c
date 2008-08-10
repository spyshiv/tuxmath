/*

multiplayer.h - Provides routines for organizing and running a turn-based
                multiplayer that can accommodate up to four players (more with
                a recompilation)

Author: B. Luchen

*/

#include "SDL.h"
#include "multiplayer.h"
#include "game.h"
#include "options.h"
#include "fileops.h"
#include "highscore.h"
#include "credits.h"

int params[NUM_PARAMS] = {0, 0, 0, 0};

int inprogress = 0;
int pscores[MAX_PLAYERS];
char* pnames[MAX_PLAYERS];

//local function decs
static void showWinners(int* order, int num); //show a sequence recognizing winner
static int initMP();
static void cleanupMP();

void mp_set_parameter(unsigned int param, int value)
{
  if (inprogress)
  {
    tmdprintf("Oops, tried to set param %d in the middle of a game\n", param);
    return;
  }
  params[param] = value;
}

void mp_run_multiplayer()
{
  int i;
  int round = 1;
  int currentplayer = 0;
  int result = 0;
  int done = 0;
  int activeplayers = params[PLAYERS];
  int winners[MAX_PLAYERS];

  for (i = 0; i < MAX_PLAYERS; ++i)
    winners[i] = -1;

  if (initMP() )
  {
    printf("Initialization failed, bailing out\n");
    return;
  }


  if (params[MODE] == ELIMINATION)
  {
    while(!done)
    {

      game_set_start_message(pnames[currentplayer], "Go!", "", "");
      result = game();

      if (result == GAME_OVER_LOST || result == GAME_OVER_ESCAPE)
      {
        //eliminate player
        pscores[currentplayer] = 0xbeef;
        winners[--activeplayers] = currentplayer;
      }

      while (pscores[currentplayer] == 0xbeef) //skip over eliminated players
      {
        ++currentplayer;
        currentplayer %= params[PLAYERS];
        if (currentplayer == 0)
          ++round;
      }
      if (activeplayers <= 1) //last man standing!
      {
//        showWinners(winners, params[PLAYERS]);
        tmdprintf("%d wins\n", currentplayer);
        winners[0] = currentplayer;
        done = 1;
      }
    }
  }
  else if (params[MODE] == SCORE_SWEEP)
  {
    int hiscore = 0;
    int currentwinner = -1;

    for (round = 1; round <= params[ROUNDS]; ++round)
    {
      for (currentplayer = 0; currentplayer < params[PLAYERS]; ++currentplayer)
      {
        game_set_start_message(pnames[currentplayer], "Go!", NULL, NULL);
        result = game();
        pscores[currentplayer] += Opts_LastScore(); //add this player's score
        if (result == GAME_OVER_WON)
          pscores[currentplayer] += 500; //plus a possible bonus
      }
    }
    for (i = 0; i < params[PLAYERS]; ++i)
    {
      for (currentplayer = 0; currentplayer < params[PLAYERS]; ++currentplayer)
      {
        if (pscores[currentplayer] > hiscore)
        {
          hiscore = pscores[currentplayer];
          currentwinner = currentplayer;
        }
      winners[i] = currentwinner;
      pscores[currentwinner] = 0;
      }
    }
  }
  tmdprintf("Game over; showing winners\n");
  showWinners(winners, params[PLAYERS]);
  cleanupMP();
}

int mp_get_player_score(int playernum)
{
  return pscores[playernum];
}

const char* mp_get_player_name(int playernum)
{
  return pnames[playernum];
}

int mp_get_param(int p)
{
  if (p < 0 || p > NUM_PARAMS)
  {
    printf("Invalid mp_param index: %d\n", p);
    return 0;
  }
  return params[p];
}


void showWinners(int* winners, int num)
{
  int skip = 0;
  const int boxspeed = 3;
  char text[HIGH_SCORE_NAME_LENGTH + strlen(" wins!")];
  SDL_Rect box = {screen->w / 2, screen->h / 2, 0, 0};
  SDL_Rect center = box;
  SDL_Event evt;

  tmdprintf(pnames[winners[0]] );
  tmdprintf("%d\n", snprintf(text, HIGH_SCORE_NAME_LENGTH + strlen(" wins!"),
                    "%s wins!", pnames[winners[0]]) );
  tmdprintf("Win text: %s\n", text);

  DarkenScreen(1);

  while (box.h < screen->h || box.w < screen->w)
  {
    box.x -= boxspeed;
    box.y -= boxspeed;
    box.h += boxspeed * 2;
    box.w += boxspeed * 2;

    SDL_FillRect(screen, &box, 0);
    draw_text(text, center);
    SDL_UpdateRect(screen, box.x, box.y, box.w, box.h);

    while (SDL_PollEvent(&evt) )
      if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE)
        skip = 1;
    if (skip)
      break;
    SDL_Delay(50);
  }

  SDL_FillRect(screen, NULL, 0);
  draw_text(text, center);
  SDL_Flip(screen);
  WaitForKeypress();
}

int initMP()
{
  int i;
  char nrstr[HIGH_SCORE_NAME_LENGTH * 3];
  int nplayers = params[PLAYERS];

  const char* config_files[5] = {
    "multiplay/space_cadet",
    "multiplay/scout",
    "multiplay/ranger",
    "multiplay/ace",
    "multiplay/commando"
  };

  tmdprintf("Reading in difficulty settings...\n");
  if (!read_global_config_file() ||
      !read_named_config_file("multiplay/mpoptions") ||
      !read_named_config_file(config_files[params[DIFFICULTY]]) )
  {
    printf("Couldn't read in settings for %s\n",
           config_files[params[DIFFICULTY]] );
    return 1;
  }

  pscores[0] = pscores[1] = pscores[2] = pscores[3] = 0;
  pnames[0] = pnames[1] = pnames[2] = pnames[3] = NULL;

  //allocate and enter player names
  for (i = 0; i < nplayers; ++i)
    pnames[i] = malloc((1 + 3 * HIGH_SCORE_NAME_LENGTH) * sizeof(char) );
  for (i = 0; i < nplayers; ++i)
    if (pnames[i])
      NameEntry(pnames[i], "Who is playing?", "Enter your name:");
    else
      {
        printf("Can't allocate name %d!\n", i);
        return 1;
      }
  if (params[MODE] == SCORE_SWEEP)
  {
    while (params[ROUNDS] <= 0)
    {
      NameEntry(nrstr, "How many rounds will you play?", "Enter a number");
      params[ROUNDS] = atoi(nrstr);
    }
  }
  inprogress = 1; //now we can start the game
  return 0;
}

void cleanupMP()
{
  int i;

  for (i = 0; i < params[PLAYERS]; ++i)
    if (pnames[i])
      free(pnames[i]);
  inprogress = 0;
}