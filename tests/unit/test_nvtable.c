#include "nvtable.h"
#include "apphook.h"
#include "logmsg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* NVRegistry */
/* testcases:
 *
 *   - static names will have the lowest numbered handles, starting with 1
 *   - registering the same name will always map to the same handle
 *   - adding an alias and looking up an NV pair by alias should return the same handle
 *   - NV pairs cannot have the zero-length string as a name
 *   - NV pairs cannot have names longer than 255 characters
 *   - no more than 65535 NV pairs are supported
 *   -
 */

#define TEST_ASSERT(cond) \
  do                                                                    \
    {                                                                   \
      if (!(cond))                                                      \
        {                                                               \
          fprintf(stderr, "Assertion %s failed at line %d", #cond, __LINE__); \
          exit(1);                                                      \
        }                                                               \
    }                                                                   \
  while (0)

void
test_nv_registry()
{
  NVRegistry *reg;
  gint i, j;
  NVHandle handle, prev_handle;
  const gchar *name;
  gssize len;
  const gchar *builtins[] = { "BUILTIN1", "BUILTIN2", "BUILTIN3", NULL };

  reg = nv_registry_new(builtins);

  for (i = 0; builtins[i]; i++)
    {
      handle = nv_registry_alloc_handle(reg, builtins[i]);
      TEST_ASSERT(handle == (i+1));
      name = nv_registry_get_handle_name(reg, handle, &len);
      TEST_ASSERT(strcmp(name, builtins[i]) == 0);
      TEST_ASSERT(strlen(name) == len);
    }

  for (i = 4; i < 65536; i++)
    {
      gchar dyn_name[16];

      g_snprintf(dyn_name, sizeof(dyn_name), "DYN%05d", i);

      /* try to look up the same name multiple times */
      prev_handle = 0;
      for (j = 0; j < 4; j++)
        {
          handle = nv_registry_alloc_handle(reg, dyn_name);
          TEST_ASSERT(prev_handle == 0 || (handle == prev_handle));
          prev_handle = handle;
        }
      name = nv_registry_get_handle_name(reg, handle, &len);
      TEST_ASSERT(strcmp(name, dyn_name) == 0);
      TEST_ASSERT(strlen(name) == len);

      g_snprintf(dyn_name, sizeof(dyn_name), "ALIAS%05d", i);
      nv_registry_add_alias(reg, handle, dyn_name);
      handle = nv_registry_alloc_handle(reg, dyn_name);
      TEST_ASSERT(handle == prev_handle);
    }

  for (i = 65534; i >= 4; i--)
    {
      gchar dyn_name[16];

      g_snprintf(dyn_name, sizeof(dyn_name), "DYN%05d", i);

      /* try to look up the same name multiple times */
      prev_handle = 0;
      for (j = 0; j < 4; j++)
        {
          handle = nv_registry_alloc_handle(reg, dyn_name);
          TEST_ASSERT(prev_handle == 0 || (handle == prev_handle));
          prev_handle = handle;
        }
      name = nv_registry_get_handle_name(reg, handle, &len);
      TEST_ASSERT(strcmp(name, dyn_name) == 0);
      TEST_ASSERT(strlen(name) == len);
    }

  fprintf(stderr, "One error message about too many values is to be expected\n");
  handle = nv_registry_alloc_handle(reg, "too-many-values");
  TEST_ASSERT(handle == 0);

  nv_registry_free(reg);
}

/*
 * NVTable:
 *
 *
 */

#define TEST_NVTABLE_ASSERT(tab, handle, expected_value, expected_length) \
  do  {                                                                 \
    const gchar *__value;                                               \
    gssize __len;                                                       \
                                                                        \
    __value = nv_table_get_value(tab, handle, &__len);                  \
    if (__len != expected_length || strncmp(expected_value, __value, expected_length) != 0) \
      {                                                                 \
        fprintf(stderr, "NVTable value mismatch, value=%.*s, expected=%.*s\n", \
                (gint) __len, __value, (gint) expected_length, expected_value); \
        TEST_ASSERT(__len == expected_length);                          \
        TEST_ASSERT(strncmp(expected_value, __value, expected_length) == 0); \
      }                                                                 \
  } while (0)

/*
 *  - NVTable direct values
 *    - set/get static NV entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite indirect NV entry: not possible as static entries cannot be indirect
 *
 *    - set/get dynamic NV entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite indirect NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 */

#define STATIC_VALUES 16
#define STATIC_HANDLE 1
#define STATIC_NAME   "VAL1"
#define DYN_HANDLE 17
#define DYN_NAME "VAL17"

void
test_nvtable_direct()
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;
  guint16 used;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = STATIC_HANDLE;
  do
    {
      g_snprintf(name, sizeof(name), "VAL%d", handle);
      fprintf(stderr, "Testing direct values, name: %s, handle: %d\n", name, handle);

      /*************************************************************/
      /* new NV entry */
      /*************************************************************/

      /* one that fits */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      TEST_ASSERT(success == TRUE);
      TEST_NVTABLE_ASSERT(tab, handle, value, 128);
      nv_table_unref(tab);

      /* one that is too large */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 512, NULL);
      TEST_ASSERT(success == FALSE);
      nv_table_unref(tab);

      /*************************************************************/
      /* overwrite NV entry */
      /*************************************************************/

      /* one that fits, but realloced size wouldn't fit */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      TEST_ASSERT(success == TRUE);
      used = tab->used;

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      TEST_ASSERT(success == TRUE);
      TEST_ASSERT(tab->used == used);
      TEST_NVTABLE_ASSERT(tab, handle, value, 64);
      nv_table_unref(tab);

      /* one that is too large for the given entry, but still fits in the NVTable */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      TEST_ASSERT(success == TRUE);
      used = tab->used;

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
      TEST_ASSERT(success == TRUE);
      TEST_ASSERT(tab->used > used);
      TEST_NVTABLE_ASSERT(tab, handle, value, 128);
      nv_table_unref(tab);

      /* one that is too large for the given entry, and also for the NVTable */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
      success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
      TEST_ASSERT(success == TRUE);

      success = nv_table_add_value(tab, handle, name, strlen(name), value, 512, NULL);
      TEST_ASSERT(success == FALSE);
      TEST_NVTABLE_ASSERT(tab, handle, value, 64);
      nv_table_unref(tab);

      /*************************************************************/
      /* overwrite indirect entry */
      /*************************************************************/

      if (handle > STATIC_VALUES)
        {
          /* we can only test this with dynamic entries */

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
          TEST_ASSERT(success == TRUE);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
          TEST_ASSERT(success == TRUE);
          used = tab->used;

          /* store a direct entry over the indirect one */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
          TEST_ASSERT(success == TRUE);
          TEST_ASSERT(tab->used == used);
          TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
          TEST_NVTABLE_ASSERT(tab, handle, value, 1);

          nv_table_unref(tab);

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 64, NULL);
          TEST_ASSERT(success == TRUE);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 63, NULL);
          TEST_ASSERT(success == TRUE);

          used = tab->used;

          /* store a direct entry over the indirect one, we don't fit in the allocated space */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 128, NULL);
          TEST_ASSERT(success == TRUE);
          TEST_ASSERT(tab->used > used);
          TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 64);
          TEST_NVTABLE_ASSERT(tab, handle, value, 128);

          nv_table_unref(tab);

          /* setup code: add static and a dynamic-indirect entry */
          tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
          success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 64, NULL);
          TEST_ASSERT(success == TRUE);
          success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 62, NULL);
          TEST_ASSERT(success == TRUE);
          used = tab->used;

          /* store a direct entry over the indirect one, we don't fit in the allocated space */
          success = nv_table_add_value(tab, handle, name, strlen(name), value, 256, NULL);
          TEST_ASSERT(success == FALSE);
          TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 64);
          TEST_NVTABLE_ASSERT(tab, handle, value + 1, 62);

          nv_table_unref(tab);

        }

      handle += STATIC_VALUES;
    }
  while (handle < 2 * STATIC_VALUES);
}

/*
 *  - indirect values
 *    - indirect static values are not possible
 *    - set/get dynamic NV entries that refer to direct entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite indirect NV entry
 *        - value always fits to a current NV entry, no other cases
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *    - set/get dynamic NV entries that refer to indirect entry, they become direct entries
 *      - new NV entry
 *        - entries that fit into the current NVTable
 *        - entries that do not fit into the current NVTable
 *      - overwrite indirect NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *      - overwrite direct NV entry
 *        - value that fits into the current entry
 *        - value that doesn't fit into the current entry, but fits into NVTable
 *        - value that doesn't fit into the current entry and neither to NVTable
 *    - set/get dynamic NV entries that refer to a non-existant entry
 *        -
 */
void
test_nvtable_indirect()
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;
  guint16 used;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = DYN_HANDLE+1;
  g_snprintf(name, sizeof(name), "VAL%d", handle);
  fprintf(stderr, "Testing indirect values, name: %s, handle: %d\n", name, handle);

  /*************************************************************/
  /*************************************************************/
  /* indirect entries that refer to direct entries */
  /*************************************************************/
  /*************************************************************/

  /*************************************************************/
  /* new NV entry */
  /*************************************************************/

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /* one that is too large */

  /* NOTE: the sizing of the NVTable can easily be broken, it is sized
     to make it possible to store one direct entry */

  tab = nv_table_new(STATIC_VALUES, 0, 138+3);  // direct: +3
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == FALSE);

  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite NV entry */
  /*************************************************************/

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 62, NULL);

  TEST_ASSERT(success == TRUE);
  TEST_ASSERT(used == tab->used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 62);
  nv_table_unref(tab);


  /*************************************************************/
  /* overwrite direct entry */
  /*************************************************************/

  /* the new entry fits to the space allocated to the old */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  TEST_ASSERT(tab->used == used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);

  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  TEST_ASSERT(tab->used > used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);

  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old and neither to the NVTable */

  /* setup code: add static and a dynamic-direct entry */
  tab = nv_table_new(STATIC_VALUES, 1, 154+3+4);  // direct: +3, indirect: +4
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 1, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == FALSE);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value, 1);

  nv_table_unref(tab);

  /*************************************************************/
  /*************************************************************/
  /* indirect entries that refer to indirect entries */
  /*************************************************************/
  /*************************************************************/


  /*************************************************************/
  /* new NV entry */
  /*************************************************************/

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 122, NULL);
  TEST_ASSERT(success == TRUE);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 2, 122);
  nv_table_unref(tab);

  /* one that is too large */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 122, NULL);
  TEST_ASSERT(success == FALSE);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, "", 0);
  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite indirect NV entry */
  /*************************************************************/


  /* we fit to the space of the old */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 1, NULL);
  TEST_ASSERT(success == TRUE);

  TEST_ASSERT(tab->used == used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 2, 1);
  nv_table_unref(tab);

  /* the new entry will not fit to the space allocated to the old */

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 16, NULL);
  TEST_ASSERT(success == TRUE);

  TEST_ASSERT(tab->used > used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 2, 16);
  nv_table_unref(tab);

  /* one that is too large */

  tab = nv_table_new(STATIC_VALUES, 4, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 124, NULL);
  TEST_ASSERT(success == FALSE);

  TEST_ASSERT(tab->used == used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /*************************************************************/
  /* overwrite direct NV entry */
  /*************************************************************/


  /* we fit to the space of the old */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 64, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 16, NULL);
  TEST_ASSERT(success == TRUE);

  TEST_ASSERT(tab->used == used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 2, 16);
  nv_table_unref(tab);


  /* the new entry will not fit to the space allocated to the old */

  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 16, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 32, NULL);
  TEST_ASSERT(success == TRUE);

  TEST_ASSERT(tab->used > used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value + 2, 32);
  nv_table_unref(tab);

  /* one that is too large */

  tab = nv_table_new(STATIC_VALUES, 4, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value_indirect(tab, DYN_HANDLE, DYN_NAME, strlen(DYN_NAME), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, handle, name, strlen(name), value, 16, NULL);
  TEST_ASSERT(success == TRUE);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), DYN_HANDLE, 0, 1, 124, NULL);
  TEST_ASSERT(success == FALSE);

  TEST_ASSERT(tab->used == used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, DYN_HANDLE, value + 1, 126);
  TEST_NVTABLE_ASSERT(tab, handle, value, 16);
  nv_table_unref(tab);

  /*************************************************************/
  /* indirect that refers to non-existant entry */
  /*************************************************************/
  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  used = tab->used;

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  TEST_ASSERT(used == tab->used);
  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, "", 0);
  TEST_NVTABLE_ASSERT(tab, handle, "", 0);
  nv_table_unref(tab);
}

/*
 *
 *  - other corner cases
 *    - set zero length value in direct value
 *      - add a zero-length value to non-existing entry
 *      - add a zero-length value to an existing entry that is not zero-length
 *      - add a zero-length value to an existing entry that is zero-length
 *    - set zero length value in an indirect value
 *
 * - change an entry that is referenced by other entries
 */
void
test_nvtable_others(void)
{
  NVTable *tab;
  NVHandle handle;
  gchar value[1024], name[16];
  gboolean success;
  gint i;

  for (i = 0; i < sizeof(value); i++)
    value[i] = 'A' + (i % 26);

  handle = DYN_HANDLE+1;
  g_snprintf(name, sizeof(name), "VAL%d", handle);
  fprintf(stderr, "Testing other cases, name: %s, handle: %d\n", name, handle);

  /* one that fits */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 256);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value + 32, 32, NULL);
  TEST_ASSERT(success == TRUE);

  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value + 32, 32);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);
  nv_table_unref(tab);

  /* one that doesn't fit */
  tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 192);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value, 128, NULL);
  TEST_ASSERT(success == TRUE);

  success = nv_table_add_value_indirect(tab, handle, name, strlen(name), STATIC_HANDLE, 0, 1, 126, NULL);
  TEST_ASSERT(success == TRUE);
  success = nv_table_add_value(tab, STATIC_HANDLE, STATIC_NAME, 4, value + 32, 32, NULL);
  TEST_ASSERT(success == FALSE);

  TEST_NVTABLE_ASSERT(tab, STATIC_HANDLE, value, 128);
  TEST_NVTABLE_ASSERT(tab, handle, value + 1, 126);
  nv_table_unref(tab);
}

void
test_nvtable_lookup()
{
  NVTable *tab;
  NVHandle handle;
  gchar name[16];
  gboolean success;
  gint i;
  NVHandle handles[100];
  gint x;

  srand(time(NULL));
  for (x = 0; x < 100; x++)
    {
      /* test dynamic lookup */
      tab = nv_table_new(STATIC_VALUES, STATIC_VALUES, 4096);

      for (i = 0; i < 100; i++)
        {
          do
            {
              handle = rand() & 0x8FFF;
            }
          while (handle == 0);
          g_snprintf(name, sizeof(name), "VAL%d", handle);
          success = nv_table_add_value(tab, handle, name, strlen(name), name, strlen(name), NULL);
          TEST_ASSERT(success == TRUE);
          handles[i] = handle;
        }

      for (i = 99; i >= 0; i--)
        {
          handle = handles[i];
          g_snprintf(name, sizeof(name), "VAL%d", handle);
          TEST_NVTABLE_ASSERT(tab, handles[i], name, strlen(name));
        }
      nv_table_unref(tab);
    }
}

#define STATIC_VALUE "Test Static Value"

void
test_nvtable_realloc_common(NVTable *payload)
{
  NVTable *new_payload = NULL;
  guint16 old_size;
  const gchar *value;
  gssize value_length;
  nv_table_add_value(payload, STATIC_HANDLE, STATIC_NAME, strlen(STATIC_NAME), STATIC_VALUE, strlen(STATIC_VALUE), NULL);
  old_size = payload->size;
  new_payload = nv_table_realloc(payload, &new_payload);
  TEST_ASSERT(old_size < new_payload->size);
  fprintf(stderr, "New size: %d\n", new_payload->size);
  TEST_ASSERT(new_payload->size == NV_TABLE_MAX_BYTES);
  value = __nv_table_get_value(new_payload, STATIC_HANDLE, STATIC_VALUES, &value_length);
  TEST_ASSERT(value_length == strlen(STATIC_VALUE));
  TEST_ASSERT(strcmp(value, STATIC_VALUE) == 0);
  payload = new_payload;
  new_payload = nv_table_realloc(new_payload, &new_payload);
  TEST_ASSERT(new_payload == NULL);
  nv_table_unref(payload);

}

void
test_nvtable_realloc_with_one_ref()
{
  NVTable *payload = nv_table_new(STATIC_VALUES, 0, NV_TABLE_MAX_BYTES / 2 + 1);
  test_nvtable_realloc_common(payload);
}

void
test_nvtable_realloc_with_more_ref()
{
  NVTable *payload = nv_table_new(STATIC_VALUES, 0, NV_TABLE_MAX_BYTES / 2 + 1);
  nv_table_ref(payload);
  test_nvtable_realloc_common(payload);
  nv_table_unref(payload);
}

void
test_nvtable(void)
{
  test_nvtable_direct();
  test_nvtable_indirect();
  test_nvtable_others();
  test_nvtable_lookup();
  test_nvtable_realloc_with_one_ref();
  test_nvtable_realloc_with_more_ref();
}

int
main(int argc, char *argv[])
{
  app_startup();
  test_nv_registry();
  test_nvtable();
  app_shutdown();
  return 0;
}
