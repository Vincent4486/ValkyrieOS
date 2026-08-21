// SPDX-License-Identifier: BSD-3-Clause

#include <stddef.h>
#include <stdint.h>

#include <dl/callback.h>
#include <logging.h>
#include <status.h>

#include "conf.h"

#define CONF_LINE_MAX (CONF_MAX_ARGS_LEN + 32)
#define CONF_READ_CHUNK 512

#define SECTION_NONE 0
#define SECTION_MENU 1
#define SECTION_PROFILE 2

#define logfmt g_DlCallbackOps->logfmt

typedef struct
{
   int fd;
   char buf[CONF_READ_CHUNK];
   int buf_len;
   int buf_pos;
} ConfReader;

static CONF_BootProfile s_BootProfiles[CONF_MAX_PROFILES] = {0};
static char s_DefaultProfile[CONF_MAX_NAME_LEN] = {0};
static int s_ProfileCount = 0;
static int s_Timeout = 0;

static int conf_strncmp(const char *a, const char *b, size_t n)
{
   size_t i;

   for (i = 0; i < n; i++)
   {
      if (a[i] != b[i])
         return (unsigned char)a[i] - (unsigned char)b[i];
      if (a[i] == '\0')
         return 0;
   }

   return 0;
}

static void conf_copy_strn(char *dst, size_t dst_size, const char *src,
                           size_t max_len)
{
   size_t len = 0;
   size_t i;

   while (len < max_len && src[len] != '\0')
      len++;

   if (len >= dst_size)
      len = dst_size - 1;

   for (i = 0; i < len; i++)
      dst[i] = src[i];

   dst[len] = '\0';
}

static void conf_copy_str(char *dst, size_t dst_size, const char *src)
{
   conf_copy_strn(dst, dst_size, src, (size_t)-1);
}

static int conf_atoi(const char *s)
{
   int value = 0;

   while (*s >= '0' && *s <= '9')
   {
      value = value * 10 + (*s - '0');
      s++;
   }

   return value;
}

/* Returns line length (0 for blank lines), -1 on EOF, -errno on error. */
static int conf_read_line(ConfReader *reader, char *line, int line_cap)
{
   int line_len = 0;

   for (;;)
   {
      if (reader->buf_pos >= reader->buf_len)
      {
         int rc = g_DlCallbackOps->Read(reader->fd, reader->buf,
                                        (int)sizeof(reader->buf));
         if (rc <= 0)
         {
            if (rc < 0)
               return rc;
            if (line_len == 0)
               return -1; /* EOF */
            line[line_len] = '\0';
            return line_len;
         }
         reader->buf_len = rc;
         reader->buf_pos = 0;
      }

      char c = reader->buf[reader->buf_pos++];

      if (c == '\n')
      {
         line[line_len] = '\0';
         return line_len;
      }
      if (line_len + 1 >= line_cap)
         return -EINVAL;

      line[line_len++] = c;
   }
}

static int parse_section_header(const char *line, char *name, int name_cap)
{
   const char *end = line;
   int len;

   while (*end != '\0' && *end != ']')
      end++;

   if (*end != ']' || end == line + 1)
      return SECTION_NONE;

   len = (int)(end - line) - 1;

   if (len == 4 && conf_strncmp(line + 1, "menu", 4) == 0)
      return SECTION_MENU;

   if (len > 8 && conf_strncmp(line + 1, "profile.", 8) == 0)
   {
      conf_copy_strn(name, name_cap, line + 9, (size_t)(len - 8));
      if (name[0] != '\0')
         return SECTION_PROFILE;
   }

   return SECTION_NONE;
}

static void parse_menu_line(const char *line)
{
   if (conf_strncmp(line, "default=", 8) == 0)
   {
      conf_copy_str(s_DefaultProfile, sizeof(s_DefaultProfile), line + 8);
   }
   else if (conf_strncmp(line, "timeout=", 8) == 0)
   {
      s_Timeout = conf_atoi(line + 8);
   }
}

static void parse_config_line(const char *line, CONF_BootProfile *profile)
{
   if (conf_strncmp(line, "title=", 6) == 0)
   {
      conf_copy_str(profile->title, sizeof(profile->title), line + 6);
   }
   else if (conf_strncmp(line, "root=", 5) == 0)
   {
      conf_copy_str(profile->root_label, sizeof(profile->root_label), line + 5);
   }
   else if (conf_strncmp(line, "path=", 5) == 0)
   {
      conf_copy_str(profile->path, sizeof(profile->path), line + 5);
   }
   else if (conf_strncmp(line, "args=", 5) == 0)
   {
      conf_copy_str(profile->args, sizeof(profile->args), line + 5);
   }
}

int CONF_ParseConfigFile(const char *path)
{
   ConfReader reader;
   char line[CONF_LINE_MAX];
   char name[CONF_MAX_NAME_LEN];
   int section = SECTION_NONE;
   CONF_BootProfile *profile = NULL;
   int fd;
   int rc;
   int result = SUCCESS;

   if (!path)
      return -EINVAL;

   if (!g_DlCallbackOps || !g_DlCallbackOps->Open || !g_DlCallbackOps->Read ||
       !g_DlCallbackOps->Close)
      return -ENODEV;

   fd = g_DlCallbackOps->Open(path);
   if (fd < 0)
   {
      logfmt(LOG_ERROR, "conf: cannot open %s (%d)\n", path, fd);
      return -ENOENT;
   }

   s_ProfileCount = 0;
   s_DefaultProfile[0] = '\0';
   s_Timeout = 0;

   reader.fd = fd;
   reader.buf_len = 0;
   reader.buf_pos = 0;

   while ((rc = conf_read_line(&reader, line, (int)sizeof(line))) != -1)
   {
      int len = rc;

      if (rc < -1)
      {
         result = rc;
         break;
      }

      /* Strip trailing carriage return from CRLF files */
      while (len > 0 && line[len - 1] == '\r')
         line[--len] = '\0';

      if (len == 0 || line[0] == '#')
         continue;

      if (line[0] == '[')
      {
         int type = parse_section_header(line, name, sizeof(name));

         if (type == SECTION_MENU)
         {
            section = SECTION_MENU;
            profile = NULL;
         }
         else if (type == SECTION_PROFILE)
         {
            if (s_ProfileCount >= CONF_MAX_PROFILES)
            {
               logfmt(LOG_ERROR, "conf: too many profiles, max is %d\n",
                      CONF_MAX_PROFILES);
               result = -EINVAL;
               break;
            }
            profile = &s_BootProfiles[s_ProfileCount];
            conf_copy_str(profile->name, sizeof(profile->name), name);
            s_ProfileCount++;
            section = SECTION_PROFILE;
         }
         else
         {
            section = SECTION_NONE;
            profile = NULL;
         }
         continue;
      }

      if (section == SECTION_PROFILE && profile)
      {
         parse_config_line(line, profile);
      }
      else if (section == SECTION_MENU)
      {
         parse_menu_line(line);
      }
   }

   g_DlCallbackOps->Close(fd);

   if (result != SUCCESS)
   {
      logfmt(LOG_ERROR, "conf: failed to parse %s (%d)\n", path, result);
      return result;
   }

   if (s_ProfileCount == 0)
   {
      logfmt(LOG_ERROR, "conf: no profiles defined in %s\n", path);
      return -EINVAL;
   }

   return SUCCESS;
}

CONF_BootProfile *CONF_GetProfile(int profile_id)
{
   if (profile_id < 0 || profile_id >= s_ProfileCount)
      return NULL;

   return &s_BootProfiles[profile_id];
}

int CONF_GetProfileCount(void)
{
   return s_ProfileCount;
}

const char *CONF_GetDefaultProfile(void)
{
   return s_DefaultProfile;
}

int CONF_GetTimeout(void)
{
   return s_Timeout;
}
