// from http://vxheaven.org/lib/var00.html and http://vxheaven.org/lib/vzo08.html
// ELF "island" disassembling/infection engine

#define MIN(a,b)                ((a)<(b)?(a):(b))
#define MAX(a,b)                ((a)>(b)?(a):(b))

// globals

int OPT_DUMP = 0;               // debug printf
int OPT_DUMP_SYMS = 0;               // debug printf

// ELF header

#include "elf.h"

typedef elf64_hdr  ELF_HEADER, *PELF_HEADER;
typedef Elf64_Shdr ELF_SH, *PELF_SH;
typedef Elf64_Sym  ELF_SYM, *PELF_SYM;
typedef Elf64_Phdr ELF_PHDR, *PELF_PHDR;
typedef Elf64_Rela ELF_RELA, *PELF_RELA;
typedef Elf64_Rel  ELF_REL, *PELF_REL;
typedef Elf64_Dyn  ELF_DYN, *PELF_DYN;


#define IS_SPECIAL_SHN(n)      ( ((n) == SHN_UNDEF) || ((n) >= SHN_LORESERVE) )

// disassembler

#define PAGE_SIZE 4096

//#include "ade32.cpp"
#include "ade64.cpp"

// error codes

#define ERR_SUCCESS             0
#define ERR_CANTOPENFILE        1
#define ERR_CANTCREATEFILE      2
#define ERR_CANTREADFILE        3
#define ERR_CANTWRITEFILE       4
#define ERR_NOMEMORY            5
#define ERR_BADFILEFORMAT       6
#define ERR_DISASM              7
#define ERR_NOSPACE             8
#define ERR_NOTSUPP             9
#define ERR_INTERNAL1           10
#define ERR_BADHOOKOFFS         11
#define ERR_NOTOPCODEATHOOK     12
#define ERR_NOORIGBYTESSIGN     13
#define ERR_JMPRETATHOOK        14
#define ERR_INTERNAL2           15
#define ERR_INTERNAL3           16
#define ERR_INTERNAL4           17
#define ERR_INTERNAL5           18
#define ERR_INTERNAL6           19
#define ERR_TOOSMALLFILE        20

char* ElfErr[] =
{
    "OK",
    "cant open input file",
    "cant create output file",
    "cant read file",
    "cant write file",
    "not enough memory",
    "bad file format",
    "error while disasm",
    "not enough free space in the target file",
    "snippet: loop/loopz/loopnz/jcxz are not supported yet",
    "internal error 1",
    "invalid hook offset",
    "not an opcode at hook offset",
    "no $ORIGINAL_BYTES$ signature within code snippet",
    "jmp/ret-alike opcode at hook offset",
    "internal error 2",
    "internal error 3",
    "internal error 4",
    "internal error 5",
    "internal error 6",
    "input file is too small"
};

// Disasm() flags

#define DF_JMPTAB               0x00000001      // find jmp tables
#define DF_RELREF               0x00000002      // find relative references
#define DF_FUNC                 0x00000004      // push ebp/mov ebp, esp
#define DF_SYM                  0x00000008      // process symbols
#define DF_GOT                  0x00000010      // process .got entries

// Inject() flags

#define IN_KILLNOP              0x00000001      // remove NOP's in the snippet
#define IN_BIGFIRST             0x00000002      // 1st try bigger islands

// internal flags & etc.

#define FL_LABEL                0x00000001      // label (i.e. has xref)
#define FL_OPCODE               0x00000002      // 1st opcode byte
#define FL_CODE                 0x00000004      // any opcode byte
#define FL_OPCODE_END           0x00000008
#define FL_EXEC                 0x00000010      // section attr: executable
#define FL_NEXT                 0x00000020      // marked for analysis
#define FL_ANALYZED             0x00000040      // analysed
#define FL_FREE_START           0x00001000      // start of free area
// ^^^ associated arg1 = size of free area
#define FL_FREE                 0x00002000      // any byte of free area
#define FL_INJECTED_START       0x00004000      // start of injected area
// ^^^ associated arg1 = 0-based offset within code snippet
#define FL_INJECTED             0x00008000      // any byte of injected area
#define FL_FIXREL               0x00010000      // rel to fix (temp flag)

#define NONE                    0xFFFFFFFF
#define OFFS_PLUS_REL(offs,rel) (va2offs(offs2va(offs)+rel))
#define MAX_ISLAND_LEN          1024

class CElfHlp
{
public:

    int result;                   // here is stored ERR_xxx after any method call
    DWORD MaxIslandLen;           // default=15
    BYTE* buf;                    // target file itself
    DWORD len;                    // length
    DWORD* flag;                  // FL_xxx
    DWORD* arg1;                  // used in infection
    PELF_HEADER hdr;            // ptr to ELF header at buf[0]

    CElfHlp();
    ~CElfHlp();
    void Empty();                 // free allocated buffers, zerofill variables

    int Load(char* infile);       // load file into memory, returns ERR_xxx
    int Save(char* outfile, DWORD fl, int c1, int c2);  // returns ERR_xxx
    int SaveText(char* outfile);
    int DumpSections(DWORD df_flags);
    int payload(DWORD df_flags);// disasm ELF file, returns ERR_xxx
    char *strtab(PELF_SH sh, int off);
    int Inject(CElfHlp* S, DWORD hook_offs, DWORD in_flags);
    DWORD GetEntryOffs();         // get entrypoint offset
    DWORD GetFuncOffs(char* func);// get func() offset -- look into symbols

    DWORD offs2va(DWORD offs);    // returns VA or NONE
    DWORD va2offs(DWORD va);      // returns OFFSET or NONE
    PELF_SH va2sh(DWORD va);    // returns PELF_SH or NULL

}; // class CElfHlp

CElfHlp::CElfHlp()
{
    buf = NULL;
    flag = NULL;
    arg1 = NULL;
    Empty();
}//CElfHlp::CElfHlp

CElfHlp::~CElfHlp()
{
    Empty();
}//CElfHlp::~CElfHlp

void CElfHlp::Empty()
{
    if (buf != NULL) delete buf;
    if (flag != NULL) delete flag;
    if (arg1 != NULL) delete arg1;
    buf = NULL;
    len = 0;
    flag = NULL;
    hdr = NULL;
    MaxIslandLen = 15;
} // CElfHlp::Empty

// load file into memory

int CElfHlp::Load(char* infile)
{
    Empty();

    FILE*f = fopen(infile, "rb");
    if (f == NULL)
        return result = ERR_CANTOPENFILE;

    // alloc/init buffer(s)

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);
    buf = new BYTE[ len*2+1 ];
    if (buf == NULL)
    {
        fclose(f);
        return result = ERR_NOMEMORY;
    }

    flag = new DWORD[ len*2+1 ];
    if (flag == NULL)
    {
        fclose(f);
        return result = ERR_NOMEMORY;
    }
    memset(flag, 0x00, (len*2+1)*4);

    if (fread(buf,1,len,f) != len)
    {
        fclose(f);
        return result = ERR_CANTREADFILE;
    }
    fclose(f);

    return result = ERR_SUCCESS;
} // CElfHlp::Load

// save file to disk
// fl == 0 -- normal mode
// fl == FL_xxx -- replace bytes marked with fl with c1, other with c2

int CElfHlp::Save(char* outfile, DWORD fl, int c1, int c2)
{
    BYTE* temp = buf;

    if (fl != 0)
    {
        temp = new BYTE[ len+1 ];
        if (temp == NULL)
            return result = ERR_NOMEMORY;

        for(DWORD i=0; i<len; i++)
        {
            if (flag[i] & fl)
                temp[i] = c1 == -1 ? buf[i] : c1;
            else
                temp[i] = c2 == -1 ? buf[i] : c2;
        }

    } // fl != 0

    FILE*f=fopen(outfile, "wb");
    if (f==NULL)
    {
        if (temp != buf)
            delete temp;
        return result = ERR_CANTCREATEFILE;
    }
    if (fwrite(temp,1,len,f)!=len)
    {
        fclose(f);
        if (temp != buf)
            delete temp;
        return result = ERR_CANTWRITEFILE;
    }
    fclose(f);

    if (temp != buf)
        delete temp;

    return result = ERR_SUCCESS;
} // CElfHlp::Save

// dump data/flags as text file

int CElfHlp::SaveText(char* outfile)
{
    FILE*f=fopen(outfile, "wb");
    if (f==NULL)
        return result = ERR_CANTCREATEFILE;
    for(DWORD i=0; i<len; )
    {
        if (flag[i] & FL_LABEL)
            fprintf(f, "[LABEL]\n");
        if (hdr == 0)
            fprintf(f, "%08X ", i);
        else
            fprintf(f, "%08X/.%08X ", i, offs2va(i));
        fprintf(f, "%s%s%s%s ",
                flag[i] & FL_EXEC     ? "E" : " ",
                flag[i] & FL_CODE     ? "C" : " ",
                flag[i] & (FL_INJECTED|FL_INJECTED_START) ? "I" : " ",
                flag[i] & (FL_FREE    |FL_FREE_START    ) ? "F" : " " );
        DWORD op_len = 1;
        if (flag[i] & FL_OPCODE)
            op_len = ade64_disasm(&buf[i], NULL);
        for(DWORD t=0; t<op_len; t++)
        {
            fprintf(f, " %02X", buf[i+t]);
            if (arg1 != 0)
                if (arg1[i+t] != 0)
                    fprintf(f, "<%08X>", arg1[i+t]);
        }
        fprintf(f, "\n");
        i += op_len;
    }
    fprintf(f, "[EOF]\n");
    return result = ERR_SUCCESS;
} // CElfHlp::SaveText

DWORD CElfHlp::offs2va(DWORD offs)
{
    for(int i=0; i<hdr->e_shnum; i++)
    {
        PELF_SH sh = (PELF_SH) &buf[ hdr->e_shoff + i * hdr->e_shentsize ];
        if (sh->sh_offset != 0)
            if (sh->sh_addr   != 0)
                if ((offs >= sh->sh_offset) && (offs < sh->sh_offset + sh->sh_size))
                    return offs - sh->sh_offset + sh->sh_addr;
    }
    return NONE;
}//CElfHlp::offs2va

DWORD CElfHlp::va2offs(DWORD va)
{
    PELF_SH sh = va2sh(va);
    if ((sh == NULL) || (sh->sh_type == SHT_NOBITS))
        return NONE;
    else
        return va - sh->sh_addr + sh->sh_offset;
}//CElfHlp::va2offs

PELF_SH CElfHlp::va2sh(DWORD va)
{
    for(int i=0; i<hdr->e_shnum; i++)
    {
        PELF_SH sh = (PELF_SH) &buf[ hdr->e_shoff + i * hdr->e_shentsize ];
        if (sh->sh_offset != 0)
            if (sh->sh_addr   != 0)
                if ((va >= sh->sh_addr) && (va < sh->sh_addr + sh->sh_size))
                    return sh;
    }
    return NULL;
}//CElfHlp::va2sh

char *CElfHlp::strtab(PELF_SH sh, int off) {
    return (char*)&buf[ ((PELF_SH)&buf[ hdr->e_shoff + sh->sh_link * hdr->e_shentsize ])->sh_offset + off ];
}

DWORD CElfHlp::GetEntryOffs()
{
    return va2offs(hdr->e_entry);
}//CElfHlp::GetEntryOffs

DWORD CElfHlp::GetFuncOffs(char* func)
{
    // for each section
    for(int i=0; i<hdr->e_shnum; i++)
    {
        // make ptr to section header
        PELF_SH sh = (PELF_SH) &buf[ hdr->e_shoff + i * hdr->e_shentsize ];
        // check if symbols
        if ( (sh->sh_type == SHT_SYMTAB) ||
             (sh->sh_type == SHT_DYNSYM) )
        {
            // for each symbol entry
            for(DWORD j=0; j<sh->sh_size/sh->sh_entsize; j++)
            {
                PELF_SYM st = (PELF_SYM) &buf[ sh->sh_offset + j * sh->sh_entsize ];
                if (ELF32_ST_TYPE(st->st_info) == STT_FUNC)
                    if (!strcmp(func, (char*)&buf[ ((PELF_SH)&buf[ hdr->e_shoff + sh->sh_link     * hdr->e_shentsize ])->sh_offset + st->st_name ]))
                        return va2offs(st->st_value);
            } // for j -- for each symbol
        } // SHT_SYMTAB || SHT_DYNSYM
    } // for i -- for each section
    return NONE;
} // CElfHlp::GetFuncOffs

/*
  Local Variables:
  compile-command:"gcc test_libc.c -o test_libc.exe"
  mode:c++
  c-basic-offset:4
  c-file-style:"bsd"
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
