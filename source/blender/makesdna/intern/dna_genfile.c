/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * DNA handling
 */

/** \file
 * \ingroup DNA
 *
 * Lowest-level functions for decoding the parts of a saved .blend
 * file, including interpretation of its SDNA block and conversion of
 * contents of other parts according to the differences between that
 * SDNA and the SDNA of the current (running) version of Blender.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"  // for MEM_freeN MEM_mallocN MEM_callocN

#include "BLI_endian_switch.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"  // for SDNA ;-)

/**
 * \section dna_genfile Overview
 *
 * - please note: no builtin security to detect input of double structs
 * - if you want a struct not to be in DNA file: add two hash marks above it `(#<enter>#<enter>)`.
 *
 * Structure DNA data is added to each blender file and to each executable, this to detect
 * in .blend files new variables in structs, changed array sizes, etc. It's also used for
 * converting endian and pointer size (32-64 bits)
 * As an extra, Python uses a call to detect run-time the contents of a blender struct.
 *
 * Create a structDNA: only needed when one of the input include (.h) files change.
 * File Syntax:
 * \code{.unparsed}
 *     SDNA (4 bytes) (magic number)
 *     NAME (4 bytes)
 *     <nr> (4 bytes) amount of names (int)
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TYPE (4 bytes)
 *     <nr> amount of types (int)
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TLEN (4 bytes)
 *     <len> (short) the lengths of types
 *     <len>
 *     ...
 *     ...
 *     STRC (4 bytes)
 *     <nr> amount of structs (int)
 *     <typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
 * \endcode
 *
 * **Remember to read/write integer and short aligned!**
 *
 * While writing a file, the names of a struct is indicated with a type number,
 * to be found with: ``type = DNA_struct_find_nr(SDNA *, const char *)``
 * The value of ``type`` corresponds with the index within the structs array
 *
 * For the moment: the complete DNA file is included in a .blend file. For
 * the future we can think of smarter methods, like only included the used
 * structs. Only needed to keep a file short though...
 *
 * ALLOWED AND TESTED CHANGES IN STRUCTS:
 *  - Type change (a char to float will be divided by 255).
 *  - Location within a struct (everything can be randomly mixed up).
 *  - Struct within struct (within struct etc), this is recursive.
 *  - Adding new elements, will be default initialized zero.
 *  - Removing elements.
 *  - Change of array sizes.
 *  - Change of a pointer type: when the name doesn't change the contents is copied.
 *
 * NOT YET:
 *  - array (``vec[3]``) to float struct (``vec3f``).
 *
 * DONE:
 *  - Endian compatibility.
 *  - Pointer conversion (32-64 bits).
 *
 * IMPORTANT:
 *  - Do not use #defines in structs for array lengths, this cannot be read by the dna functions.
 *  - Do not use uint, but unsigned int instead, ushort and ulong are allowed.
 *  - Only use a long in Blender if you want this to be the size of a pointer. so it is
 *    32 bits or 64 bits, dependent at the cpu architecture.
 *  - Chars are always unsigned
 *  - Alignment of variables has to be done in such a way, that any system does
 *    not create 'padding' (gaps) in structures. So make sure that:
 *    - short: 2 aligned.
 *    - int: 4 aligned.
 *    - float: 4 aligned.
 *    - double: 8 aligned.
 *    - long: 8 aligned.
 *    - int64: 8 aligned.
 *    - struct: 8 aligned.
 *  - the sdna functions have several error prints builtin,
 *    always check blender running from a console.
 */

#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

/* ************************* DIV ********************** */

void DNA_sdna_free(SDNA *sdna)
{
  if (sdna->data_alloc) {
    MEM_freeN((void *)sdna->data);
  }

  MEM_freeN((void *)sdna->names);
  MEM_freeN((void *)sdna->names_array_len);
  MEM_freeN((void *)sdna->types);
  MEM_freeN(sdna->structs);

#ifdef WITH_DNA_GHASH
  if (sdna->structs_map) {
    BLI_ghash_free(sdna->structs_map, NULL, NULL);
  }
#endif

  if (sdna->mem_arena) {
    BLI_memarena_free(sdna->mem_arena);
  }

  MEM_SAFE_FREE(sdna->alias.names);
  MEM_SAFE_FREE(sdna->alias.types);
#ifdef WITH_DNA_GHASH
  if (sdna->alias.structs_map) {
    BLI_ghash_free(sdna->alias.structs_map, NULL, NULL);
  }
#endif

  MEM_freeN(sdna);
}

/**
 * Return true if the name indicates a pointer of some kind.
 */
static bool ispointer(const char *name)
{
  /* check if pointer or function pointer */
  return (name[0] == '*' || (name[0] == '(' && name[1] == '*'));
}

/**
 * Returns the size of struct fields of the specified type and name.
 *
 * \param type: Index into sdna->types/types_size
 * \param name: Index into sdna->names,
 * needed to extract possible pointer/array information.
 */
int DNA_elem_size_nr(const SDNA *sdna, short type, short name)
{
  const char *cp = sdna->names[name];
  int len = 0;

  /* is it a pointer or function pointer? */
  if (ispointer(cp)) {
    /* has the name an extra length? (array) */
    len = sdna->pointer_size * sdna->names_array_len[name];
  }
  else if (sdna->types_size[type]) {
    /* has the name an extra length? (array) */
    len = (int)sdna->types_size[type] * sdna->names_array_len[name];
  }

  return len;
}

#if 0
static void printstruct(SDNA *sdna, short strnr)
{
  /* is for debug */

  SDNA_Struct *struct_info = sdna->structs[strnr];
  printf("struct %s\n", sdna->types[struct_info->type]);

  for (int b = 0; b < struct_info->members_len; b++) {
    SDNA_StructMember *struct_member = &struct_info->members[b];
    printf("   %s %s\n",
           sdna->types[struct_member->type],
           sdna->names[struct_member->name]);
  }
}
#endif

/**
 * Returns the index of the struct info for the struct with the specified name.
 */
static int dna_struct_find_nr_ex_impl(
    /* From SDNA struct. */
    const char **types,
    const int UNUSED(types_len),
    SDNA_Struct **const structs,
    const int structs_len,
#ifdef WITH_DNA_GHASH
    GHash *structs_map,
#endif
    /* Regular args. */
    const char *str,
    unsigned int *index_last)
{
  if (*index_last < structs_len) {
    const SDNA_Struct *struct_info = structs[*index_last];
    if (STREQ(types[struct_info->type], str)) {
      return *index_last;
    }
  }

#ifdef WITH_DNA_GHASH
  {
    void **index_p = BLI_ghash_lookup_p(structs_map, str);
    if (index_p) {
      const int index = POINTER_AS_INT(*index_p);
      *index_last = index;
      return index;
    }
  }
#else
  {
    for (int index = 0; index < structs_len; index++) {
      const SDNA_Struct *struct_info = structs[index];
      if (STREQ(types[struct_info->type], str)) {
        *index_last = index;
        return index;
      }
    }
  }
#endif
  return -1;
}

/**
 * Returns the index of the struct info for the struct with the specified name.
 */
int DNA_struct_find_nr_ex(const SDNA *sdna, const char *str, unsigned int *index_last)
{
  return dna_struct_find_nr_ex_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->types_len,
      sdna->structs,
      sdna->structs_len,
#ifdef WITH_DNA_GHASH
      sdna->structs_map,
#endif
      /* Regular args. */
      str,
      index_last);
}

/** \note requires #DNA_sdna_alias_data_ensure_structs_map to be called. */
int DNA_struct_alias_find_nr_ex(const SDNA *sdna, const char *str, unsigned int *index_last)
{
#ifdef WITH_DNA_GHASH
  BLI_assert(sdna->alias.structs_map != NULL);
#endif
  return dna_struct_find_nr_ex_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->types_len,
      sdna->structs,
      sdna->structs_len,
#ifdef WITH_DNA_GHASH
      sdna->alias.structs_map,
#endif
      /* Regular args. */
      str,
      index_last);
}

int DNA_struct_find_nr(const SDNA *sdna, const char *str)
{
  unsigned int index_last_dummy = UINT_MAX;
  return DNA_struct_find_nr_ex(sdna, str, &index_last_dummy);
}

/** \note requires #DNA_sdna_alias_data_ensure_structs_map to be called. */
int DNA_struct_alias_find_nr(const SDNA *sdna, const char *str)
{
  unsigned int index_last_dummy = UINT_MAX;
  return DNA_struct_alias_find_nr_ex(sdna, str, &index_last_dummy);
}

/* ************************* END DIV ********************** */

/* ************************* READ DNA ********************** */

BLI_INLINE const char *pad_up_4(const char *ptr)
{
  return (const char *)((((uintptr_t)ptr) + 3) & ~3);
}

/**
 * In sdna->data the data, now we convert that to something understandable
 */
static bool init_structDNA(SDNA *sdna, bool do_endian_swap, const char **r_error_message)
{
  int gravity_fix = -1;

  int *data = (int *)sdna->data;

  /* Clear pointers in case of error. */
  sdna->names = NULL;
  sdna->types = NULL;
  sdna->structs = NULL;
#ifdef WITH_DNA_GHASH
  sdna->structs_map = NULL;
#endif
  sdna->mem_arena = NULL;

  /* Lazy initialize. */
  memset(&sdna->alias, 0, sizeof(sdna->alias));

  /* Struct DNA ('SDNA') */
  if (*data != MAKE_ID('S', 'D', 'N', 'A')) {
    *r_error_message = "SDNA error in SDNA file";
    return false;
  }

  const char *cp;

  data++;
  /* Names array ('NAME') */
  if (*data == MAKE_ID('N', 'A', 'M', 'E')) {
    data++;

    sdna->names_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->names_len);
    }
    sdna->names_len_alloc = sdna->names_len;

    data++;
    sdna->names = MEM_callocN(sizeof(void *) * sdna->names_len, "sdnanames");
  }
  else {
    *r_error_message = "NAME error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int nr = 0; nr < sdna->names_len; nr++) {
    sdna->names[nr] = cp;

    /* "float gravity [3]" was parsed wrong giving both "gravity" and
     * "[3]"  members. we rename "[3]", and later set the type of
     * "gravity" to "void" so the offsets work out correct */
    if (*cp == '[' && STREQ(cp, "[3]")) {
      if (nr && STREQ(sdna->names[nr - 1], "Cvi")) {
        sdna->names[nr] = "gravity[3]";
        gravity_fix = nr;
      }
    }
    while (*cp) {
      cp++;
    }
    cp++;
  }

  cp = pad_up_4(cp);

  /* Type names array ('TYPE') */
  data = (int *)cp;
  if (*data == MAKE_ID('T', 'Y', 'P', 'E')) {
    data++;

    sdna->types_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->types_len);
    }

    data++;
    sdna->types = MEM_callocN(sizeof(void *) * sdna->types_len, "sdnatypes");
  }
  else {
    *r_error_message = "TYPE error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int nr = 0; nr < sdna->types_len; nr++) {
    /* WARNING! See: DNA_struct_rename_legacy_hack_static_from_alias docs. */
    sdna->types[nr] = DNA_struct_rename_legacy_hack_static_from_alias(cp);
    while (*cp) {
      cp++;
    }
    cp++;
  }

  cp = pad_up_4(cp);

  /* Type lengths array ('TLEN') */
  data = (int *)cp;
  short *sp;
  if (*data == MAKE_ID('T', 'L', 'E', 'N')) {
    data++;
    sp = (short *)data;
    sdna->types_size = sp;

    if (do_endian_swap) {
      BLI_endian_switch_int16_array(sp, sdna->types_len);
    }

    sp += sdna->types_len;
  }
  else {
    *r_error_message = "TLEN error in SDNA file";
    return false;
  }
  /* prevent BUS error */
  if (sdna->types_len & 1) {
    sp++;
  }

  /* Struct array ('STRC') */
  data = (int *)sp;
  if (*data == MAKE_ID('S', 'T', 'R', 'C')) {
    data++;

    sdna->structs_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->structs_len);
    }

    data++;
    sdna->structs = MEM_callocN(sizeof(SDNA_Struct *) * sdna->structs_len, "sdnastrcs");
  }
  else {
    *r_error_message = "STRC error in SDNA file";
    return false;
  }

  sp = (short *)data;
  for (int nr = 0; nr < sdna->structs_len; nr++) {
    SDNA_Struct *struct_info = (SDNA_Struct *)sp;
    sdna->structs[nr] = struct_info;

    if (do_endian_swap) {
      BLI_endian_switch_int16(&struct_info->type);
      BLI_endian_switch_int16(&struct_info->members_len);

      for (short a = 0; a < struct_info->members_len; a++) {
        SDNA_StructMember *member = &struct_info->members[a];
        BLI_endian_switch_int16(&member->type);
        BLI_endian_switch_int16(&member->name);
      }
    }
    sp += 2 + (sizeof(SDNA_StructMember) / sizeof(short)) * struct_info->members_len;
  }

  {
    /* second part of gravity problem, setting "gravity" type to void */
    if (gravity_fix > -1) {
      for (int nr = 0; nr < sdna->structs_len; nr++) {
        sp = (short *)sdna->structs[nr];
        if (STREQ(sdna->types[sp[0]], "ClothSimSettings")) {
          sp[10] = SDNA_TYPE_VOID;
        }
      }
    }
  }

#ifdef WITH_DNA_GHASH
  {
    /* create a ghash lookup to speed up */
    sdna->structs_map = BLI_ghash_str_new_ex("init_structDNA gh", sdna->structs_len);

    for (intptr_t nr = 0; nr < sdna->structs_len; nr++) {
      SDNA_Struct *struct_info = sdna->structs[nr];
      BLI_ghash_insert(
          sdna->structs_map, (void *)sdna->types[struct_info->type], POINTER_FROM_INT(nr));
    }
  }
#endif

  /* Calculate 'sdna->pointer_size' */
  {
    const int nr = DNA_struct_find_nr(sdna, "ListBase");

    /* should never happen, only with corrupt file for example */
    if (UNLIKELY(nr == -1)) {
      *r_error_message = "ListBase struct error! Not found.";
      return false;
    }

    /* finally pointer_size: use struct ListBase to test it, never change the size of it! */
    SDNA_Struct *struct_info = sdna->structs[nr];
    /* weird; i have no memory of that... I think I used sizeof(void *) before... (ton) */

    sdna->pointer_size = sdna->types_size[struct_info->type] / 2;

    if (struct_info->members_len != 2 || (sdna->pointer_size != 4 && sdna->pointer_size != 8)) {
      *r_error_message = "ListBase struct error! Needs it to calculate pointerize.";
      /* well, at least sizeof(ListBase) is error proof! (ton) */
      return false;
    }
  }

  /* Cache name size. */
  {
    short *names_array_len = MEM_mallocN(sizeof(*names_array_len) * sdna->names_len, __func__);
    for (int i = 0; i < sdna->names_len; i++) {
      names_array_len[i] = DNA_elem_array_size(sdna->names[i]);
    }
    sdna->names_array_len = names_array_len;
  }

  return true;
}

/**
 * Constructs and returns a decoded SDNA structure from the given encoded SDNA data block.
 */
SDNA *DNA_sdna_from_data(const void *data,
                         const int data_len,
                         bool do_endian_swap,
                         bool data_alloc,
                         const char **r_error_message)
{
  SDNA *sdna = MEM_mallocN(sizeof(*sdna), "sdna");
  const char *error_message = NULL;

  sdna->data_len = data_len;
  if (data_alloc) {
    char *data_copy = MEM_mallocN(data_len, "sdna_data");
    memcpy(data_copy, data, data_len);
    sdna->data = data_copy;
  }
  else {
    sdna->data = data;
  }
  sdna->data_alloc = data_alloc;

  if (init_structDNA(sdna, do_endian_swap, &error_message)) {
    return sdna;
  }

  if (r_error_message == NULL) {
    fprintf(stderr, "Error decoding blend file SDNA: %s\n", error_message);
  }
  else {
    *r_error_message = error_message;
  }
  DNA_sdna_free(sdna);
  return NULL;
}

/**
 * Using a global is acceptable here,
 * the data is read-only and only changes between Blender versions.
 *
 * So it is safe to create once and reuse.
 */
static SDNA *g_sdna = NULL;

void DNA_sdna_current_init(void)
{
  g_sdna = DNA_sdna_from_data(DNAstr, DNAlen, false, false, NULL);
}

const struct SDNA *DNA_sdna_current_get(void)
{
  BLI_assert(g_sdna != NULL);
  return g_sdna;
}

void DNA_sdna_current_free(void)
{
  DNA_sdna_free(g_sdna);
  g_sdna = NULL;
}

/* ******************** END READ DNA ********************** */

/* ******************* HANDLE DNA ***************** */

static void make_compare_flags(const SDNA *oldsdna,
                               const SDNA *newsdna,
                               char *compare_flags,
                               const int old_struct_index)
{
  if (compare_flags[old_struct_index] != SDNA_CMP_UNKNOWN) {
    return;
  }

  SDNA_Struct *old_struct = oldsdna->structs[old_struct_index];
  const char *struct_name = oldsdna->types[old_struct->type];

  const int new_struct_index = DNA_struct_find_nr(newsdna, struct_name);

  if (new_struct_index == -1) {
    compare_flags[old_struct_index] = SDNA_CMP_REMOVED;
    return;
  }

  SDNA_Struct *new_struct = newsdna->structs[new_struct_index];
  if (old_struct->members_len != new_struct->members_len) {
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }
  if (oldsdna->types_size[old_struct->type] != newsdna->types_size[new_struct->type]) {
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }

  for (int member_index = 0; member_index < old_struct->members_len; member_index++) {
    SDNA_StructMember *old_member = &old_struct->members[member_index];
    SDNA_StructMember *new_member = &new_struct->members[member_index];

    const char *old_type_name = oldsdna->types[old_member->type];
    const char *new_type_name = newsdna->types[new_member->type];
    if (!STREQ(old_type_name, new_type_name)) {
      compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
      return;
    }

    const char *old_member_name = oldsdna->names[old_member->name];
    const char *new_member_name = newsdna->names[new_member->name];
    if (!STREQ(old_member_name, new_member_name)) {
      compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
      return;
    }

    if (ispointer(old_member_name)) {
      if (oldsdna->pointer_size != newsdna->pointer_size) {
        compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
        return;
      }
    }
    else {
      const int old_member_struct_index = DNA_struct_find_nr(oldsdna, old_type_name);
      if (old_member_struct_index >= 0) {
        make_compare_flags(oldsdna, newsdna, compare_flags, old_member_struct_index);
        if (compare_flags[old_member_struct_index] != SDNA_CMP_EQUAL) {
          compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
          return;
        }
      }
    }
  }

  compare_flags[old_struct_index] = SDNA_CMP_EQUAL;
}

/**
 * Constructs and returns an array of byte flags with one element for each struct in oldsdna,
 * indicating how it compares to newsdna:
 */
const char *DNA_struct_get_compareflags(const SDNA *oldsdna, const SDNA *newsdna)
{
  if (oldsdna->structs_len == 0) {
    printf("error: file without SDNA\n");
    return NULL;
  }

  char *compare_flags = MEM_mallocN(oldsdna->structs_len, "compare flags");
  memset(compare_flags, SDNA_CMP_UNKNOWN, oldsdna->structs_len);

  for (int a = 0; a < oldsdna->structs_len; a++) {
    make_compare_flags(oldsdna, newsdna, compare_flags, a);
    BLI_assert(compare_flags[a] != SDNA_CMP_UNKNOWN);
  }

  /* first struct in util.h is struct Link, this is skipped in compare_flags (als # 0).
   * was a bug, and this way dirty patched! Solve this later....
   */
  compare_flags[0] = SDNA_CMP_EQUAL;

#if 0
  for (int a = 0; a < oldsdna->structs_len; a++) {
    if (compare_flags[a] == SDNA_CMP_NOT_EQUAL) {
      SDNA_Struct *struct_info = oldsdna->structs[a];
      printf("changed: %s\n", oldsdna->types[struct_info->type]);
    }
  }
#endif

  return compare_flags;
}

/**
 * Converts the name of a primitive type to its enumeration code.
 */
static eSDNA_Type sdna_type_nr(const char *dna_type)
{
  if (STR_ELEM(dna_type, "char", "const char")) {
    return SDNA_TYPE_CHAR;
  }
  if (STR_ELEM(dna_type, "uchar", "unsigned char")) {
    return SDNA_TYPE_UCHAR;
  }
  if (STR_ELEM(dna_type, "short")) {
    return SDNA_TYPE_SHORT;
  }
  if (STR_ELEM(dna_type, "ushort", "unsigned short")) {
    return SDNA_TYPE_USHORT;
  }
  if (STR_ELEM(dna_type, "int")) {
    return SDNA_TYPE_INT;
  }
  if (STR_ELEM(dna_type, "float")) {
    return SDNA_TYPE_FLOAT;
  }
  if (STR_ELEM(dna_type, "double")) {
    return SDNA_TYPE_DOUBLE;
  }
  if (STR_ELEM(dna_type, "int64_t")) {
    return SDNA_TYPE_INT64;
  }
  if (STR_ELEM(dna_type, "uint64_t")) {
    return SDNA_TYPE_UINT64;
  }
  /* invalid! */

  return -1;
}

/**
 * Converts a value of one primitive type to another.
 * Note there is no optimization for the case where otype and ctype are the same:
 * assumption is that caller will handle this case.
 *
 * \param ctype: Name of type to convert to
 * \param otype: Name of type to convert from
 * \param name_array_len: Result of #DNA_elem_array_size for this element.
 * \param curdata: Where to put converted data
 * \param olddata: Data of type otype to convert
 */
static void cast_elem(
    const char *ctype, const char *otype, int name_array_len, char *curdata, const char *olddata)
{
  eSDNA_Type ctypenr, otypenr;
  if ((otypenr = sdna_type_nr(otype)) == -1 || (ctypenr = sdna_type_nr(ctype)) == -1) {
    return;
  }

  /* define lengths */
  const int oldlen = DNA_elem_type_size(otypenr);
  const int curlen = DNA_elem_type_size(ctypenr);

  double old_value_f = 0.0;
  uint64_t old_value_i = 0;

  while (name_array_len > 0) {
    switch (otypenr) {
      case SDNA_TYPE_CHAR:
        old_value_i = *olddata;
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_UCHAR:
        old_value_i = *((unsigned char *)olddata);
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_SHORT:
        old_value_i = *((short *)olddata);
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_USHORT:
        old_value_i = *((unsigned short *)olddata);
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_INT:
        old_value_i = *((int *)olddata);
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_FLOAT:
        old_value_f = *((float *)olddata);
        old_value_i = (uint64_t)(int64_t)old_value_f;
        break;
      case SDNA_TYPE_DOUBLE:
        old_value_f = *((double *)olddata);
        old_value_i = (uint64_t)(int64_t)old_value_f;
        break;
      case SDNA_TYPE_INT64:
        old_value_i = (uint64_t) * ((int64_t *)olddata);
        old_value_f = (double)old_value_i;
        break;
      case SDNA_TYPE_UINT64:
        old_value_i = *((uint64_t *)olddata);
        old_value_f = (double)old_value_i;
        break;
    }

    switch (ctypenr) {
      case SDNA_TYPE_CHAR:
        *curdata = (char)old_value_i;
        break;
      case SDNA_TYPE_UCHAR:
        *((unsigned char *)curdata) = (unsigned char)old_value_i;
        break;
      case SDNA_TYPE_SHORT:
        *((short *)curdata) = (short)old_value_i;
        break;
      case SDNA_TYPE_USHORT:
        *((unsigned short *)curdata) = (unsigned short)old_value_i;
        break;
      case SDNA_TYPE_INT:
        *((int *)curdata) = (int)old_value_i;
        break;
      case SDNA_TYPE_FLOAT:
        if (otypenr < 2) {
          old_value_f /= 255.0;
        }
        *((float *)curdata) = old_value_f;
        break;
      case SDNA_TYPE_DOUBLE:
        if (otypenr < 2) {
          old_value_f /= 255.0;
        }
        *((double *)curdata) = old_value_f;
        break;
      case SDNA_TYPE_INT64:
        *((int64_t *)curdata) = (int64_t)old_value_i;
        break;
      case SDNA_TYPE_UINT64:
        *((uint64_t *)curdata) = old_value_i;
        break;
    }

    olddata += oldlen;
    curdata += curlen;
    name_array_len--;
  }
}

/**
 * Converts pointer values between different sizes. These are only used
 * as lookup keys to identify data blocks in the saved .blend file, not
 * as actual in-memory pointers.
 *
 * \param curlen: Pointer length to convert to
 * \param oldlen: Length of pointers in olddata
 * \param name_array_len: Result of #DNA_elem_array_size for this element.
 * \param curdata: Where to put converted data
 * \param olddata: Data to convert
 */
static void cast_pointer(
    int curlen, int oldlen, int name_array_len, char *curdata, const char *olddata)
{
  while (name_array_len > 0) {

    if (curlen == oldlen) {
      memcpy(curdata, olddata, curlen);
    }
    else if (curlen == 4 && oldlen == 8) {
      int64_t lval = *((int64_t *)olddata);

      /* WARNING: 32-bit Blender trying to load file saved by 64-bit Blender,
       * pointers may lose uniqueness on truncation! (Hopefully this wont
       * happen unless/until we ever get to multi-gigabyte .blend files...) */
      *((int *)curdata) = lval >> 3;
    }
    else if (curlen == 8 && oldlen == 4) {
      *((int64_t *)curdata) = *((int *)olddata);
    }
    else {
      /* for debug */
      printf("errpr: illegal pointersize!\n");
    }

    olddata += oldlen;
    curdata += curlen;
    name_array_len--;
  }
}

/**
 * Equality test on name and oname excluding any array-size suffix.
 */
static bool elem_streq(const char *name, const char *oname)
{
  int a = 0;

  while (1) {
    if (name[a] != oname[a]) {
      return false;
    }
    if (name[a] == '[' || oname[a] == '[') {
      break;
    }
    if (name[a] == 0 || oname[a] == 0) {
      break;
    }
    a++;
  }
  return true;
}

/**
 * Returns whether the specified field exists according to the struct format
 * pointed to by old.
 *
 * \param type: Current field type name.
 * \param name: Current field name.
 * \param old: Pointer to struct information in sdna.
 * \return true when existing, false otherwise..
 */
static bool elem_exists_impl(
    /* Expand SDNA. */
    const char **types,
    const char **names,
    /* Regular args. */
    const char *type,
    const char *name,
    const SDNA_Struct *old)
{
  /* in old is the old struct */
  for (int a = 0; a < old->members_len; a++) {
    const SDNA_StructMember *member = &old->members[a];
    const char *otype = types[member->type];
    const char *oname = names[member->name];

    if (elem_streq(name, oname)) { /* name equal */
      return STREQ(type, otype);   /* type equal */
    }
  }
  return false;
}

/**
 * \param sdna: Old SDNA.
 */
static bool elem_exists(const SDNA *sdna,
                        const char *type,
                        const char *name,
                        const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->names,
      /* Regular args. */
      type,
      name,
      old);
}

static bool elem_exists_alias(const SDNA *sdna,
                              const char *type,
                              const char *name,
                              const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->alias.names,
      /* Regular args. */
      type,
      name,
      old);
}

/**
 * Returns the address of the data for the specified field within olddata
 * according to the struct format pointed to by old, or NULL if no such
 * field can be found.
 *
 * Passing olddata=NULL doesn't work reliably for existence checks; it will
 * return NULL both when the field is found at offset 0 and when it is not
 * found at all. For field existence checks, use #elem_exists() instead.
 *
 * \param sdna: Old SDNA
 * \param type: Current field type name
 * \param name: Current field name
 * \param old: Pointer to struct information in sdna
 * \param olddata: Struct data
 * \param sppo: Optional place to return pointer to field info in sdna
 * \return Data address.
 */
static const char *find_elem(const SDNA *sdna,
                             const char *type,
                             const char *name,
                             const SDNA_Struct *old,
                             const char *olddata,
                             const SDNA_StructMember **sppo)
{
  /* without arraypart, so names can differ: return old namenr and type */

  /* in old is the old struct */
  for (int a = 0; a < old->members_len; a++) {
    const SDNA_StructMember *member = &old->members[a];

    const char *otype = sdna->types[member->type];
    const char *oname = sdna->names[member->name];

    const int len = DNA_elem_size_nr(sdna, member->type, member->name);

    if (elem_streq(name, oname)) { /* name equal */
      if (STREQ(type, otype)) {    /* type equal */
        if (sppo) {
          *sppo = member;
        }
        return olddata;
      }

      return NULL;
    }

    olddata += len;
  }
  return NULL;
}

/**
 * Converts the contents of a single field of a struct, of a non-struct type,
 * from \a oldsdna to \a newsdna format.
 *
 * \param newsdna: SDNA of current Blender
 * \param oldsdna: SDNA of Blender that saved file
 * \param type: current field type name
 * \param new_name_nr: current field name number.
 * \param curdata: put field data converted to newsdna here
 * \param old: pointer to struct info in oldsdna
 * \param olddata: struct contents laid out according to oldsdna
 */
static void reconstruct_elem(const SDNA *newsdna,
                             const SDNA *oldsdna,
                             const char *type,
                             const int new_name_nr,
                             char *curdata,
                             const SDNA_Struct *old,
                             const char *olddata)
{
  /* rules: test for NAME:
   *      - name equal:
   *          - cast type
   *      - name partially equal (array differs)
   *          - type equal: memcpy
   *          - type cast (per element).
   * (nzc 2-4-2001 I want the 'unsigned' bit to be parsed as well. Where
   * can I force this?)
   */

  /* is 'name' an array? */
  const char *name = newsdna->names[new_name_nr];
  const char *cp = name;
  int countpos = 0;
  while (*cp && *cp != '[') {
    cp++;
    countpos++;
  }
  if (*cp != '[') {
    countpos = 0;
  }

  /* in old is the old struct */
  for (int a = 0; a < old->members_len; a++) {
    const SDNA_StructMember *old_member = &old->members[a];
    const int old_name_nr = old_member->name;
    const char *otype = oldsdna->types[old_member->type];
    const char *oname = oldsdna->names[old_member->name];
    const int len = DNA_elem_size_nr(oldsdna, old_member->type, old_member->name);

    if (STREQ(name, oname)) { /* name equal */

      if (ispointer(name)) { /* pointer of functionpointer afhandelen */
        cast_pointer(newsdna->pointer_size,
                     oldsdna->pointer_size,
                     newsdna->names_array_len[new_name_nr],
                     curdata,
                     olddata);
      }
      else if (STREQ(type, otype)) { /* type equal */
        memcpy(curdata, olddata, len);
      }
      else {
        cast_elem(type, otype, newsdna->names_array_len[new_name_nr], curdata, olddata);
      }

      return;
    }
    if (countpos != 0) { /* name is an array */

      if (oname[countpos] == '[' && strncmp(name, oname, countpos) == 0) { /* basis equal */
        const int new_name_array_len = newsdna->names_array_len[new_name_nr];
        const int old_name_array_len = oldsdna->names_array_len[old_name_nr];
        const int min_name_array_len = MIN2(new_name_array_len, old_name_array_len);

        if (ispointer(name)) { /* handle pointer or functionpointer */
          cast_pointer(
              newsdna->pointer_size, oldsdna->pointer_size, min_name_array_len, curdata, olddata);
        }
        else if (STREQ(type, otype)) { /* type equal */
                                       /* size of single old array element */
          int mul = len / old_name_array_len;
          /* smaller of sizes of old and new arrays */
          mul *= min_name_array_len;

          memcpy(curdata, olddata, mul);

          if (old_name_array_len > new_name_array_len && STREQ(type, "char")) {
            /* string had to be truncated, ensure it's still null-terminated */
            curdata[mul - 1] = '\0';
          }
        }
        else {
          cast_elem(type, otype, min_name_array_len, curdata, olddata);
        }
        return;
      }
    }
    olddata += len;
  }
}

/**
 * Converts the contents of an entire struct from oldsdna to newsdna format.
 *
 * \param newsdna: SDNA of current Blender
 * \param oldsdna: SDNA of Blender that saved file
 * \param compflags:
 *
 * Result from DNA_struct_get_compareflags to avoid needless conversions.
 * \param oldSDNAnr: Index of old struct definition in oldsdna
 * \param data: Struct contents laid out according to oldsdna
 * \param curSDNAnr: Index of current struct definition in newsdna
 * \param cur: Where to put converted struct contents
 */
static void reconstruct_struct(const SDNA *newsdna,
                               const SDNA *oldsdna,
                               const char *compflags,

                               int oldSDNAnr,
                               const char *data,
                               int curSDNAnr,
                               char *cur)
{
  /* Recursive!
   * Per element from cur_struct, read data from old_struct.
   * If element is a struct, call recursive.
   */

  if (oldSDNAnr == -1) {
    return;
  }
  if (curSDNAnr == -1) {
    return;
  }

  if (compflags[oldSDNAnr] == SDNA_CMP_EQUAL) {
    /* if recursive: test for equal */
    const SDNA_Struct *struct_old = oldsdna->structs[oldSDNAnr];
    const int elen = oldsdna->types_size[struct_old->type];
    memcpy(cur, data, elen);

    return;
  }

  const int firststructtypenr = newsdna->structs[0]->type;

  const SDNA_Struct *struct_old = oldsdna->structs[oldSDNAnr];
  const SDNA_Struct *struct_new = newsdna->structs[curSDNAnr];

  char *cpc = cur;
  for (int a = 0; a < struct_new->members_len; a++) { /* convert each field */
    const SDNA_StructMember *member_new = &struct_new->members[a];
    const char *type = newsdna->types[member_new->type];
    const char *name = newsdna->names[member_new->name];

    int elen = DNA_elem_size_nr(newsdna, member_new->type, member_new->name);

    /* Skip pad bytes which must start with '_pad', see makesdna.c 'is_name_legal'.
     * for exact rules. Note that if we fail to skip a pad byte it's harmless,
     * this just avoids unnecessary reconstruction. */
    if (name[0] == '_' || (name[0] == '*' && name[1] == '_')) {
      cpc += elen;
    }
    else if (member_new->type >= firststructtypenr && !ispointer(name)) {
      /* struct field type */

      /* where does the old struct data start (and is there an old one?) */
      const SDNA_StructMember *member_old;
      const char *cpo = find_elem(oldsdna, type, name, struct_old, data, &member_old);

      if (cpo) {
        unsigned int oldsdna_index_last = UINT_MAX;
        unsigned int cursdna_index_last = UINT_MAX;
        oldSDNAnr = DNA_struct_find_nr_ex(oldsdna, type, &oldsdna_index_last);
        curSDNAnr = DNA_struct_find_nr_ex(newsdna, type, &cursdna_index_last);

        /* array! */
        int mul = newsdna->names_array_len[member_new->name];
        int mulo = oldsdna->names_array_len[member_old->name];

        int eleno = DNA_elem_size_nr(oldsdna, member_old->type, member_old->name);

        elen /= mul;
        eleno /= mulo;

        while (mul--) {
          reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
          cpo += eleno;
          cpc += elen;

          /* new struct array larger than old */
          mulo--;
          if (mulo <= 0) {
            break;
          }
        }
      }
      else {
        cpc += elen; /* skip field no longer present */
      }
    }
    else {
      /* non-struct field type */
      reconstruct_elem(newsdna, oldsdna, type, member_new->name, cpc, struct_old, data);
      cpc += elen;
    }
  }
}

/**
 * Does endian swapping on the fields of a struct value.
 *
 * \param oldsdna: SDNA of Blender that saved file
 * \param oldSDNAnr: Index of struct info within oldsdna
 * \param data: Struct data
 */
void DNA_struct_switch_endian(const SDNA *oldsdna, int oldSDNAnr, char *data)
{
  /* Recursive!
   * If element is a struct, call recursive.
   */
  if (oldSDNAnr == -1) {
    return;
  }
  const int firststructtypenr = oldsdna->structs[0]->type;
  const SDNA_Struct *struct_info = oldsdna->structs[oldSDNAnr];
  char *cur = data;
  for (int a = 0; a < struct_info->members_len; a++) {
    const SDNA_StructMember *member = &struct_info->members[a];
    const char *type = oldsdna->types[member->type];
    const char *name = oldsdna->names[member->name];
    const int old_name_array_len = oldsdna->names_array_len[member->name];

    /* DNA_elem_size_nr = including arraysize */
    const int elen = DNA_elem_size_nr(oldsdna, member->type, member->name);

    /* test: is type a struct? */
    if (member->type >= firststructtypenr && !ispointer(name)) {
      /* struct field type */
      /* where does the old data start (is there one?) */
      char *cpo = (char *)find_elem(oldsdna, type, name, struct_info, data, NULL);
      if (cpo) {
        unsigned int oldsdna_index_last = UINT_MAX;
        oldSDNAnr = DNA_struct_find_nr_ex(oldsdna, type, &oldsdna_index_last);

        int mul = old_name_array_len;
        const int elena = elen / mul;

        while (mul--) {
          DNA_struct_switch_endian(oldsdna, oldSDNAnr, cpo);
          cpo += elena;
        }
      }
    }
    else {
      /* non-struct field type */
      if (ispointer(name)) {
        if (oldsdna->pointer_size == 8) {
          BLI_endian_switch_int64_array((int64_t *)cur, old_name_array_len);
        }
      }
      else {
        if (ELEM(member->type, SDNA_TYPE_SHORT, SDNA_TYPE_USHORT)) {

          /* exception: variable called blocktype: derived from ID_  */
          bool skip = false;
          if (name[0] == 'b' && name[1] == 'l') {
            if (STREQ(name, "blocktype")) {
              skip = true;
            }
          }

          if (skip == false) {
            BLI_endian_switch_int16_array((int16_t *)cur, old_name_array_len);
          }
        }
        else if (ELEM(member->type, SDNA_TYPE_INT, SDNA_TYPE_FLOAT)) {
          /* note, intentionally ignore long/ulong here these could be 4 or 8 bits,
           * but turns out we only used for runtime vars and
           * only once for a struct type that's no longer used. */

          BLI_endian_switch_int32_array((int32_t *)cur, old_name_array_len);
        }
        else if (ELEM(member->type, SDNA_TYPE_INT64, SDNA_TYPE_UINT64, SDNA_TYPE_DOUBLE)) {
          BLI_endian_switch_int64_array((int64_t *)cur, old_name_array_len);
        }
      }
    }
    cur += elen;
  }
}

typedef enum eReconstructStepType {
  RECONSTRUCT_STEP_MEMCPY,
  RECONSTRUCT_STEP_CAST_ELEMENT,
  RECONSTRUCT_STEP_CAST_POINTER_TO_32,
  RECONSTRUCT_STEP_CAST_POINTER_TO_64,
  RECONSTRUCT_STEP_SUBSTRUCT,
  RECONSTRUCT_STEP_INIT_ZERO,
} eReconstructStepType;

typedef struct ReconstructStepMemcpy {
  int old_offset;
  int new_offset;
  int size;
} ReconstructStepMemcpy;

typedef struct ReconstructStepCastElement {
  int old_offset;
  int new_offset;
  int amount;
  eSDNA_Type old_type;
  eSDNA_Type new_type;
} ReconstructStepCastElement;

typedef struct ReconstructStepCastPointer {
  int old_offset;
  int new_offset;
  int amount;
} ReconstructStepCastPointer;

typedef struct ReconstructStepSubstruct {
  int old_offset;
  int new_offset;
  int amount;
  short new_struct_number;
} ReconstructStepSubstruct;

typedef struct ReconstructStep {
  eReconstructStepType step_type;
  union {
    ReconstructStepMemcpy memcpy;
    ReconstructStepCastElement cast_element;
    ReconstructStepCastPointer cast_pointer;
    ReconstructStepSubstruct substruct;
  } data;
} ReconstructStep;

typedef struct DNA_ReconstructInfo {
  const SDNA *oldsdna;
  const SDNA *newsdna;
  const char *compare_flags;
} DNA_ReconstructInfo;

/**
 * \param newsdna: SDNA of current Blender
 * \param oldsdna: SDNA of Blender that saved file
 * \param compflags:
 *
 * Result from DNA_struct_get_compareflags to avoid needless conversions
 * \param oldSDNAnr: Index of struct info within oldsdna
 * \param blocks: The number of array elements
 * \param data: Array of struct data
 * \return An allocated reconstructed struct
 */
void *DNA_struct_reconstruct(const DNA_ReconstructInfo *reconstruct_info,
                             int oldSDNAnr,
                             int blocks,
                             const void *data)
{
  const SDNA *oldsdna = reconstruct_info->oldsdna;
  const SDNA *newsdna = reconstruct_info->newsdna;
  const char *compflags = reconstruct_info->compare_flags;

  /* oldSDNAnr == structnr, we're looking for the corresponding 'cur' number */
  const SDNA_Struct *struct_old = oldsdna->structs[oldSDNAnr];
  const char *type = oldsdna->types[struct_old->type];
  const int oldlen = oldsdna->types_size[struct_old->type];
  const int curSDNAnr = DNA_struct_find_nr(newsdna, type);

  /* init data and alloc */
  int curlen = 0;
  if (curSDNAnr != -1) {
    const SDNA_Struct *struct_new = newsdna->structs[curSDNAnr];
    curlen = newsdna->types_size[struct_new->type];
  }
  if (curlen == 0) {
    return NULL;
  }

  char *cur = MEM_callocN(blocks * curlen, "reconstruct");
  char *cpc = cur;
  const char *cpo = data;
  for (int a = 0; a < blocks; a++) {
    reconstruct_struct(newsdna, oldsdna, compflags, oldSDNAnr, cpo, curSDNAnr, cpc);
    cpc += curlen;
    cpo += oldlen;
  }

  return cur;
}

DNA_ReconstructInfo *DNA_reconstruct_info_new(const SDNA *oldsdna,
                                              const SDNA *newsdna,
                                              const char *compare_flags)
{
  DNA_ReconstructInfo *reconstruct_info = MEM_callocN(sizeof(DNA_ReconstructInfo), AT);
  reconstruct_info->oldsdna = oldsdna;
  reconstruct_info->newsdna = newsdna;
  reconstruct_info->compare_flags = compare_flags;
  return reconstruct_info;
}

void DNA_reconstruct_info_free(DNA_ReconstructInfo *reconstruct_info)
{
  MEM_freeN(reconstruct_info);
}

static ReconstructStep *create_member_reconstruct_steps(const SDNA *oldsdna,
                                                        const SDNA *newsdna,
                                                        const char *compare_flags,
                                                        SDNA_Struct *new_struct)
{
  const char *new_struct_name = newsdna->types[new_struct->type];
  const int old_struct_number = DNA_struct_find_nr(oldsdna, new_struct_name);
  BLI_assert(old_struct_number >= 0);
  SDNA_Struct *old_struct = oldsdna->structs[old_struct_number];

  ReconstructStep *steps = MEM_calloc_arrayN(new_struct->members_len, sizeof(ReconstructStep), AT);

  int new_member_offset = 0;
  for (int new_member_index = 0; new_member_index < new_struct->members_len; new_member_index++) {
    SDNA_StructMember *new_member = &new_struct->members[new_member_index];

    const char *new_name = newsdna->names[new_member->name];
    const int new_type_number = new_member->type;
    const char *new_type_name = newsdna->types[new_type_number];
    const int new_array_length = newsdna->names_array_len[new_member->name];
    const int new_member_type_size = newsdna->types_size[new_type_number];
    const int new_member_size = new_member_type_size * new_array_length;

    /* Find matching member in old struct. */
    int old_member_offset = 0;
    SDNA_StructMember *matching_old_member = NULL;
    for (int old_member_index = 0; old_member_index < old_struct->members_len;
         old_member_index++) {
      SDNA_StructMember *old_member = &old_struct->members[old_member_index];
      const char *old_name = oldsdna->names[old_member->name];
      if (elem_streq(old_name, new_name)) {
        matching_old_member = old_member;
        break;
      }
      old_member_offset += oldsdna->types_size[old_member->type] *
                           oldsdna->names_array_len[old_member->name];
    }

    ReconstructStep *step = &steps[new_member_index];
    /* Initialize the reconstruction step for this member. This handles all the different
     * conversions of member types that can happen. */
    if (matching_old_member == NULL) {
      step->step_type = RECONSTRUCT_STEP_INIT_ZERO;
    }
    else {
      const int old_type_number = matching_old_member->type;
      const char *old_type_name = oldsdna->types[old_type_number];
      const int old_array_length = oldsdna->names_array_len[matching_old_member->name];
      const char *old_name = oldsdna->names[matching_old_member->name];
      const int shared_array_length = MIN2(old_array_length, new_array_length);

      const bool new_is_pointer = ispointer(new_name);
      const bool old_is_pointer = ispointer(old_name);

      if (new_is_pointer && old_is_pointer) {
        if (newsdna->pointer_size == oldsdna->pointer_size) {
          step->step_type = RECONSTRUCT_STEP_MEMCPY;
          step->data.memcpy.new_offset = new_member_offset;
          step->data.memcpy.old_offset = old_member_offset;
          step->data.memcpy.size = newsdna->pointer_size * shared_array_length;
        }
        else if (newsdna->pointer_size > oldsdna->pointer_size) {
          step->step_type = RECONSTRUCT_STEP_CAST_POINTER_TO_64;
          step->data.cast_pointer.new_offset = new_member_offset;
          step->data.cast_pointer.old_offset = old_member_offset;
          step->data.cast_pointer.amount = shared_array_length;
        }
        else {
          step->step_type = RECONSTRUCT_STEP_CAST_POINTER_TO_32;
          step->data.cast_pointer.new_offset = new_member_offset;
          step->data.cast_pointer.old_offset = old_member_offset;
          step->data.cast_pointer.amount = shared_array_length;
        }
      }
      else if (new_is_pointer != old_is_pointer) {
        step->step_type = RECONSTRUCT_STEP_INIT_ZERO;
      }
      else {
        const int old_member_struct_number = DNA_struct_find_nr(oldsdna, old_type_name);
        const int new_member_struct_number = DNA_struct_find_nr(newsdna, new_type_name);
        const bool old_member_is_struct = old_member_struct_number >= 0;
        const bool new_member_is_struct = new_member_struct_number >= 0;

        if (STREQ(old_type_name, new_type_name)) {
          if (new_member_struct_number && old_member_is_struct) {
            enum eSDNA_StructCompare compare_flag = compare_flags[old_member_struct_number];
            BLI_assert(compare_flag != SDNA_CMP_REMOVED);
            if (compare_flag == SDNA_CMP_EQUAL) {
              step->step_type = RECONSTRUCT_STEP_MEMCPY;
              step->data.memcpy.new_offset = new_member_offset;
              step->data.memcpy.old_offset = old_member_offset;
              step->data.memcpy.size = new_member_size;
            }
            else {
              BLI_assert(compare_flag == SDNA_CMP_NOT_EQUAL);
              step->step_type = RECONSTRUCT_STEP_SUBSTRUCT;
              step->data.substruct.new_offset = new_member_offset;
              step->data.substruct.old_offset = old_member_offset;
              step->data.substruct.amount = shared_array_length;
              step->data.substruct.new_struct_number = new_member_struct_number;
            }
          }
          else {
            step->step_type = RECONSTRUCT_STEP_INIT_ZERO;
          }
        }
        else {
          if (new_member_is_struct && old_member_is_struct) {
            /* If the struct name changed, we don't know how to match them. */
            step->step_type = RECONSTRUCT_STEP_INIT_ZERO;
          }
          else if (new_member_is_struct != old_member_is_struct) {
            /* Cannot convert between simple type (int, float, ...) and a struct. */
            step->step_type = RECONSTRUCT_STEP_INIT_ZERO;
          }
          else {
            /* Both members have a simple type (int, float, ...). Cast from old to new type. */
            step->step_type = RECONSTRUCT_STEP_CAST_ELEMENT;
            step->data.cast_element.amount = shared_array_length;
            step->data.cast_element.new_offset = new_member_offset;
            step->data.cast_element.old_offset = old_member_offset;
            step->data.cast_element.new_type = new_type_number;
            step->data.cast_element.old_type = old_type_number;
          }
        }
      }
    }

    new_member_offset += new_member_size;
  }

  return steps;
}

/**
 * Returns the offset of the field with the specified name and type within the specified
 * struct type in sdna.
 */
int DNA_elem_offset(SDNA *sdna, const char *stype, const char *vartype, const char *name)
{
  const int SDNAnr = DNA_struct_find_nr(sdna, stype);
  const SDNA_Struct *const spo = sdna->structs[SDNAnr];
  const char *const cp = find_elem(sdna, vartype, name, spo, NULL, NULL);
  BLI_assert(SDNAnr != -1);
  return (int)((intptr_t)cp);
}

bool DNA_struct_find(const SDNA *sdna, const char *stype)
{
  return DNA_struct_find_nr(sdna, stype) != -1;
}

bool DNA_struct_elem_find(const SDNA *sdna,
                          const char *stype,
                          const char *vartype,
                          const char *name)
{
  const int SDNAnr = DNA_struct_find_nr(sdna, stype);

  if (SDNAnr != -1) {
    const SDNA_Struct *const spo = sdna->structs[SDNAnr];
    const bool found = elem_exists(sdna, vartype, name, spo);

    if (found) {
      return true;
    }
  }
  return false;
}

/** \note requires #DNA_sdna_alias_data_ensure_structs_map to be called. */
bool DNA_struct_alias_elem_find(const SDNA *sdna,
                                const char *stype,
                                const char *vartype,
                                const char *name)
{
  const int SDNAnr = DNA_struct_alias_find_nr(sdna, stype);

  if (SDNAnr != -1) {
    const SDNA_Struct *const spo = sdna->structs[SDNAnr];
    const bool found = elem_exists_alias(sdna, vartype, name, spo);

    if (found) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the size in bytes of a primitive type.
 */
int DNA_elem_type_size(const eSDNA_Type elem_nr)
{
  /* should contain all enum types */
  switch (elem_nr) {
    case SDNA_TYPE_CHAR:
    case SDNA_TYPE_UCHAR:
      return 1;
    case SDNA_TYPE_SHORT:
    case SDNA_TYPE_USHORT:
      return 2;
    case SDNA_TYPE_INT:
    case SDNA_TYPE_FLOAT:
      return 4;
    case SDNA_TYPE_DOUBLE:
    case SDNA_TYPE_INT64:
    case SDNA_TYPE_UINT64:
      return 8;
  }

  /* weak */
  return 8;
}

/* -------------------------------------------------------------------- */
/** \name Version Patch DNA
 * \{ */

static bool DNA_sdna_patch_struct_nr(SDNA *sdna,
                                     const int struct_name_old_nr,
                                     const char *struct_name_new)
{
  BLI_assert(DNA_struct_find_nr(DNA_sdna_current_get(), struct_name_new) != -1);
  const SDNA_Struct *struct_info = sdna->structs[struct_name_old_nr];
#ifdef WITH_DNA_GHASH
  BLI_ghash_remove(sdna->structs_map, (void *)sdna->types[struct_info->type], NULL, NULL);
  BLI_ghash_insert(
      sdna->structs_map, (void *)struct_name_new, POINTER_FROM_INT(struct_name_old_nr));
#endif
  sdna->types[struct_info->type] = struct_name_new;
  return true;
}
/**
 * Rename a struct
 */
bool DNA_sdna_patch_struct(SDNA *sdna, const char *struct_name_old, const char *struct_name_new)
{
  const int struct_name_old_nr = DNA_struct_find_nr(sdna, struct_name_old);
  if (struct_name_old_nr != -1) {
    return DNA_sdna_patch_struct_nr(sdna, struct_name_old_nr, struct_name_new);
  }
  return false;
}

/* Make public if called often with same struct (avoid duplicate look-ups). */
static bool DNA_sdna_patch_struct_member_nr(SDNA *sdna,
                                            const int struct_name_nr,
                                            const char *elem_old,
                                            const char *elem_new)
{
  /* These names aren't handled here (it's not used).
   * Ensure they are never used or we get out of sync arrays. */
  BLI_assert(sdna->alias.names == NULL);
  const int elem_old_len = strlen(elem_old);
  const int elem_new_len = strlen(elem_new);
  BLI_assert(elem_new != NULL);
  SDNA_Struct *sp = sdna->structs[struct_name_nr];
  for (int elem_index = sp->members_len; elem_index > 0; elem_index--) {
    SDNA_StructMember *member = &sp->members[elem_index];
    const char *elem_old_full = sdna->names[member->name];
    /* Start & end offsets in 'elem_old_full'. */
    uint elem_old_full_offset_start;
    if (DNA_elem_id_match(elem_old, elem_old_len, elem_old_full, &elem_old_full_offset_start)) {
      if (sdna->mem_arena == NULL) {
        sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }
      const char *elem_new_full = DNA_elem_id_rename(sdna->mem_arena,
                                                     elem_old,
                                                     elem_old_len,
                                                     elem_new,
                                                     elem_new_len,
                                                     elem_old_full,
                                                     strlen(elem_old_full),
                                                     elem_old_full_offset_start);

      if (sdna->names_len == sdna->names_len_alloc) {
        sdna->names_len_alloc += 64;
        sdna->names = MEM_recallocN((void *)sdna->names,
                                    sizeof(*sdna->names) * sdna->names_len_alloc);
        sdna->names_array_len = MEM_recallocN(
            (void *)sdna->names_array_len, sizeof(*sdna->names_array_len) * sdna->names_len_alloc);
      }
      const short name_nr_prev = member->name;
      member->name = sdna->names_len++;
      sdna->names[member->name] = elem_new_full;
      sdna->names_array_len[member->name] = sdna->names_array_len[name_nr_prev];

      return true;
    }
  }
  return false;
}
/**
 * Replace \a elem_old with \a elem_new for struct \a struct_name
 * handles search & replace, maintaining surrounding non-identifier characters
 * such as pointer & array size.
 */
bool DNA_sdna_patch_struct_member(SDNA *sdna,
                                  const char *struct_name,
                                  const char *elem_old,
                                  const char *elem_new)
{
  const int struct_name_nr = DNA_struct_find_nr(sdna, struct_name);
  if (struct_name_nr != -1) {
    return DNA_sdna_patch_struct_member_nr(sdna, struct_name_nr, elem_old, elem_new);
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Versioning (Forward Compatible)
 *
 * Versioning that allows new names.
 * \{ */

/**
 * Names are shared between structs which causes problems renaming.
 * Make sure every struct member gets it's own name so renaming only ever impacts a single struct.
 *
 * The resulting SDNA is never written to disk.
 */
static void sdna_expand_names(SDNA *sdna)
{
  int names_expand_len = 0;
  for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
    const SDNA_Struct *struct_old = sdna->structs[struct_nr];
    names_expand_len += struct_old->members_len;
  }
  const char **names_expand = MEM_mallocN(sizeof(*names_expand) * names_expand_len, __func__);
  short *names_array_len_expand = MEM_mallocN(sizeof(*names_array_len_expand) * names_expand_len,
                                              __func__);

  int names_expand_index = 0;
  for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
    /* We can't edit this memory 'sdna->structs' points to (readonly datatoc file). */
    const SDNA_Struct *struct_old = sdna->structs[struct_nr];

    const int array_size = sizeof(short) * 2 + sizeof(SDNA_StructMember) * struct_old->members_len;
    SDNA_Struct *struct_new = BLI_memarena_alloc(sdna->mem_arena, array_size);
    memcpy(struct_new, struct_old, array_size);
    sdna->structs[struct_nr] = struct_new;

    for (int i = 0; i < struct_old->members_len; i++) {
      const SDNA_StructMember *member_old = &struct_old->members[i];
      SDNA_StructMember *member_new = &struct_new->members[i];

      names_expand[names_expand_index] = sdna->names[member_old->name];
      names_array_len_expand[names_expand_index] = sdna->names_array_len[member_old->name];

      BLI_assert(names_expand_index < SHRT_MAX);
      member_new->name = names_expand_index;
      names_expand_index++;
    }
  }
  MEM_freeN((void *)sdna->names);
  sdna->names = names_expand;

  MEM_freeN((void *)sdna->names_array_len);
  sdna->names_array_len = names_array_len_expand;

  sdna->names_len = names_expand_len;
}

static const char *dna_sdna_alias_from_static_elem_full(SDNA *sdna,
                                                        GHash *elem_map_alias_from_static,
                                                        const char *struct_name_static,
                                                        const char *elem_static_full)
{
  const int elem_static_full_len = strlen(elem_static_full);
  char *elem_static = alloca(elem_static_full_len + 1);
  const int elem_static_len = DNA_elem_id_strip_copy(elem_static, elem_static_full);
  const char *str_pair[2] = {struct_name_static, elem_static};
  const char *elem_alias = BLI_ghash_lookup(elem_map_alias_from_static, str_pair);
  if (elem_alias) {
    return DNA_elem_id_rename(sdna->mem_arena,
                              elem_static,
                              elem_static_len,
                              elem_alias,
                              strlen(elem_alias),
                              elem_static_full,
                              elem_static_full_len,
                              DNA_elem_id_offset_start(elem_static_full));
  }
  return NULL;
}

void DNA_sdna_alias_data_ensure(SDNA *sdna)
{
  /* We may want this to be optional later. */
  const bool use_legacy_hack = true;

  if (sdna->mem_arena == NULL) {
    sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  }

  GHash *struct_map_alias_from_static;
  GHash *elem_map_alias_from_static;

  DNA_alias_maps(
      DNA_RENAME_ALIAS_FROM_STATIC, &struct_map_alias_from_static, &elem_map_alias_from_static);

  if (sdna->alias.types == NULL) {
    sdna->alias.types = MEM_mallocN(sizeof(*sdna->alias.types) * sdna->types_len, __func__);
    for (int type_nr = 0; type_nr < sdna->types_len; type_nr++) {
      const char *struct_name_static = sdna->types[type_nr];

      if (use_legacy_hack) {
        struct_name_static = DNA_struct_rename_legacy_hack_alias_from_static(struct_name_static);
      }

      sdna->alias.types[type_nr] = BLI_ghash_lookup_default(
          struct_map_alias_from_static, struct_name_static, (void *)struct_name_static);
    }
  }

  if (sdna->alias.names == NULL) {
    sdna_expand_names(sdna);
    sdna->alias.names = MEM_mallocN(sizeof(*sdna->alias.names) * sdna->names_len, __func__);
    for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
      const SDNA_Struct *struct_info = sdna->structs[struct_nr];
      const char *struct_name_static = sdna->types[struct_info->type];

      if (use_legacy_hack) {
        struct_name_static = DNA_struct_rename_legacy_hack_alias_from_static(struct_name_static);
      }

      for (int a = 0; a < struct_info->members_len; a++) {
        const SDNA_StructMember *member = &struct_info->members[a];
        const char *elem_alias_full = dna_sdna_alias_from_static_elem_full(
            sdna, elem_map_alias_from_static, struct_name_static, sdna->names[member->name]);
        if (elem_alias_full != NULL) {
          sdna->alias.names[member->name] = elem_alias_full;
        }
        else {
          sdna->alias.names[member->name] = sdna->names[member->name];
        }
      }
    }
  }
  BLI_ghash_free(struct_map_alias_from_static, NULL, NULL);
  BLI_ghash_free(elem_map_alias_from_static, MEM_freeN, NULL);
}

/**
 * Separated from #DNA_sdna_alias_data_ensure because it's not needed
 * unless we want to lookup aliased struct names (#DNA_struct_alias_find_nr and friends).
 */
void DNA_sdna_alias_data_ensure_structs_map(SDNA *sdna)
{
  DNA_sdna_alias_data_ensure(sdna);
#ifdef WITH_DNA_GHASH
  /* create a ghash lookup to speed up */
  struct GHash *structs_map = BLI_ghash_str_new_ex(__func__, sdna->structs_len);
  for (intptr_t nr = 0; nr < sdna->structs_len; nr++) {
    const SDNA_Struct *struct_info = sdna->structs[nr];
    BLI_ghash_insert(
        structs_map, (void *)sdna->alias.types[struct_info->type], POINTER_FROM_INT(nr));
  }
  sdna->alias.structs_map = structs_map;
#else
  UNUSED_VARS(sdna);
#endif
}

/** \} */
