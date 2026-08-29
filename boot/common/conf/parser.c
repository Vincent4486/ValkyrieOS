// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>

#include <dl/bindgen.h>
#include <dl/callback.h>
#include <logging.h>
#include <status.h>

#include "conf.h"

typedef struct CONF_FieldDef CONF_FieldDef;
typedef struct CONF_MenuConfig CONF_MenuConfig;

static int conf_str_equal(const char *a, const char *b);
static int conf_str_startswith(const char *str, const char *prefix);
static int conf_copy_str(char *dst, size_t dst_size, const char *src);
static int conf_parse_int(const char *s, int *out);
static int conf_find_field(const CONF_FieldDef *fields, int count,
                           const char *key);
static int conf_apply_field(const CONF_FieldDef *def, const char *value,
                            void *base);
static int conf_find_profile(CONF_GlobalBoot *global, const char *name);
static int conf_read_file(int fd, char *buf, int cap, int *out_len);
static int conf_lex_error(CONF_Lexer *lexer, int rc);
static int conf_parse_section(CONF_Lexer *lexer, CONF_GlobalBoot *global,
                              int *section, CONF_BootProfile **profile);
static int conf_parse_assignment(CONF_Lexer *lexer, int section,
                                 CONF_BootProfile *profile,
                                 CONF_MenuConfig *menu, uint32_t *menu_seen,
                                 uint32_t *profile_seen);

#define logfmt g_DlCallbackOps->logfmt

#define CONF_MAX_FILE_SIZE 8192

#define SECTION_NONE 0
#define SECTION_MENU 1
#define SECTION_PROFILE 2

typedef enum
{
   CONF_FIELD_STRING,
   CONF_FIELD_INT,
} CONF_FieldKind;

struct CONF_FieldDef
{
   const char *key;
   CONF_FieldKind kind;
   size_t offset;
   size_t max_len;
};

struct CONF_MenuConfig
{
   char default_name[CONF_MAX_NAME_LEN];
   int timeout;
};

static const CONF_FieldDef s_MenuFields[] = {
   { "default", CONF_FIELD_STRING, offsetof(CONF_MenuConfig, default_name),
     CONF_MAX_NAME_LEN },
   { "timeout", CONF_FIELD_INT, offsetof(CONF_MenuConfig, timeout), 0 },
};

static const CONF_FieldDef s_ProfileFields[] = {
   { "title", CONF_FIELD_STRING, offsetof(CONF_BootProfile, title),
     CONF_MAX_TITLE_LEN },
   { "root", CONF_FIELD_STRING, offsetof(CONF_BootProfile, root_label),
     CONF_MAX_TITLE_LEN },
   { "path", CONF_FIELD_STRING, offsetof(CONF_BootProfile, path),
     CONF_MAX_PATH_LEN },
   { "args", CONF_FIELD_STRING, offsetof(CONF_BootProfile, args),
     CONF_MAX_ARGS_LEN },
};

static char s_ConfFileBuf[CONF_MAX_FILE_SIZE];

static int conf_str_equal(const char *a, const char *b)
{
   while (*a != '\0' && *a == *b)
   {
      a++;
      b++;
   }

   return *a == *b;
}

static int conf_str_startswith(const char *str, const char *prefix)
{
   while (*prefix != '\0')
   {
      if (*str != *prefix)
         return 0;

      str++;
      prefix++;
   }

   return 1;
}

static int conf_copy_str(char *dst, size_t dst_size, const char *src)
{
   size_t i = 0;

   while (src[i] != '\0')
   {
      if (i + 1 >= dst_size)
         return -EINVAL;

      dst[i] = src[i];
      i++;
   }

   dst[i] = '\0';
   return SUCCESS;
}

static int conf_parse_int(const char *s, int *out)
{
   int value = 0;
   int digits = 0;

   while (*s >= '0' && *s <= '9')
   {
      if (digits >= 9)
         return -EINVAL;

      value = value * 10 + (*s - '0');
      digits++;
      s++;
   }

   if (*s != '\0' || digits == 0)
      return -EINVAL;

   *out = value;
   return SUCCESS;
}

static int conf_find_field(const CONF_FieldDef *fields, int count,
                           const char *key)
{
   int i;

   for (i = 0; i < count; i++)
   {
      if (conf_str_equal(fields[i].key, key))
         return i;
   }

   return -1;
}

static int conf_apply_field(const CONF_FieldDef *def, const char *value,
                            void *base)
{
   char *dst = (char *)base + def->offset;

   if (def->kind == CONF_FIELD_INT)
   {
      int parsed;

      if (conf_parse_int(value, &parsed) != SUCCESS)
         return -EINVAL;

      *(int *)dst = parsed;
      return SUCCESS;
   }

   if (conf_copy_str(dst, def->max_len, value) != SUCCESS)
      return -EINVAL;

   return SUCCESS;
}

static int conf_find_profile(CONF_GlobalBoot *global, const char *name)
{
   int i;

   for (i = 0; i < global->profile_count; i++)
   {
      if (conf_str_equal(global->profiles[i].name, name))
         return i;
   }

   return -1;
}

static int conf_read_file(int fd, char *buf, int cap, int *out_len)
{
   int total = 0;

   for (;;)
   {
      int rc;

      if (total >= cap)
      {
         char extra;

         /* One extra byte tells an exact fit apart from overflow. */
         rc = g_DlCallbackOps->Read(fd, &extra, 1);
         if (rc < 0)
            return rc;

         if (rc > 0)
         {
            logfmt(LOG_ERROR, "conf: file exceeds %d bytes\n", cap);
            return -ENOMEM;
         }

         *out_len = total;
         return SUCCESS;
      }

      rc = g_DlCallbackOps->Read(fd, buf + total, cap - total);
      if (rc < 0)
         return rc;

      if (rc == 0)
      {
         *out_len = total;
         return SUCCESS;
      }

      total += rc;
   }
}

static int conf_lex_error(CONF_Lexer *lexer, int rc)
{
   if (rc == CONF_LEX_ERR_STRING)
      logfmt(LOG_ERROR, "conf: line %d: unterminated string\n", lexer->line);
   else if (rc == CONF_LEX_ERR_LONG)
      logfmt(LOG_ERROR, "conf: line %d: token too long\n", lexer->line);

   return -EINVAL;
}

static int conf_parse_section(CONF_Lexer *lexer, CONF_GlobalBoot *global,
                              int *section, CONF_BootProfile **profile)
{
   int rc;

   rc = CONF_LexerNext(lexer);
   if (rc < 0)
      return conf_lex_error(lexer, rc);

   if (lexer->type != CONF_T_WORD)
   {
      logfmt(LOG_ERROR, "conf: line %d: expected section name\n", lexer->line);
      return -EINVAL;
   }

   if (conf_str_equal(lexer->text, "menu"))
   {
      *section = SECTION_MENU;
      *profile = NULL;
   }
   else if (conf_str_startswith(lexer->text, "profile."))
   {
      const char *name = lexer->text + 8;

      if (name[0] == '\0')
      {
         logfmt(LOG_ERROR, "conf: line %d: profile name is empty\n",
                lexer->line);
         return -EINVAL;
      }

      if (global->profile_count >= CONF_MAX_PROFILES)
      {
         logfmt(LOG_ERROR, "conf: too many profiles (max %d)\n",
                CONF_MAX_PROFILES);
         return -EINVAL;
      }

      if (conf_find_profile(global, name) >= 0)
      {
         logfmt(LOG_ERROR, "conf: duplicate profile '%s'\n", name);
         return -EINVAL;
      }

      *profile = &global->profiles[global->profile_count];
      if (conf_copy_str((*profile)->name, sizeof((*profile)->name), name) !=
          SUCCESS)
      {
         logfmt(LOG_ERROR, "conf: line %d: profile name too long\n",
                lexer->line);
         return -EINVAL;
      }

      global->profile_count++;
      *section = SECTION_PROFILE;
   }
   else
   {
      logfmt(LOG_ERROR, "conf: line %d: unknown section '%s'\n", lexer->line,
             lexer->text);
      return -EINVAL;
   }

   rc = CONF_LexerNext(lexer);
   if (rc < 0)
      return conf_lex_error(lexer, rc);

   if (lexer->type != CONF_T_RBRACKET)
   {
      logfmt(LOG_ERROR, "conf: line %d: expected ']'\n", lexer->line);
      return -EINVAL;
   }

   return SUCCESS;
}

static int conf_parse_assignment(CONF_Lexer *lexer, int section,
                                 CONF_BootProfile *profile,
                                 CONF_MenuConfig *menu, uint32_t *menu_seen,
                                 uint32_t *profile_seen)
{
   const CONF_FieldDef *fields;
   int field_count;
   uint32_t *seen;
   void *base;
   int field;
   int rc;

   if (section == SECTION_MENU)
   {
      fields = s_MenuFields;
      field_count = (int)(sizeof(s_MenuFields) / sizeof(s_MenuFields[0]));
      seen = menu_seen;
      base = menu;
   }
   else if (section == SECTION_PROFILE)
   {
      if (!profile)
      {
         logfmt(LOG_ERROR, "conf: line %d: no active profile\n", lexer->line);
         return -EINVAL;
      }

      fields = s_ProfileFields;
      field_count = (int)(sizeof(s_ProfileFields) / sizeof(s_ProfileFields[0]));
      seen = profile_seen;
      base = profile;
   }
   else
   {
      logfmt(LOG_ERROR, "conf: line %d: key outside section\n", lexer->line);
      return -EINVAL;
   }

   field = conf_find_field(fields, field_count, lexer->text);
   if (field < 0)
   {
      logfmt(LOG_ERROR, "conf: line %d: unknown key '%s'\n", lexer->line,
             lexer->text);
      return -EINVAL;
   }

   if (*seen & (1u << field))
   {
      logfmt(LOG_ERROR, "conf: line %d: duplicate key '%s'\n", lexer->line,
             lexer->text);
      return -EINVAL;
   }

   rc = CONF_LexerNext(lexer);
   if (rc < 0)
      return conf_lex_error(lexer, rc);

   if (lexer->type != CONF_T_EQUALS)
   {
      logfmt(LOG_ERROR, "conf: line %d: expected '=' after '%s'\n",
             lexer->line, fields[field].key);
      return -EINVAL;
   }

   rc = CONF_LexerNext(lexer);
   if (rc < 0)
      return conf_lex_error(lexer, rc);

   if (lexer->type != CONF_T_WORD && lexer->type != CONF_T_STRING)
   {
      logfmt(LOG_ERROR, "conf: line %d: expected value for '%s'\n",
             lexer->line, fields[field].key);
      return -EINVAL;
   }

   if (conf_apply_field(&fields[field], lexer->text, base) != SUCCESS)
   {
      logfmt(LOG_ERROR, "conf: line %d: invalid value for '%s'\n",
             lexer->line, fields[field].key);
      return -EINVAL;
   }

   *seen |= (1u << field);

   rc = CONF_LexerNext(lexer);
   if (rc < 0)
      return conf_lex_error(lexer, rc);

   if (lexer->type != CONF_T_NEWLINE && lexer->type != CONF_T_EOF)
   {
      logfmt(LOG_ERROR, "conf: line %d: unexpected token after '%s'\n",
             lexer->line, fields[field].key);
      return -EINVAL;
   }

   return SUCCESS;
}

_DL_FORCE_EXCLUDE
int CONF_ParseFile(const char *path, CONF_GlobalBoot *global)
{
   CONF_MenuConfig menu = {0};
   CONF_Lexer lexer;
   int section = SECTION_NONE;
   CONF_BootProfile *profile = NULL;
   uint32_t menu_seen = 0;
   uint32_t profile_seen = 0;
   int file_len = 0;
   int fd;
   int rc;

   if (!path || !global)
      return -EINVAL;

   fd = g_DlCallbackOps->Open(path);
   if (fd < 0)
   {
      logfmt(LOG_ERROR, "conf: cannot open %s (%d)\n", path, fd);
      return fd;
   }

   rc = conf_read_file(fd, s_ConfFileBuf, (int)sizeof(s_ConfFileBuf),
                       &file_len);
   g_DlCallbackOps->Close(fd);

   if (rc != SUCCESS)
      return rc;

   if (CONF_LexerInit(&lexer, s_ConfFileBuf, file_len) != SUCCESS)
      return -EINVAL;

   for (;;)
   {
      rc = CONF_LexerNext(&lexer);
      if (rc < 0)
         return conf_lex_error(&lexer, rc);

      if (lexer.type == CONF_T_EOF)
         break;

      if (lexer.type == CONF_T_NEWLINE)
         continue;

      if (lexer.type == CONF_T_LBRACKET)
      {
         rc = conf_parse_section(&lexer, global, &section, &profile);
         if (rc != SUCCESS)
            return rc;

         if (section == SECTION_PROFILE)
            profile_seen = 0;

         continue;
      }

      if (lexer.type != CONF_T_WORD)
      {
         logfmt(LOG_ERROR, "conf: line %d: expected key\n", lexer.line);
         return -EINVAL;
      }

      rc = conf_parse_assignment(&lexer, section, profile, &menu, &menu_seen,
                                 &profile_seen);
      if (rc != SUCCESS)
         return rc;
   }

   global->timeout = menu.timeout;

   if (menu.default_name[0] != '\0')
   {
      int index = conf_find_profile(global, menu.default_name);

      if (index < 0)
      {
         logfmt(LOG_ERROR, "conf: default profile '%s' not found\n",
                menu.default_name);
         return -EINVAL;
      }

      global->default_profile = index;
   }

   return SUCCESS;
}
