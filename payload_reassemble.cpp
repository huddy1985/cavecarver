
int CElfHlp::DumpSections(DWORD df_flags)
{
    // check file format
    PELF_SH shtext = 0;
    int payload = PAGE_SIZE;

    if (len < 1024)
        return result = ERR_TOOSMALLFILE;

    hdr = (PELF_HEADER) buf;

    if ( (hdr->e_ident[EI_MAG0]    != 0x7F       ) ||
         (hdr->e_ident[EI_MAG1]    != 'E'        ) ||
         (hdr->e_ident[EI_MAG2]    != 'L'        ) ||
         (hdr->e_ident[EI_MAG3]    != 'F'        ) ||
         (hdr->e_ident[EI_CLASS]   != ELFCLASS32 && hdr->e_ident[EI_CLASS]   != ELFCLASS64 ) ||
         (hdr->e_ident[EI_DATA]    != ELFDATA2LSB) ||
         (hdr->e_ident[EI_VERSION] != EV_CURRENT ) ||
         (hdr->e_type              != ET_EXEC && hdr->e_type              != ET_DYN    ) ||
         (hdr->e_machine           != EM_386  && hdr->e_machine           != EM_X86_64     ) ||

         (hdr->e_version           != EV_CURRENT ) ||
         (hdr->e_flags             != 0          ) )
        return result = ERR_BADFILEFORMAT;

    // for each section
    for(int i=0; i<hdr->e_shnum; i++)
    {
        // make ptr to section header
        PELF_SH sh = (PELF_SH) &buf[ hdr->e_shoff + i * hdr->e_shentsize ];
        char *n = (char*)&buf[ ((PELF_SH)&buf[ hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize ])->sh_offset + sh->sh_name ];

        if (OPT_DUMP) {
            printf("%02d:[%18s](%08x)@[%016llx.%016llx] %04llx\n", i,

                   (char*)&buf[ ((PELF_SH)&buf[ hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize ])->sh_offset + sh->sh_name ],
                   sh->sh_type,
                   sh->sh_offset,
                   sh->sh_size,
                   sh->sh_addralign

                );
        }
    }

    printf("              vaddr            paddr             fileoff          filesz\n");
    for(int i=0; i<hdr->e_phnum; i++)
    {
        // make ptr to section header
        PELF_PHDR phdr = (PELF_PHDR) &buf[ hdr->e_phoff + i * hdr->e_phentsize ];

        printf("%02d:(%08x) %016llx:%016llx [%016llx.%016llx] %06llx %016llx\n", i, phdr->p_type,
               phdr->p_vaddr, phdr->p_paddr,
               phdr->p_offset, phdr->p_filesz, phdr->p_align-1,
               phdr->p_vaddr - phdr->p_offset

            );

    }

    return 0;
}

int CElfHlp::payload(DWORD df_flags)
{
    // check file format
    PELF_SH shtext = 0;
    int payload = PAGE_SIZE;
    Elf64_Addr insertion_point = 0;

    if (len < 1024)
        return result = ERR_TOOSMALLFILE;

    hdr = (PELF_HEADER) buf;

    if ( (hdr->e_ident[EI_MAG0]    != 0x7F       ) ||
         (hdr->e_ident[EI_MAG1]    != 'E'        ) ||
         (hdr->e_ident[EI_MAG2]    != 'L'        ) ||
         (hdr->e_ident[EI_MAG3]    != 'F'        ) ||
         (hdr->e_ident[EI_CLASS]   != ELFCLASS32 && hdr->e_ident[EI_CLASS]   != ELFCLASS64 ) ||
         (hdr->e_ident[EI_DATA]    != ELFDATA2LSB) ||
         (hdr->e_ident[EI_VERSION] != EV_CURRENT ) ||
         (hdr->e_type              != ET_EXEC && hdr->e_type              != ET_DYN    ) ||
         (hdr->e_machine           != EM_X86_64     ) ||

         (hdr->e_version           != EV_CURRENT ) ||
         (hdr->e_flags             != 0          ) )
        return result = ERR_BADFILEFORMAT;

    printf("shoff: 0x%016x, shidx:%d\n", hdr->e_shoff,hdr->e_shstrndx);
    printf("phoff: 0x%016x\n", hdr->e_phoff);
    DumpSections(0);

    PELF_SH sh6 = (PELF_SH) &buf[ hdr->e_shoff + 6 * hdr->e_shentsize ];
    PELF_SH sh20 = (PELF_SH) &buf[ hdr->e_shoff + 20 * hdr->e_shentsize ];

    for(int i=0; i<hdr->e_shnum; i++)
    {
        PELF_SH sh = (PELF_SH) &buf[ hdr->e_shoff + i * hdr->e_shentsize ];
        char *n = (char*)&buf[ ((PELF_SH)&buf[ hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize ])->sh_offset + sh->sh_name ];

        if (strcmp(n, ".text") == 0) {

            unsigned char *b = &buf[sh->sh_offset];
            /* patch .text here */
            //printf("%02x %02x\n", buf[0xf7580],buf[0xf7580+1]);
        }
    }

    unsigned int *flags = (unsigned int *)malloc(len*sizeof(int));
    unsigned int *off = (unsigned int *)malloc(len*sizeof(int));
    memset(flags, 0, len*sizeof(int));
    memset(off, 0, len*sizeof(int));

    unsigned int ip = 0xf7580;

    while (1) {
        disasm64_struct diza;
        int op_len = ade64_disasm(&buf[ip], &diza);
        if (op_len == 0)
        {
            printf("break: disasm error @0x%x\n",ip);
            break;                      // disasm error
        }
        if (OPT_DUMP)
        {
            printf("%08lx", ip);
            for(DWORD t=0; t<op_len; t++)
                printf(" %02X", buf[ip+t]);
            printf("\n");
        }

        DWORD nxt = ip + op_len;    // normal way
        DWORD rel = NONE;           // jmp,call,jxx-way

        if (diza.disasm_flag & C_REL)  // have relative arg?
        {
            if (diza.disasm_datasize == 1) // jcc,jcxz,loop/z/nz,jmps
                rel = OFFS_PLUS_REL(nxt, diza.disasm_data_c[0]);
            if (diza.disasm_datasize == 4) // jcc near,call,jmp
            {
                //rel = OFFS_PLUS_REL(nxt, diza.disasm_data_l[0]);
                rel = ((long)nxt) + (long)diza.disasm_data_l[0];
                rel = rel & 0xffffffff;
                printf(" Rel:0x%lx+%d=>0x%lx\n", nxt, diza.disasm_data_l[0],rel);
            }
        }
        // ret/ret#/retf/retf#/iret/jmp modrm ?
        if (diza.disasm_flag & C_STOP)
            nxt = NONE;

        if ((nxt == NONE) && (rel != NONE))
            nxt = rel;

        if (nxt == NONE) {
            printf("break: STOP\n");
            break;
        }

        if (nxt >= len) {
            printf("break: nxt out of range (nx:\n0x%lx >= \n0x%lx)\n", nxt, len);
            break;                     // out of range?
        }
        ip = nxt;

    }


    return result = ERR_SUCCESS;
} // CElfHlp::DisasmELF



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
