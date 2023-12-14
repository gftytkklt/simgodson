/* 
 * elf.h - SimpleScalar ELF definitions
 * written by fenghao 2004/4/4
 */
 
/* SimpleScalar ELF definitions */

#ifndef ELF_H
#define ELF_H   

#define EI_NIDENT 16

typedef struct
{
        byte_t e_ident[EI_NIDENT];
        half_t e_type;
        half_t e_machine;
        word_t e_version;
        word_t e_entry;
        word_t e_phoff;
        word_t e_shoff;
        word_t e_flags;
        half_t e_ehsize;
        half_t e_phentisize;
        half_t e_phnum;
        half_t e_shentsize;
        half_t e_shnum;
        half_t e_shstrndx;
}Elf32_Ehdr;

typedef struct
{
        word_t          p_type;
        word_t          p_offset;
        word_t		p_vaddr;
        word_t 	        p_paddr;
        word_t 		p_filesz;
        word_t 	       	p_memsz;
        word_t 		p_flags;
        word_t 		p_align;
}Elf32_Phdr;

typedef struct
{
        word_t 		sh_name;
        word_t 		sh_type;
        word_t 		sh_flags;
        word_t	 	sh_addr;
        word_t		sh_offset;
        word_t 		sh_size;
        word_t 		sh_link;
        word_t		sh_info;
        word_t		sh_addralign;
        word_t		sh_entsize; 
}Elf32_Shdr;

/* values of e_ident */
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4     
#define EI_DATA         5
#define EI_VERSION      6
#define EI_PAD          7
#define EI_NIDENT       16

#define ELFMAG0         0X7f

#define ELFMAG1         'E'

#define ELFMAG2         'L'

#define ELFMAG3         'F'

#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2

#define ELFDATANONE     0
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2

/* values of e_type */
#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4
#define ET_LOPROC       0Xff00
#define ET_HIPROC       0Xffff


/* values of e_version */
#define EV_NONE         0
#define EV_CURRENT      1


/* values of p_type */
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_LOPROC       0X70000000
#define PT_HIPROC       0X7fffffff


/* values of p_flags */
#define PF_X            0X1
#define PF_W            0X2
#define PF_R            0X4
#define PF_MASKPROC     0Xf0000000


/* values of sh_type */
#define SHT_NULL	0
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11
#define SHT_LOPROC	0X70000000
#define SHT_HIPROC	0X7fffffff
#define SHT_LOUSER	0X80000000
#define SHT_HIUSER	0Xffffffff


/* values of sh_flags */
#define SHT_WRITE	0X1
#define SHT_ALLOC	0X2
#define SHT_EXECINSTR	0X4
#define SHT_MASKPROC	0Xf0000000





#endif
