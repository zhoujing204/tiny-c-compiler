/*
 * TCC - Tiny C Compiler
 *
 * Section management for code and data.
 */

#include "tcc.h"

/* ELF section types (we use these for internal consistency) */
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8

/* Section flags */
#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

/*============================================================
 * Section Creation and Management
 *============================================================*/

Section *new_section(TCCState *s, const char *name, int sh_type, int sh_flags) {
  Section *sec;

  sec = tcc_malloc(sizeof(Section));
  memset(sec, 0, sizeof(Section));

  strncpy(sec->name, name, sizeof(sec->name) - 1);
  sec->sh_type = sh_type;
  sec->sh_flags = sh_flags;

  /* Initial allocation */
  sec->data_alloc = 256;
  sec->data = tcc_malloc(sec->data_alloc);
  sec->data_size = 0;

  /* Add to linked list */
  sec->next = s->sections;
  s->sections = sec;

  return sec;
}

void section_realloc(Section *sec, size_t new_size) {
  if (new_size > sec->data_alloc) {
    size_t new_alloc = sec->data_alloc;
    while (new_alloc < new_size) {
      new_alloc *= 2;
    }
    sec->data = tcc_realloc(sec->data, new_alloc);
    sec->data_alloc = new_alloc;
  }
}

/* Add data to section, return offset */
size_t section_add(Section *sec, const void *data, size_t size) {
  size_t offset = sec->data_size;

  section_realloc(sec, sec->data_size + size);
  memcpy(sec->data + sec->data_size, data, size);
  sec->data_size += size;

  return offset;
}

/* Reserve space in section, return pointer to it */
void *section_ptr_add(Section *sec, size_t size) {
  void *ptr;

  section_realloc(sec, sec->data_size + size);
  ptr = sec->data + sec->data_size;
  sec->data_size += size;

  return ptr;
}
