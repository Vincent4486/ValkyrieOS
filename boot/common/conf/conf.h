// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#define CONF_MAX_NAME_LEN  64
#define CONF_MAX_TITLE_LEN 64
#define CONF_MAX_PATH_LEN  128
#define CONF_MAX_ARGS_LEN  256

#define CONF_MAX_TOKEN_LEN (CONF_MAX_ARGS_LEN + 1)
#define CONF_MAX_PROFILES  8

#define CONF_LEX_ERR_STRING (-1)
#define CONF_LEX_ERR_LONG   (-2)

typedef enum
{
   CONF_T_EOF,
   CONF_T_NEWLINE,
   CONF_T_WORD,
   CONF_T_STRING,
   CONF_T_EQUALS,
   CONF_T_LBRACKET,
   CONF_T_RBRACKET,
} CONF_TokenType;

typedef struct
{
   char name[CONF_MAX_NAME_LEN];
   char title[CONF_MAX_TITLE_LEN];
   char root_label[CONF_MAX_TITLE_LEN];
   char path[CONF_MAX_PATH_LEN];
   char args[CONF_MAX_ARGS_LEN];
} CONF_BootProfile;

typedef struct
{
   int profile_count;
   int default_profile; // index into profiles[], -1 when none
   int timeout;
   CONF_BootProfile profiles[CONF_MAX_PROFILES];
} CONF_GlobalBoot;

typedef struct
{
   const char *src;
   int len;
   int pos;
   int line;
   CONF_TokenType type;
   char text[CONF_MAX_TOKEN_LEN];
   int text_len;
} CONF_Lexer;

int CONF_ParseConf(const char *path);
void CONF_GetGlobal(CONF_GlobalBoot **global);
void CONF_GetProfile(CONF_BootProfile **profile, int profile_id);

int CONF_LexerInit(CONF_Lexer *lexer, const char *src, int len);
int CONF_LexerNext(CONF_Lexer *lexer);
