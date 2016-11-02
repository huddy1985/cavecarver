#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#define PERLDLL
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/*EXTERN_C void xs_init(pTHX);*/
#ifndef SHINTER_PERL5LIB
#define SHINTER_PERL5LIB "."
#endif

#define DO_NOTHING
#undef DO_NOTHING

static char **const_to_char(const char **a) {
    int i; char **c = 0;
    for(i = 0; a[i]; i++) ;
    /* using libc malloc, perl's malloc not available yet */
    c = (char **) /*libc_*/malloc((i+1) * sizeof(char *));
    for(i = 0; a[i]; i++)
        c[i] = strdup(a[i]);
    c[i] = 0;
    return c;
}

static void rel_const_to_char(char **a) {
    int i;
    for(i = 0; a[i]; i++)
        /*libc_*/free(a[i]);
    /*libc_*/free(a);
}

PerlInterpreter *my_perl;
#if defined(DOSISH) || defined(__SYMBIAN32__)
#    define PERLLIB_SEP ';'
#else
#  if defined(VMS)
#    define PERLLIB_SEP '|'
#  else
#    define PERLLIB_SEP ':'
#  endif
#endif

/* Do not delete this line--writemain depends on it */
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void boot_B (pTHX_ CV* cv);
EXTERN_C void boot_Compress__Raw__Bzip2 (pTHX_ CV* cv);
EXTERN_C void boot_Compress__Raw__Zlib (pTHX_ CV* cv);
EXTERN_C void boot_Cwd (pTHX_ CV* cv);
EXTERN_C void boot_Data__Dumper (pTHX_ CV* cv);
EXTERN_C void boot_Devel__DProf (pTHX_ CV* cv);
EXTERN_C void boot_Devel__PPPort (pTHX_ CV* cv);
EXTERN_C void boot_Devel__Peek (pTHX_ CV* cv);
EXTERN_C void boot_Digest__MD5 (pTHX_ CV* cv);
EXTERN_C void boot_Digest__SHA (pTHX_ CV* cv);
EXTERN_C void boot_Encode (pTHX_ CV* cv);
EXTERN_C void boot_Fcntl (pTHX_ CV* cv);
EXTERN_C void boot_File__Glob (pTHX_ CV* cv);
EXTERN_C void boot_Filter__Util__Call (pTHX_ CV* cv);
EXTERN_C void boot_Hash__Util (pTHX_ CV* cv);
EXTERN_C void boot_Hash__Util__FieldHash (pTHX_ CV* cv);
EXTERN_C void boot_I18N__Langinfo (pTHX_ CV* cv);
EXTERN_C void boot_IO (pTHX_ CV* cv);
EXTERN_C void boot_IPC__SysV (pTHX_ CV* cv);
EXTERN_C void boot_List__Util (pTHX_ CV* cv);
EXTERN_C void boot_MIME__Base64 (pTHX_ CV* cv);
EXTERN_C void boot_Math__BigInt__FastCalc (pTHX_ CV* cv);
EXTERN_C void boot_Opcode (pTHX_ CV* cv);
EXTERN_C void boot_POSIX (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__encoding (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__scalar (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__via (pTHX_ CV* cv);
EXTERN_C void boot_SDBM_File (pTHX_ CV* cv);
EXTERN_C void boot_Socket (pTHX_ CV* cv);
EXTERN_C void boot_Storable (pTHX_ CV* cv);
EXTERN_C void boot_Sys__Hostname (pTHX_ CV* cv);
EXTERN_C void boot_Sys__Syslog (pTHX_ CV* cv);
EXTERN_C void boot_Text__Soundex (pTHX_ CV* cv);
EXTERN_C void boot_Time__HiRes (pTHX_ CV* cv);
EXTERN_C void boot_Time__Piece (pTHX_ CV* cv);
EXTERN_C void boot_Unicode__Normalize (pTHX_ CV* cv);
EXTERN_C void boot_attributes (pTHX_ CV* cv);
EXTERN_C void boot_mro (pTHX_ CV* cv);
EXTERN_C void boot_re (pTHX_ CV* cv);
EXTERN_C void boot_threads (pTHX_ CV* cv);
EXTERN_C void boot_threads__shared (pTHX_ CV* cv);

static void
xs_init(pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
        newXS("B::bootstrap", boot_B, file);
        newXS("Compress::Raw::Bzip2::bootstrap", boot_Compress__Raw__Bzip2, file);
        newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
        newXS("Cwd::bootstrap", boot_Cwd, file);
        newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
        //newXS("Devel::DProf::bootstrap", boot_Devel__DProf, file);
        newXS("Devel::PPPort::bootstrap", boot_Devel__PPPort, file);
        newXS("Devel::Peek::bootstrap", boot_Devel__Peek, file);
        newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
        newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
        newXS("Encode::bootstrap", boot_Encode, file);
        newXS("Fcntl::bootstrap", boot_Fcntl, file);
        newXS("File::Glob::bootstrap", boot_File__Glob, file);
        newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
        newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
        newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file);
        newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
        newXS("IO::bootstrap", boot_IO, file);
        newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
        newXS("List::Util::bootstrap", boot_List__Util, file);
        newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
        newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
        newXS("Opcode::bootstrap", boot_Opcode, file);
        newXS("POSIX::bootstrap", boot_POSIX, file);
        newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
        newXS("PerlIO::scalar::bootstrap", boot_PerlIO__scalar, file);
        newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
        newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
        newXS("Socket::bootstrap", boot_Socket, file);
        newXS("Storable::bootstrap", boot_Storable, file);
        newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
        newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
        //newXS("Text::Soundex::bootstrap", boot_Text__Soundex, file);
        newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
        newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
        //newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
        newXS("attributes::bootstrap", boot_attributes, file);
        newXS("mro::bootstrap", boot_mro, file);
        newXS("re::bootstrap", boot_re, file);
        newXS("threads::bootstrap", boot_threads, file);
        newXS("threads::shared::bootstrap", boot_threads__shared, file);
}

static void  __attribute__ ((constructor)) perl_exec_start(void) {

    XSINIT_t f = xs_init;
    int exitstatus, argc = 1; const char *p = 0, *b = SHINTER_PERL5LIB; char *pb = 0;
    const char *argv_[2] = {"test",0}; char **argv = 0;
    const char *embedding_[] = { "", "-e", "0", 0 }; char **embedding = 0;
    argv = const_to_char(argv_);
    embedding = const_to_char(embedding_);

    PERL_SYS_INIT(&argc,&argv);
    if (!(my_perl = perl_alloc()))
        goto out;

    /* add path to Lang/e during interpreter initialization */
    p = PerlEnv_getenv("PERL5LIB");
    pb = (char *)malloc((p ? strlen(p) : 0) + MAXPATHLEN + 2);
    if (p)
        sprintf(pb,"PERL5LIB=%s%c%s",p,PERLLIB_SEP,b);
    else
        sprintf(pb,"PERL5LIB=%s",b);
    /*printf("Set env:\"%s\"\n",pb);*/
    PerlEnv_putenv(pb);

    perl_construct(my_perl);
    PL_perl_destruct_level = 0;
    /* init PL_defstash */
    perl_parse(my_perl, f, 3, embedding, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_run(my_perl);

    /* reset back */
    sprintf(pb,"PERL5LIB=%s",p ? p : "");
    PerlEnv_putenv(pb);
    free(pb);
 out:
    rel_const_to_char(argv);
    rel_const_to_char(embedding);
}

int execv (const char *filename, char *const argv[]) {
    SV *sv;
    static int (*func)(const char *, char **) = 0;
    if (!func)
        func = dlsym (RTLD_NEXT, "execv");

    eval_pv( "use shpreload;", TRUE);
#ifndef DO_NOTHING
    {
        dSP;
        int i; AV*a;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);

        XPUSHs(sv_2mortal(newSVpv(filename, 0)));
        a =  newAV();
        for (i=0; argv[i]; i++) {
            av_push(a, newSVpv(argv[i], 0));
        }
        XPUSHs(sv_2mortal(newRV((SV *)a)));

        PUTBACK;
        call_pv("shpreload::execv", G_DISCARD);
        FREETMPS;
        LEAVE;

    }
#endif

    return (*func) (filename, (char **) argv);
}

int execve (const char *filename, char *const argv[], char *const envp[])
{
    SV *sv;
    static int (*func)(const char *, char **, char **) = 0;
    if (!func)
        func = dlsym (RTLD_NEXT, "execve");

    eval_pv( "use shpreload;", TRUE);
#ifndef DO_NOTHING
    {
        dSP;
        int i; AV *a, *e;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);

        XPUSHs(sv_2mortal(newSVpv(filename, 0)));
        a =  newAV();
        for (i=0; argv[i]; i++) {
            av_push(a, newSVpv(argv[i], 0));
        }
        XPUSHs(sv_2mortal(newRV((SV *)a)));
        e =  newAV();
        for (i=0; envp[i]; i++) {
            av_push(e, newSVpv(envp[i], 0));
        }
        XPUSHs(sv_2mortal(newRV((SV *)e)));

        PUTBACK;
        call_pv("shpreload::execve", G_DISCARD);
        FREETMPS;
        LEAVE;

    }
#endif

    return (*func) (filename, (char**) argv, (char **) envp);
}

/*
Local Variables:
c-basic-offset:4
indent-tabs-mode:nil
End:
*/
