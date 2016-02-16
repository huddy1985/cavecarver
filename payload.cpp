
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

            /* patch .text here */

        } else if (sh->sh_type == SHT_GNU_versym) {

            printf("Cave at 0x%x size 0x%x\n", sh->sh_offset, sh->sh_size);
            memset(&buf[sh->sh_offset], 0x90, sh->sh_size);
            insertion_point = sh->sh_offset;
            sh->sh_type = SHT_PROGBITS;
            sh->sh_flags |= SHF_EXECINSTR;

            FILE*f = fopen("cavefile.bin", "rb");
            if (f) {
                int r = fread(&buf[sh->sh_offset],1,sh->sh_size,f);
                printf("Filled with %d bytes\n", r);
                fclose(f);
            }


        } else if (sh->sh_type == SHT_DYNAMIC) {
            PELF_DYN dyn = (PELF_DYN )&buf[sh->sh_offset];
            int nelm = sh->sh_size/sizeof(ELF_DYN);
            int k = 0;
            for (int j = 0; j < nelm; j++,k++) {
                switch(dyn[j].d_tag ) {
                case DT_VERNEED:
                case DT_VERNEEDNUM:
                case DT_VERSYM:
                    k--;
                    break;
                default:
                    if (k != j)
                        dyn[k] = dyn[j];
                    break;
                }
            }
            for (;k < nelm; k++) {
                dyn[k].d_tag = 0;
            }
        }
    }


    printf("After paylout insertion AT 0x%016x:\n", insertion_point);
    printf("shoff: 0x%016x, shidx:%d\n", hdr->e_shoff,hdr->e_shstrndx);
    printf("phoff: 0x%016x\n", hdr->e_phoff);

    //PELF_PHDR phdr = (PELF_PHDR) &buf[ hdr->e_phoff + 0 * hdr->e_phentsize ];
    //phdr->p_flags |= PF_W;

    DumpSections(0);

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
