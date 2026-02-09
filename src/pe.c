/*
 * TCC - Tiny C Compiler
 *
 * PE (Portable Executable) file generator for Windows.
 */

#include "tcc.h"

/*============================================================
 * PE File Format Structures
 *============================================================*/

/* DOS Header */
typedef struct {
  uint16_t e_magic; /* 'MZ' */
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew; /* Offset to PE header */
} DOSHeader;

/* PE Signature + COFF Header */
typedef struct {
  uint32_t signature; /* 'PE\0\0' */
  uint16_t machine;
  uint16_t numberOfSections;
  uint32_t timeDateStamp;
  uint32_t pointerToSymbolTable;
  uint32_t numberOfSymbols;
  uint16_t sizeOfOptionalHeader;
  uint16_t characteristics;
} PEHeader;

/* PE Optional Header (64-bit) */
typedef struct {
  uint16_t magic;
  uint8_t majorLinkerVersion;
  uint8_t minorLinkerVersion;
  uint32_t sizeOfCode;
  uint32_t sizeOfInitializedData;
  uint32_t sizeOfUninitializedData;
  uint32_t addressOfEntryPoint;
  uint32_t baseOfCode;
  uint64_t imageBase;
  uint32_t sectionAlignment;
  uint32_t fileAlignment;
  uint16_t majorOperatingSystemVersion;
  uint16_t minorOperatingSystemVersion;
  uint16_t majorImageVersion;
  uint16_t minorImageVersion;
  uint16_t majorSubsystemVersion;
  uint16_t minorSubsystemVersion;
  uint32_t win32VersionValue;
  uint32_t sizeOfImage;
  uint32_t sizeOfHeaders;
  uint32_t checkSum;
  uint16_t subsystem;
  uint16_t dllCharacteristics;
  uint64_t sizeOfStackReserve;
  uint64_t sizeOfStackCommit;
  uint64_t sizeOfHeapReserve;
  uint64_t sizeOfHeapCommit;
  uint32_t loaderFlags;
  uint32_t numberOfRvaAndSizes;
} PEOptionalHeader64;

/* Data Directory */
typedef struct {
  uint32_t virtualAddress;
  uint32_t size;
} DataDirectory;

/* Section Header */
typedef struct {
  char name[8];
  uint32_t virtualSize;
  uint32_t virtualAddress;
  uint32_t sizeOfRawData;
  uint32_t pointerToRawData;
  uint32_t pointerToRelocations;
  uint32_t pointerToLinenumbers;
  uint16_t numberOfRelocations;
  uint16_t numberOfLinenumbers;
  uint32_t characteristics;
} SectionHeader;

/*============================================================
 * Constants
 *============================================================*/

#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020

#define IMAGE_SUBSYSTEM_WINDOWS_CUI 3 /* Console application */
#define IMAGE_SUBSYSTEM_WINDOWS_GUI 2 /* GUI application */

#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000

#define PE_HEADER_SIZE 0x200      /* Size of all headers (aligned) */
#define SECTION_ALIGNMENT 0x1000  /* Section alignment in memory */
#define FILE_ALIGNMENT 0x200      /* Section alignment in file */
#define IMAGE_BASE 0x140000000ULL /* Default image base for x64 */

/*============================================================
 * Helper Functions
 *============================================================*/

static uint32_t align_up(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

static void write_u16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

static void write_u32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
  p[2] = (v >> 16) & 0xff;
  p[3] = (v >> 24) & 0xff;
}

static void write_u64(uint8_t *p, uint64_t v) {
  write_u32(p, (uint32_t)v);
  write_u32(p + 4, (uint32_t)(v >> 32));
}

/*============================================================
 * PE Output
 *============================================================*/

int pe_output_file(TCCState *s, const char *filename) {
  FILE *f;
  uint8_t header[PE_HEADER_SIZE];
  int num_sections = 0;
  uint32_t file_offset, virtual_addr;

  /* Count sections */
  if (s->text_section && s->text_section->data_size > 0)
    num_sections++;
  if (s->data_section && s->data_section->data_size > 0)
    num_sections++;
  if (s->rdata_section && s->rdata_section->data_size > 0)
    num_sections++;
  if (s->bss_section && s->bss_section->data_size > 0)
    num_sections++;

  /* Default to 1 section if we have any code */
  if (num_sections == 0 && s->text_section) {
    /* Create a minimal main that returns 0 */
    /* push rbp */
    g(s, 0x55);
    /* mov rbp, rsp */
    g(s, 0x48);
    g(s, 0x89);
    g(s, 0xe5);
    /* xor eax, eax */
    g(s, 0x31);
    g(s, 0xc0);
    /* pop rbp */
    g(s, 0x5d);
    /* ret */
    g(s, 0xc3);
    num_sections = 1;
  }

  memset(header, 0, sizeof(header));

  /* DOS Header */
  header[0] = 'M';
  header[1] = 'Z';
  write_u32(header + 0x3c, 0x80); /* Offset to PE header */

  /* DOS stub (minimal) */
  /* "This program cannot be run in DOS mode." */

  /* PE Signature at offset 0x80 */
  header[0x80] = 'P';
  header[0x81] = 'E';
  header[0x82] = 0;
  header[0x83] = 0;

  /* COFF Header at 0x84 */
  write_u16(header + 0x84, IMAGE_FILE_MACHINE_AMD64);
  write_u16(header + 0x86, num_sections);
  write_u32(header + 0x88, 0);   /* Timestamp */
  write_u32(header + 0x8c, 0);   /* Symbol table pointer */
  write_u32(header + 0x90, 0);   /* Number of symbols */
  write_u16(header + 0x94, 240); /* Size of optional header (PE32+) */
  write_u16(header + 0x96,
            IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE);

  /* Optional Header at 0x98 */
  write_u16(header + 0x98, 0x20b); /* PE32+ magic */
  header[0x9a] = 1;                /* Linker major version */
  header[0x9b] = 0;                /* Linker minor version */

  /* Calculate sizes */
  uint32_t size_of_code =
      s->text_section
          ? align_up((uint32_t)s->text_section->data_size, FILE_ALIGNMENT)
          : 0;
  uint32_t size_of_init_data = 0;
  if (s->data_section)
    size_of_init_data +=
        align_up((uint32_t)s->data_section->data_size, FILE_ALIGNMENT);
  if (s->rdata_section)
    size_of_init_data +=
        align_up((uint32_t)s->rdata_section->data_size, FILE_ALIGNMENT);

  write_u32(header + 0x9c, size_of_code);
  write_u32(header + 0xa0, size_of_init_data);
  write_u32(header + 0xa4,
            s->bss_section ? (uint32_t)s->bss_section->data_size : 0);
  /* Entry point RVA */
  uint32_t entry_point = SECTION_ALIGNMENT;
  Sym *main_sym = sym_find2(s, "main");
  if (main_sym) {
    entry_point += (uint32_t)main_sym->c;
    if (s->verbose)
      printf("Entry point set to 'main' at RVA %08x\n", entry_point);
  } else {
    if (s->verbose)
      printf("Entry point set to start of .text at RVA %08x (main not found)\n",
             entry_point);
  }
  write_u32(header + 0xa8, entry_point);

  write_u32(header + 0xac, SECTION_ALIGNMENT); /* Base of code */
  write_u64(header + 0xb0, IMAGE_BASE);
  write_u32(header + 0xb8, SECTION_ALIGNMENT);
  write_u32(header + 0xbc, FILE_ALIGNMENT);
  write_u16(header + 0xc0, 6); /* OS major version */
  write_u16(header + 0xc2, 0); /* OS minor version */
  write_u16(header + 0xc4, 0); /* Image major version */
  write_u16(header + 0xc6, 0); /* Image minor version */
  write_u16(header + 0xc8, 6); /* Subsystem major version */
  write_u16(header + 0xca, 0); /* Subsystem minor version */
  write_u32(header + 0xcc, 0); /* Win32 version value */

  /* Calculate size of image */
  virtual_addr = SECTION_ALIGNMENT; /* Start after headers */
  if (s->text_section && s->text_section->data_size > 0) {
    virtual_addr +=
        align_up((uint32_t)s->text_section->data_size, SECTION_ALIGNMENT);
  }
  if (s->data_section && s->data_section->data_size > 0) {
    virtual_addr +=
        align_up((uint32_t)s->data_section->data_size, SECTION_ALIGNMENT);
  }
  if (s->rdata_section && s->rdata_section->data_size > 0) {
    virtual_addr +=
        align_up((uint32_t)s->rdata_section->data_size, SECTION_ALIGNMENT);
  }
  if (s->bss_section && s->bss_section->data_size > 0) {
    virtual_addr +=
        align_up((uint32_t)s->bss_section->data_size, SECTION_ALIGNMENT);
  }

  write_u32(header + 0xd0, virtual_addr);                /* Size of image */
  write_u32(header + 0xd4, PE_HEADER_SIZE);              /* Size of headers */
  write_u32(header + 0xd8, 0);                           /* Checksum */
  write_u16(header + 0xdc, IMAGE_SUBSYSTEM_WINDOWS_CUI); /* Console app */
  write_u16(header + 0xde, 0x8160);   /* DLL characteristics (NX, ASLR, etc.) */
  write_u64(header + 0xe0, 0x100000); /* Stack reserve */
  write_u64(header + 0xe8, 0x1000);   /* Stack commit */
  write_u64(header + 0xf0, 0x100000); /* Heap reserve */
  write_u64(header + 0xf8, 0x1000);   /* Heap commit */
  write_u32(header + 0x100, 0);       /* Loader flags */
  write_u32(header + 0x104, 16);      /* Number of data directories */

  /* Data directories (16 entries, 8 bytes each = 128 bytes) */
  /* All zeros for now (no imports/exports) */

  /* Section headers start at offset 0x188 */
  int section_offset = 0x188;
  file_offset = PE_HEADER_SIZE;
  virtual_addr = SECTION_ALIGNMENT;

  /* .text section */
  if (s->text_section && s->text_section->data_size > 0) {
    memcpy(header + section_offset, ".text\0\0\0", 8);
    write_u32(header + section_offset + 8,
              (uint32_t)s->text_section->data_size);
    write_u32(header + section_offset + 12, virtual_addr);
    write_u32(header + section_offset + 16,
              align_up((uint32_t)s->text_section->data_size, FILE_ALIGNMENT));
    write_u32(header + section_offset + 20, file_offset);
    write_u32(header + section_offset + 36,
              IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);

    s->text_section->sh_addr = virtual_addr;
    file_offset +=
        align_up((uint32_t)s->text_section->data_size, FILE_ALIGNMENT);
    virtual_addr +=
        align_up((uint32_t)s->text_section->data_size, SECTION_ALIGNMENT);
    section_offset += 40;
  }

  /* .data section */
  if (s->data_section && s->data_section->data_size > 0) {
    memcpy(header + section_offset, ".data\0\0\0", 8);
    write_u32(header + section_offset + 8,
              (uint32_t)s->data_section->data_size);
    write_u32(header + section_offset + 12, virtual_addr);
    write_u32(header + section_offset + 16,
              align_up((uint32_t)s->data_section->data_size, FILE_ALIGNMENT));
    write_u32(header + section_offset + 20, file_offset);
    write_u32(header + section_offset + 36, IMAGE_SCN_CNT_INITIALIZED_DATA |
                                                IMAGE_SCN_MEM_READ |
                                                IMAGE_SCN_MEM_WRITE);

    s->data_section->sh_addr = virtual_addr;
    file_offset +=
        align_up((uint32_t)s->data_section->data_size, FILE_ALIGNMENT);
    virtual_addr +=
        align_up((uint32_t)s->data_section->data_size, SECTION_ALIGNMENT);
    section_offset += 40;
  }

  /* .rdata section */
  if (s->rdata_section && s->rdata_section->data_size > 0) {
    memcpy(header + section_offset, ".rdata\0\0", 8);
    write_u32(header + section_offset + 8,
              (uint32_t)s->rdata_section->data_size);
    write_u32(header + section_offset + 12, virtual_addr);
    write_u32(header + section_offset + 16,
              align_up((uint32_t)s->rdata_section->data_size, FILE_ALIGNMENT));
    write_u32(header + section_offset + 20, file_offset);
    write_u32(header + section_offset + 36,
              IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ);

    s->rdata_section->sh_addr = virtual_addr;
    file_offset +=
        align_up((uint32_t)s->rdata_section->data_size, FILE_ALIGNMENT);
    virtual_addr +=
        align_up((uint32_t)s->rdata_section->data_size, SECTION_ALIGNMENT);
    section_offset += 40;
  }

  /* Write the file */
  f = fopen(filename, "wb");
  if (!f) {
    tcc_error(s, "cannot create output file '%s'", filename);
    return -1;
  }

  /* Write headers */
  fwrite(header, 1, PE_HEADER_SIZE, f);

  /* Write sections */
  if (s->text_section && s->text_section->data_size > 0) {
    fwrite(s->text_section->data, 1, s->text_section->data_size, f);
    /* Pad to file alignment */
    uint32_t padding =
        align_up((uint32_t)s->text_section->data_size, FILE_ALIGNMENT) -
        (uint32_t)s->text_section->data_size;
    if (padding > 0) {
      uint8_t *pad = tcc_malloc(padding);
      memset(pad, 0, padding);
      fwrite(pad, 1, padding, f);
      tcc_free(pad);
    }
  }

  if (s->data_section && s->data_section->data_size > 0) {
    fwrite(s->data_section->data, 1, s->data_section->data_size, f);
    uint32_t padding =
        align_up((uint32_t)s->data_section->data_size, FILE_ALIGNMENT) -
        (uint32_t)s->data_section->data_size;
    if (padding > 0) {
      uint8_t *pad = tcc_malloc(padding);
      memset(pad, 0, padding);
      fwrite(pad, 1, padding, f);
      tcc_free(pad);
    }
  }

  if (s->rdata_section && s->rdata_section->data_size > 0) {
    fwrite(s->rdata_section->data, 1, s->rdata_section->data_size, f);
    uint32_t padding =
        align_up((uint32_t)s->rdata_section->data_size, FILE_ALIGNMENT) -
        (uint32_t)s->rdata_section->data_size;
    if (padding > 0) {
      uint8_t *pad = tcc_malloc(padding);
      memset(pad, 0, padding);
      fwrite(pad, 1, padding, f);
      tcc_free(pad);
    }
  }

  fclose(f);

  if (s->verbose) {
    printf("PE file created: %s\n", filename);
    printf("  Code size: %zu bytes\n",
           s->text_section ? s->text_section->data_size : 0);
  }

  return 0;
}
