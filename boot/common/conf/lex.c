// SPDX-License-Identifier: GPL-3.0-only

#include <status.h>

#include <dl/bindgen.h>

#include "conf.h"

static int lexer_is_space(char c)
{
   return c == ' ' || c == '\t' || c == '\r';
}

static int lexer_is_word_char(char c)
{
   return c != '\0' && c != '\n' && !lexer_is_space(c) && c != '#' &&
          c != '=' && c != '[' && c != ']' && c != '"' && c != '\'';
}

static void lexer_clear(CONF_Lexer *lexer)
{
   lexer->text[0] = '\0';
   lexer->text_len = 0;
}

static int lexer_push(CONF_Lexer *lexer, char c)
{
   if (lexer->text_len >= CONF_MAX_TOKEN_LEN - 1)
      return CONF_LEX_ERR_LONG;

   lexer->text[lexer->text_len++] = c;
   return SUCCESS;
}

static int lexer_read_word(CONF_Lexer *lexer)
{
   int rc;

   lexer_clear(lexer);

   while (lexer->pos < lexer->len && lexer_is_word_char(lexer->src[lexer->pos]))
   {
      rc = lexer_push(lexer, lexer->src[lexer->pos]);
      if (rc != SUCCESS)
         return rc;

      lexer->pos++;
   }

   lexer->text[lexer->text_len] = '\0';
   lexer->type = CONF_T_WORD;
   return SUCCESS;
}

static int lexer_read_string(CONF_Lexer *lexer, char quote)
{
   int rc;

   lexer_clear(lexer);
   lexer->pos++; /* skip opening quote */

   while (lexer->pos < lexer->len)
   {
      char c = lexer->src[lexer->pos++];

      if (c == quote)
      {
         lexer->text[lexer->text_len] = '\0';
         lexer->type = CONF_T_STRING;
         return SUCCESS;
      }

      if (c == '\\')
      {
         if (lexer->pos >= lexer->len)
            break;

         c = lexer->src[lexer->pos++];
         if (c == 'n')
            c = '\n';
         else if (c == 't')
            c = '\t';
         else if (c == 'r')
            c = '\r';
      }

      rc = lexer_push(lexer, c);
      if (rc != SUCCESS)
         return rc;
   }

   return CONF_LEX_ERR_STRING;
}

_DL_FORCE_EXCLUDE
int CONF_LexerInit(CONF_Lexer *lexer, const char *src, int len)
{
   if (!lexer || !src || len < 0)
      return -EINVAL;

   lexer->src = src;
   lexer->len = len;
   lexer->pos = 0;
   lexer->line = 1;
   lexer->type = CONF_T_EOF;
   lexer_clear(lexer);
   return SUCCESS;
}

_DL_FORCE_EXCLUDE
int CONF_LexerNext(CONF_Lexer *lexer)
{
   int rc;

   if (!lexer)
      return -EINVAL;

   for (;;)
   {
      char c;

      if (lexer->pos >= lexer->len)
      {
         lexer_clear(lexer);
         lexer->type = CONF_T_EOF;
         return SUCCESS;
      }

      c = lexer->src[lexer->pos];

      if (lexer_is_space(c))
      {
         lexer->pos++;
         continue;
      }

      if (c == '#')
      {
         while (lexer->pos < lexer->len && lexer->src[lexer->pos] != '\n')
            lexer->pos++;
         continue;
      }

      if (c == '\n')
      {
         lexer->pos++;
         lexer->line++;
         lexer_clear(lexer);
         lexer->type = CONF_T_NEWLINE;
         return SUCCESS;
      }

      if (c == '=')
      {
         lexer->pos++;
         lexer_clear(lexer);
         lexer->type = CONF_T_EQUALS;
         return SUCCESS;
      }

      if (c == '[')
      {
         lexer->pos++;
         lexer_clear(lexer);
         lexer->type = CONF_T_LBRACKET;
         return SUCCESS;
      }

      if (c == ']')
      {
         lexer->pos++;
         lexer_clear(lexer);
         lexer->type = CONF_T_RBRACKET;
         return SUCCESS;
      }

      if (c == '"' || c == '\'')
      {
         rc = lexer_read_string(lexer, c);
         if (rc != SUCCESS)
            return rc;

         return SUCCESS;
      }

      rc = lexer_read_word(lexer);
      if (rc != SUCCESS)
         return rc;

      return SUCCESS;
   }
}
