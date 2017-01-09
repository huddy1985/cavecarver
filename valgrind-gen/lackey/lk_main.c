
/*--------------------------------------------------------------------*/
/*--- An example Valgrind tool.                          lk_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Lackey, an example Valgrind tool that does
   some simple program measurement and tracing.

   Copyright (C) 2002-2015 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

// This tool shows how to do some basic instrumentation.
//
// There are four kinds of instrumentation it can do.  They can be turned
// on/off independently with command line options:
//
// * --basic-counts   : do basic counts, eg. number of instructions
//                      executed, jumps executed, etc.
// * --detailed-counts: do more detailed counts:  number of loads, stores
//                      and ALU operations of different sizes.
// * --trace-mem=yes:   trace all (data) memory accesses.
// * --trace-superblocks=yes:
//                      trace all superblock entries.  Mostly of interest
//                      to the Valgrind developers.
//
// The code for each kind of instrumentation is guarded by a clo_* variable:
// clo_basic_counts, clo_detailed_counts, clo_trace_mem and clo_trace_sbs.
//
// If you want to modify any of the instrumentation code, look for the code
// that is guarded by the relevant clo_* variable (eg. clo_trace_mem)
// If you're not interested in the other kinds of instrumentation you can
// remove them.  If you want to do more complex modifications, please read
// VEX/pub/libvex_ir.h to understand the intermediate representation.
//
//
// Specific Details about --trace-mem=yes
// --------------------------------------
// Lackey's --trace-mem code is a good starting point for building Valgrind
// tools that act on memory loads and stores.  It also could be used as is,
// with its output used as input to a post-mortem processing step.  However,
// because memory traces can be very large, online analysis is generally
// better.
//
// It prints memory data access traces that look like this:
//
//   I  0023C790,2  # instruction read at 0x0023C790 of size 2
//   I  0023C792,5
//    S BE80199C,4  # data store at 0xBE80199C of size 4
//   I  0025242B,3
//    L BE801950,4  # data load at 0xBE801950 of size 4
//   I  0023D476,7
//    M 0025747C,1  # data modify at 0x0025747C of size 1
//   I  0023DC20,2
//    L 00254962,1
//    L BE801FB3,1
//   I  00252305,1
//    L 00254AEB,1
//    S 00257998,1
//
// Every instruction executed has an "instr" event representing it.
// Instructions that do memory accesses are followed by one or more "load",
// "store" or "modify" events.  Some instructions do more than one load or
// store, as in the last two examples in the above trace.
//
// Here are some examples of x86 instructions that do different combinations
// of loads, stores, and modifies.
//
//    Instruction          Memory accesses                  Event sequence
//    -----------          ---------------                  --------------
//    add %eax, %ebx       No loads or stores               instr
//
//    movl (%eax), %ebx    loads (%eax)                     instr, load
//
//    movl %eax, (%ebx)    stores (%ebx)                    instr, store
//
//    incl (%ecx)          modifies (%ecx)                  instr, modify
//
//    cmpsb                loads (%esi), loads(%edi)        instr, load, load
//
//    call*l (%edx)        loads (%edx), stores -4(%esp)    instr, load, store
//    pushl (%edx)         loads (%edx), stores -4(%esp)    instr, load, store
//    movsw                loads (%esi), stores (%edi)      instr, load, store
//
// Instructions using x86 "rep" prefixes are traced as if they are repeated
// N times.
//
// Lackey with --trace-mem gives good traces, but they are not perfect, for
// the following reasons:
//
// - It does not trace into the OS kernel, so system calls and other kernel
//   operations (eg. some scheduling and signal handling code) are ignored.
//
// - It could model loads and stores done at the system call boundary using
//   the pre_mem_read/post_mem_write events.  For example, if you call
//   fstat() you know that the passed in buffer has been written.  But it
//   currently does not do this.
//
// - Valgrind replaces some code (not much) with its own, notably parts of
//   code for scheduling operations and signal handling.  This code is not
//   traced.
//
// - There is no consideration of virtual-to-physical address mapping.
//   This may not matter for many purposes.
//
// - Valgrind modifies the instruction stream in some very minor ways.  For
//   example, on x86 the bts, btc, btr instructions are incorrectly
//   considered to always touch memory (this is a consequence of these
//   instructions being very difficult to simulate).
//
// - Valgrind tools layout memory differently to normal programs, so the
//   addresses you get will not be typical.  Thus Lackey (and all Valgrind
//   tools) is suitable for getting relative memory traces -- eg. if you
//   want to analyse locality of memory accesses -- but is not good if
//   absolute addresses are important.
//
// Despite all these warnings, Lackey's results should be good enough for a
// wide range of purposes.  For example, Cachegrind shares all the above
// shortcomings and it is still useful.
//
// For further inspiration, you should look at cachegrind/cg_main.c which
// uses the same basic technique for tracing memory accesses, but also groups
// events together for processing into twos and threes so that fewer C calls
// are made and things run faster.
//
// Specific Details about --trace-superblocks=yes
// ----------------------------------------------
// Valgrind splits code up into single entry, multiple exit blocks
// known as superblocks.  By itself, --trace-superblocks=yes just
// prints a message as each superblock is run:
//
//  SB 04013170
//  SB 04013177
//  SB 04013173
//  SB 04013177
//
// The hex number is the address of the first instruction in the
// superblock.  You can see the relationship more obviously if you use
// --trace-superblocks=yes and --trace-mem=yes together.  Then a "SB"
// message at address X is immediately followed by an "instr:" message
// for that address, as the first instruction in the block is
// executed, for example:
//
//  SB 04014073
//  I  04014073,3
//   L 7FEFFF7F8,8
//  I  04014076,4
//  I  0401407A,3
//  I  0401407D,3
//  I  04014080,3
//  I  04014083,6


#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"

#include "pub_tool_vki.h"           // keeps libcproc.h happy, syscall nums
#include "pub_tool_aspacemgr.h"     // VG_(am_shadow_alloc)
#include "pub_tool_debuginfo.h"     // VG_(get_fnname_w_offset), VG_(get_fnname)
#include "pub_tool_hashtable.h"     // For tnt_include.h, VgHashtable
#include "pub_tool_libcassert.h"    // tl_assert
#include "pub_tool_libcbase.h"      // VG_STREQN
#include "pub_tool_libcprint.h"     // VG_(message)
#include "pub_tool_libcproc.h"      // VG_(getenv)
#include "pub_tool_replacemalloc.h" // VG_(replacement_malloc_process_cmd_line_option)
#include "pub_tool_machine.h"       // VG_(get_IP)
#include "pub_tool_mallocfree.h"    // VG_(out_of_memory_NORETURN)
#include "pub_tool_options.h"       // VG_STR/BHEX/BINT_CLO
#include "pub_tool_oset.h"          // OSet operations
#include "pub_tool_threadstate.h"   // VG_(get_running_tid)
#include "pub_tool_xarray.h"        // VG_(*XA)
#include "pub_tool_stacktrace.h"    // VG_(get_and_pp_StackTrace)
#include "pub_tool_libcfile.h"      // VG_(readlink)
#include "pub_tool_addrinfo.h"      // VG_(describe_addr)


#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"     // VG_(fnptr_to_fnentry)

#include <sys/syscall.h>

#include "valgrind.h"
#include "distorm.h"
#include "copy.h"

#define LK_(str)    VGAPPEND(vgLackey_,str)

/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

/* Command line options controlling instrumentation kinds, as described at
 * the top of this file. */
static Bool clo_basic_counts    = True;
static Bool clo_detailed_counts = False;
static Bool clo_trace_mem       = False;
static Bool clo_trace_sbs       = False;
static Int  clo_trace_match     = False;

/* The name of the function of which the number of calls (under
 * --basic-counts=yes) is to be counted, with default. Override with command
 * line option --fnname. */
static const HChar* clo_fnname = "main";

static Bool lk_process_cmd_line_option(const HChar* arg)
{
   if VG_STR_CLO(arg, "--fnname", clo_fnname) {}
   else if VG_BOOL_CLO(arg, "--basic-counts",      clo_basic_counts) {}
   else if VG_BOOL_CLO(arg, "--detailed-counts",   clo_detailed_counts) {}
   else if VG_BOOL_CLO(arg, "--trace-mem",         clo_trace_mem) {}
   else if VG_BOOL_CLO(arg, "--trace-superblocks", clo_trace_sbs) {}
   else if VG_BHEX_CLO(arg, "--trace-match", clo_trace_match, 0x0000, 0xffffffff) {}
   else
      return False;

   tl_assert(clo_fnname);
   tl_assert(clo_fnname[0]);
   return True;
}

static void lk_print_usage(void)
{
   VG_(printf)(
"    --basic-counts=no|yes     count instructions, jumps, etc. [yes]\n"
"    --detailed-counts=no|yes  count loads, stores and alu ops [no]\n"
"    --trace-mem=no|yes        trace all loads and stores [no]\n"
"    --trace-superblocks=no|yes  trace all superblock entries [no]\n"
"    --fnname=<name>           count calls to <name> (only used if\n"
"                              --basic-count=yes)  [main]\n"
"    --trace-match=0x<val>     match for a binop result\n"
   );
}

static void lk_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}

/*------------------------------------------------------------*/
/*--- Stuff for --basic-counts                             ---*/
/*------------------------------------------------------------*/

/* Nb: use ULongs because the numbers can get very big */
static ULong n_func_calls    = 0;
static ULong n_SBs_entered   = 0;
static ULong n_SBs_completed = 0;
static ULong n_IRStmts       = 0;
static ULong n_guest_instrs  = 0;
static ULong n_Jccs          = 0;
static ULong n_Jccs_untaken  = 0;
static ULong n_IJccs         = 0;
static ULong n_IJccs_untaken = 0;

static void add_one_func_call(void)
{
   n_func_calls++;
}

static void add_one_SB_entered(void)
{
   n_SBs_entered++;
}

static void add_one_SB_completed(void)
{
   n_SBs_completed++;
}

static void add_one_IRStmt(void)
{
   n_IRStmts++;
}

static void add_one_guest_instr(void)
{
   n_guest_instrs++;
}

static void add_one_Jcc(void)
{
   n_Jccs++;
}

static void add_one_Jcc_untaken(void)
{
   n_Jccs_untaken++;
}

static void add_one_inverted_Jcc(void)
{
   n_IJccs++;
}

static void add_one_inverted_Jcc_untaken(void)
{
   n_IJccs_untaken++;
}

/*------------------------------------------------------------*/
/*--- Stuff for --detailed-counts                          ---*/
/*------------------------------------------------------------*/

typedef
   IRExpr
   IRAtom;

/* --- Operations --- */

typedef enum { OpLoad=0, OpStore=1, OpAlu=2 } Op;

#define N_OPS 3


/* --- Types --- */

#define N_TYPES 14

static Int type2index ( IRType ty )
{
   switch (ty) {
      case Ity_I1:      return 0;
      case Ity_I8:      return 1;
      case Ity_I16:     return 2;
      case Ity_I32:     return 3;
      case Ity_I64:     return 4;
      case Ity_I128:    return 5;
      case Ity_F32:     return 6;
      case Ity_F64:     return 7;
      case Ity_F128:    return 8;
      case Ity_V128:    return 9;
      case Ity_V256:    return 10;
      case Ity_D32:     return 11;
      case Ity_D64:     return 12;
      case Ity_D128:    return 13;
      default: tl_assert(0);
   }
}

static const HChar* nameOfTypeIndex ( Int i )
{
   switch (i) {
      case 0: return "I1";   break;
      case 1: return "I8";   break;
      case 2: return "I16";  break;
      case 3: return "I32";  break;
      case 4: return "I64";  break;
      case 5: return "I128"; break;
      case 6: return "F32";  break;
      case 7: return "F64";  break;
      case 8: return "F128";  break;
      case 9: return "V128";  break;
      case 10: return "V256"; break;
      case 11: return "D32";  break;
      case 12: return "D64";  break;
      case 13: return "D128"; break;
      default: tl_assert(0);
   }
}


/* --- Counts --- */

static ULong detailCounts[N_OPS][N_TYPES];

/* The helper that is called from the instrumented code. */
static VG_REGPARM(1)
void increment_detail(ULong* detail)
{
   (*detail)++;
}

/* A helper that adds the instrumentation for a detail.  guard ::
   Ity_I1 is the guarding condition for the event.  If NULL it is
   assumed to mean "always True". */
static void instrument_detail(IRSB* sb, Op op, IRType type, IRAtom* guard)
{
   IRDirty* di;
   IRExpr** argv;
   const UInt typeIx = type2index(type);

   tl_assert(op < N_OPS);
   tl_assert(typeIx < N_TYPES);

   argv = mkIRExprVec_1( mkIRExpr_HWord( (HWord)&detailCounts[op][typeIx] ) );
   di = unsafeIRDirty_0_N( 1, "increment_detail",
                              VG_(fnptr_to_fnentry)( &increment_detail ),
                              argv);
   if (guard) di->guard = guard;
   addStmtToIRSB( sb, IRStmt_Dirty(di) );
}

/* Summarize and print the details. */
static void print_details ( void )
{
   Int typeIx;
   VG_(umsg)("   Type        Loads       Stores       AluOps\n");
   VG_(umsg)("   -------------------------------------------\n");
   for (typeIx = 0; typeIx < N_TYPES; typeIx++) {
      VG_(umsg)("   %-4s %'12llu %'12llu %'12llu\n",
                nameOfTypeIndex( typeIx ),
                detailCounts[OpLoad ][typeIx],
                detailCounts[OpStore][typeIx],
                detailCounts[OpAlu  ][typeIx]
      );
   }
}


/*------------------------------------------------------------*/
/*--- Stuff for --trace-mem                                ---*/
/*------------------------------------------------------------*/

#define MAX_DSIZE    512

typedef
   enum { Event_Ir, Event_Dr, Event_Dw, Event_Dm }
   EventKind;

typedef
   struct {
      EventKind  ekind;
      IRAtom*    addr;
      Int        size;
      IRAtom*    guard; /* :: Ity_I1, or NULL=="always True" */
   }
   Event;

/* Up to this many unnotified events are allowed.  Must be at least two,
   so that reads and writes to the same address can be merged into a modify.
   Beyond that, larger numbers just potentially induce more spilling due to
   extending live ranges of address temporaries. */
#define N_EVENTS 4

/* Maintain an ordered list of memory events which are outstanding, in
   the sense that no IR has yet been generated to do the relevant
   helper calls.  The SB is scanned top to bottom and memory events
   are added to the end of the list, merging with the most recent
   notified event where possible (Dw immediately following Dr and
   having the same size and EA can be merged).

   This merging is done so that for architectures which have
   load-op-store instructions (x86, amd64), the instr is treated as if
   it makes just one memory reference (a modify), rather than two (a
   read followed by a write at the same address).

   At various points the list will need to be flushed, that is, IR
   generated from it.  That must happen before any possible exit from
   the block (the end, or an IRStmt_Exit).  Flushing also takes place
   when there is no space to add a new event, and before entering a
   RMW (read-modify-write) section on processors supporting LL/SC.

   If we require the simulation statistics to be up to date with
   respect to possible memory exceptions, then the list would have to
   be flushed before each memory reference.  That's a pain so we don't
   bother.

   Flushing the list consists of walking it start to end and emitting
   instrumentation IR for each event, in the order in which they
   appear. */

#define ENABLE_OUTPUT 0

static Event events[N_EVENTS];
static Int   events_used = 0;
//static VgFile *bin_file = 0;
static int enable_trace = 0;

static VG_REGPARM(2) void trace_instr(Addr addr, SizeT size)
{
  int i;
  if (ENABLE_OUTPUT &&
      /*(addr >= 0xa59d000 && addr < 0xadd6000) &&*/
      (enable_trace)) {

    //VG_(printf)("I  %08lx,%lu:", addr, size);

    for (i = 0; i < size; i++) {
      //VG_(printf)(" %02x", ((unsigned char*)addr)[i]);
      //if (bin_file) {
      //  VG_(fprintf)(bin_file, "%c", (int)((unsigned char*)addr)[i]);
      //}
    }
    _DInst insn[1]; unsigned int icnt = 0, decodedInstructionsCount = 0;
    _CodeInfo ci; int res;
    _DecodedInst decodedInstructions[1];
    (void)res; (void)ci; (void)icnt; (void)insn;

    ci.code = (uint8_t*)addr;
    ci.codeOffset = 0;
    ci.codeLen = 0;
    ci.nextOffset = size;
    ci.dt = Decode64Bits;
    ci.features = DF_NONE;
    //_DecodeResult r = distorm_decompose64(&ci, insn, 1, &icnt);

    res = distorm_decode(addr, (const unsigned char*)(uint8_t*)addr, addr+size, Decode64Bits, decodedInstructions, 1, &decodedInstructionsCount);

    VG_(printf)("I  %llx %-24s %s%s%s\r\n", (long long)decodedInstructions[0].offset,
	   (char*)decodedInstructions[0].instructionHex.p,
	   (char*)decodedInstructions[0].mnemonic.p, decodedInstructions[0].operands.length != 0 ? " " : "",
	   (char*)decodedInstructions[0].operands.p);

  }

  //VG_(printf)("\n");
}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size)
{
  //VG_(printf)(" L %08lx,%lu\n", addr, size);
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size)
{
  //VG_(printf)(" S %08lx,%lu\n", addr, size);
}

static VG_REGPARM(2) void trace_modify(Addr addr, SizeT size)
{
  //VG_(printf)(" M %08lx,%lu\n", addr, size);
}


static void flushEvents(IRSB* sb)
{
   Int        i;
   const HChar* helperName;
   void*      helperAddr;
   IRExpr**   argv;
   IRDirty*   di;
   Event*     ev;

   for (i = 0; i < events_used; i++) {

      ev = &events[i];

      // Decide on helper fn to call and args to pass it.
      switch (ev->ekind) {
         case Event_Ir: helperName = "trace_instr";
                        helperAddr =  trace_instr;  break;

         case Event_Dr: helperName = "trace_load";
                        helperAddr =  trace_load;   break;

         case Event_Dw: helperName = "trace_store";
                        helperAddr =  trace_store;  break;

         case Event_Dm: helperName = "trace_modify";
                        helperAddr =  trace_modify; break;
         default:
            tl_assert(0);
      }

      // Add the helper.
      argv = mkIRExprVec_2( ev->addr, mkIRExpr_HWord( ev->size ) );
      di   = unsafeIRDirty_0_N( /*regparms*/2,
                                helperName, VG_(fnptr_to_fnentry)( helperAddr ),
                                argv );
      if (ev->guard) {
         di->guard = ev->guard;
      }
      addStmtToIRSB( sb, IRStmt_Dirty(di) );
   }

   events_used = 0;
}

// WARNING:  If you aren't interested in instruction reads, you can omit the
// code that adds calls to trace_instr() in flushEvents().  However, you
// must still call this function, addEvent_Ir() -- it is necessary to add
// the Ir events to the events list so that merging of paired load/store
// events into modify events works correctly.
static void addEvent_Ir ( IRSB* sb, IRAtom* iaddr, UInt isize )
{
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
            || VG_CLREQ_SZB == isize );
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Ir;
   evt->addr  = iaddr;
   evt->size  = isize;
   evt->guard = NULL;
   events_used++;
}

/* Add a guarded read event. */
static
void addEvent_Dr_guarded ( IRSB* sb, IRAtom* daddr, Int dsize, IRAtom* guard )
{
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dr;
   evt->addr  = daddr;
   evt->size  = dsize;
   evt->guard = guard;
   events_used++;
}

/* Add an ordinary read event, by adding a guarded read event with an
   always-true guard. */
static
void addEvent_Dr ( IRSB* sb, IRAtom* daddr, Int dsize )
{
   addEvent_Dr_guarded(sb, daddr, dsize, NULL);
}

/* Add a guarded write event. */
static
void addEvent_Dw_guarded ( IRSB* sb, IRAtom* daddr, Int dsize, IRAtom* guard )
{
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dw;
   evt->addr  = daddr;
   evt->size  = dsize;
   evt->guard = guard;
   events_used++;
}

/* Add an ordinary write event.  Try to merge it with an immediately
   preceding ordinary read event of the same size to the same
   address. */
static
void addEvent_Dw ( IRSB* sb, IRAtom* daddr, Int dsize )
{
   Event* lastEvt;
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

   // Is it possible to merge this write with the preceding read?
   lastEvt = &events[events_used-1];
   if (events_used > 0
       && lastEvt->ekind == Event_Dr
       && lastEvt->size  == dsize
       && lastEvt->guard == NULL
       && eqIRAtom(lastEvt->addr, daddr))
   {
      lastEvt->ekind = Event_Dm;
      return;
   }

   // No.  Add as normal.
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dw;
   evt->size  = dsize;
   evt->addr  = daddr;
   evt->guard = NULL;
   events_used++;
}


/*------------------------------------------------------------*/
/*--- Stuff for --trace-superblocks                        ---*/
/*------------------------------------------------------------*/

static void trace_superblock(Addr addr)
{
   VG_(printf)("SB %08lx\n", addr);
}


/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

static void lk_post_clo_init(void)
{
   Int op, tyIx;

   if (clo_detailed_counts) {
      for (op = 0; op < N_OPS; op++)
         for (tyIx = 0; tyIx < N_TYPES; tyIx++)
            detailCounts[op][tyIx] = 0;
   }
}

/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
/****************************************************************/
/*************** taintgrind copy ********************************/
typedef
struct _LCEnv {
  IRSB* sb;
  int trace;
  IRType hWordTy;
} LCEnv;

/* add stmt to a bb */
static inline void stmt ( HChar cat, LCEnv* mce, IRStmt* st ) { //385
   if (mce->trace) {
      VG_(printf)("  %c: ", cat);
      ppIRStmt(st);
      VG_(printf)("\n");
   }
   addStmtToIRSB(mce->sb, st);
}

/* assign value to tmp */
static inline
void assign ( HChar cat, LCEnv* mce, IRTemp tmp, IRExpr* expr ) {
   stmt(cat, mce, IRStmt_WrTmp(tmp,expr));
}

/* build various kinds of expressions *///400
#define triop(_op, _arg1, _arg2, _arg3) \
                                 IRExpr_Triop((_op),(_arg1),(_arg2),(_arg3))
#define binop(_op, _arg1, _arg2) IRExpr_Binop((_op),(_arg1),(_arg2))
#define unop(_op, _arg)          IRExpr_Unop((_op),(_arg))
#define mkU1(_n)                 IRExpr_Const(IRConst_U1(_n))
#define mkU8(_n)                 IRExpr_Const(IRConst_U8(_n))
#define mkU16(_n)                IRExpr_Const(IRConst_U16(_n))
#define mkU32(_n)                IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n)                IRExpr_Const(IRConst_U64(_n))
#define mkV128(_n)               IRExpr_Const(IRConst_V128(_n))
#define mkexpr(_tmp)             IRExpr_RdTmp((_tmp))

/* Bind the given expression to a new temporary, and return the
   temporary.  This effectively converts an arbitrary expression into
   an atom.

   'ty' is the type of 'e' and hence the type that the new temporary
   needs to be.  But passing it in is redundant, since we can deduce
   the type merely by inspecting 'e'.  So at least use that fact to
   assert that the two types agree. */
static IRAtom* assignNew ( HChar cat, LCEnv* mce, IRType ty, IRExpr* e ) //418
{
   IRTemp   t;
   IRType   tyE = typeOfIRExpr(mce->sb->tyenv, e);
   tl_assert(tyE == ty); /* so 'ty' is redundant (!) */
   t = newIRTemp(mce->sb->tyenv, ty);
   //t = newTemp(mce, ty, k);
   assign(cat, mce, t, e);
   return mkexpr(t);
}

static IRExpr* convert_Value( LCEnv* mce, IRAtom* value ){
   IRType ty = typeOfIRExpr(mce->sb->tyenv, value);
   IRType tyH = mce->hWordTy;
   //   IRExpr* e;
   if(tyH == Ity_I32){
      switch( ty ){
      case Ity_I1:
         return assignNew( 'C', mce, tyH, unop(Iop_1Uto32, value) );
      case Ity_I8:
         return assignNew( 'C', mce, tyH, unop(Iop_8Uto32, value) );
      case Ity_I16:
         return assignNew( 'C', mce, tyH, unop(Iop_16Uto32, value) );
      case Ity_I32:
         return value;
      case Ity_I64:
         return assignNew( 'C', mce, tyH, unop(Iop_64to32, value) );
      case Ity_F32:
         return assignNew( 'C', mce, tyH, unop(Iop_ReinterpF32asI32, value) );
      case Ity_F64:
         return assignNew( 'C', mce, tyH, unop(Iop_64to32,
            assignNew( 'C', mce, Ity_I64, unop(Iop_ReinterpF64asI64, value) ) ) );
      case Ity_V128:
         return assignNew( 'C', mce, tyH, unop(Iop_V128to32, value) );
      default:
         ppIRType(ty);
         VG_(tool_panic)("tnt_translate.c: convert_Value");
      }
   }else if(tyH == Ity_I64){
      switch( ty ){
      case Ity_I1:
         return assignNew( 'C', mce, tyH, unop(Iop_1Uto64, value) );
      case Ity_I8:
         return assignNew( 'C', mce, tyH, unop(Iop_8Uto64, value) );
      case Ity_I16:
         return assignNew( 'C', mce, tyH, unop(Iop_16Uto64, value) );
      case Ity_I32:
         return assignNew( 'C', mce, tyH, unop(Iop_32Uto64, value) );
      case Ity_I64:
         return value;
      case Ity_I128:
         return assignNew( 'C', mce, tyH, unop(Iop_128to64, value) );
      case Ity_F32:
         return assignNew( 'C', mce, tyH, unop(Iop_ReinterpF64asI64,
              assignNew( 'C', mce, Ity_F64, unop(Iop_F32toF64, value) ) ) );
      case Ity_F64:
         return assignNew( 'C', mce, tyH, unop(Iop_ReinterpF64asI64, value) );
      case Ity_V128:
         return assignNew( 'C', mce, tyH, unop(Iop_V128to64, value) );
      case Ity_V256:
         // Warning: Only copies the least significant 64 bits, so there's info lost
         return assignNew( 'C', mce, tyH, unop(Iop_V256to64_0, value) );
      default:
         ppIRType(ty);
         VG_(tool_panic)("tnt_translate.c: convert_Value");
      }
   }else{
         ppIRType(tyH);
         VG_(tool_panic)("tnt_translate.c: convert_Value");
   }
}


static
VG_REGPARM(3) void LK_(h32_binop_tc) ( IRStmt *stmt, UInt a , UInt b )
{
}

static
VG_REGPARM(3) void LK_(h32_binop_ct) ( IRStmt *stmt, UInt a , UInt b )
{
}

static
VG_REGPARM(3) void LK_(h32_binop_tt) ( IRStmt *stmt, UInt a , UInt b )
{
  VG_(printf)(".");
}

static
VG_REGPARM(3) void LK_(h32_binop_cc) ( IRStmt *stmt, UInt a , UInt b )
{
}


static
VG_REGPARM(3) void LK_(h64_binop_tc) ( IRStmt *stmt, ULong a , ULong b )
{
  if (a & 0xffff == 0xe91b) {
    VG_(printf)("---------------------- found --------------------\n");
  }

}

static
VG_REGPARM(3) void LK_(h64_binop_ct) ( IRStmt *stmt, ULong a, ULong b )
{
  if (a & 0xffff == 0xe91b) {
    VG_(printf)("---------------------- found --------------------\n");
  }
}

static
VG_REGPARM(3) void LK_(h64_binop_tt) ( IRStmt *stmt, ULong a , ULong b )
{
  //ppIRStmt(stmt); vex_printf("0x%x\n", a);

  if (a & 0xffff == 0xe91b) {
    VG_(printf)("---------------------- found --------------------\n");
  }

}

static
VG_REGPARM(3) void LK_(h64_binop_cc) ( IRStmt *stmt, ULong a, ULong b )
{
}


/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */


static
IRSB* lk_instrument ( VgCallbackClosure* closure,
                      IRSB* sbIn,
                      const VexGuestLayout* layout,
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
   IRDirty*   di;
   Int        i;
   IRSB*      sbOut;
   IRTypeEnv* tyenv = sbIn->tyenv;
   Addr       iaddr = 0, dst;
   UInt       ilen = 0;
   Bool       condition_inverted = False;
   LCEnv   mce;
   VG_(memset)(&mce, 0, sizeof(mce));

   if (gWordTy != hWordTy) {
      /* We don't currently support this case. */
      VG_(tool_panic)("host/guest word size mismatch");
   }

   /* Set up SB */
   sbOut = deepCopyIRSBExceptStmts(sbIn);

   // Copy verbatim any IR preamble preceding the first IMark
   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( sbOut, sbIn->stmts[i] );
      i++;
   }

   if (clo_basic_counts) {
      /* Count this superblock. */
      di = unsafeIRDirty_0_N( 0, "add_one_SB_entered",
                                 VG_(fnptr_to_fnentry)( &add_one_SB_entered ),
                                 mkIRExprVec_0() );
      addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
   }

   if (clo_trace_sbs) {
      /* Print this superblock's address. */
      di = unsafeIRDirty_0_N(
              0, "trace_superblock",
              VG_(fnptr_to_fnentry)( &trace_superblock ),
              mkIRExprVec_1( mkIRExpr_HWord( vge->base[0] ) )
           );
      addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
   }

   if (clo_trace_mem) {
      events_used = 0;
   }

   Bool    verboze = 0||False;
   mce.sb             = sbOut;
   mce.hWordTy        = hWordTy;
   mce.trace          = verboze;

   for (/*use current i*/; i < sbIn->stmts_used; i++) {
      IRStmt* st = sbIn->stmts[i];
      if (!st || st->tag == Ist_NoOp) continue;

      if (clo_basic_counts) {
         /* Count one VEX statement. */
         di = unsafeIRDirty_0_N( 0, "add_one_IRStmt",
                                    VG_(fnptr_to_fnentry)( &add_one_IRStmt ),
                                    mkIRExprVec_0() );
         addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
      }

      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_MBE:
            addStmtToIRSB( sbOut, st );
            break;

         case Ist_IMark:
            if (clo_basic_counts) {
               /* Needed to be able to check for inverted condition in Ist_Exit */
               iaddr = st->Ist.IMark.addr;
               ilen  = st->Ist.IMark.len;

               /* Count guest instruction. */
               di = unsafeIRDirty_0_N( 0, "add_one_guest_instr",
                                          VG_(fnptr_to_fnentry)( &add_one_guest_instr ),
                                          mkIRExprVec_0() );
               addStmtToIRSB( sbOut, IRStmt_Dirty(di) );

               /* An unconditional branch to a known destination in the
                * guest's instructions can be represented, in the IRSB to
                * instrument, by the VEX statements that are the
                * translation of that known destination. This feature is
                * called 'SB chasing' and can be influenced by command
                * line option --vex-guest-chase-thresh.
                *
                * To get an accurate count of the calls to a specific
                * function, taking SB chasing into account, we need to
                * check for each guest instruction (Ist_IMark) if it is
                * the entry point of a function.
                */
               tl_assert(clo_fnname);
               tl_assert(clo_fnname[0]);
               const HChar *fnname;
               if (VG_(get_fnname_if_entry)(st->Ist.IMark.addr,
                                            &fnname)
                   && 0 == VG_(strcmp)(fnname, clo_fnname)) {
                  di = unsafeIRDirty_0_N(
                          0, "add_one_func_call",
                             VG_(fnptr_to_fnentry)( &add_one_func_call ),
                             mkIRExprVec_0() );
                  addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
               }
            }
            if (clo_trace_mem) {
               // WARNING: do not remove this function call, even if you
               // aren't interested in instruction reads.  See the comment
               // above the function itself for more detail.
               addEvent_Ir( sbOut, mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ),
                            st->Ist.IMark.len );
            }
            addStmtToIRSB( sbOut, st );
            break;

        case Ist_WrTmp: {
            // Add a call to trace_load() if --trace-mem=yes.
            if (clo_trace_mem) {
               IRExpr* data = st->Ist.WrTmp.data;

	       switch( data->tag ){
	       case Iex_Binop:
		 //data->Iex.Binop.op,
		 //data->Iex.Binop.arg1, data->Iex.Binop.arg2
		 break;
	       default:
		 break;
	       }

               if (data->tag == Iex_Load) {
                  addEvent_Dr( sbOut, data->Iex.Load.addr,
                               sizeofIRType(data->Iex.Load.ty) );
               }
            }
	    IRExpr* expr = st->Ist.WrTmp.data;
	    IRType  type = typeOfIRExpr(sbOut->tyenv, expr);
            if (clo_detailed_counts) {
               tl_assert(type != Ity_INVALID);
               switch (expr->tag) {
                  case Iex_Load:
                    instrument_detail( sbOut, OpLoad, type, NULL/*guard*/ );
                     break;
                  case Iex_Unop:
	          case Iex_Binop:
		    break;
                  case Iex_Triop:
                  case Iex_Qop:
                  case Iex_ITE:
                     instrument_detail( sbOut, OpAlu, type, NULL/*guard*/ );
                     break;
                  default:
                     break;
               }
            }

	    /* trace values of bin */
	    switch (expr->tag) {
	    case Iex_Binop:
	      {
		IRStmt *irstmt;
		IRTemp tmp = st->Ist.WrTmp.tmp;

		/* clone the src operands, later printed */
		IRStmt *clone = deepMallocIRStmt(st);

		/* add the wrtmp op now */
		irstmt = IRStmt_WrTmp( tmp, expr );
		addStmtToIRSB( sbOut, irstmt );
		IROp op = expr->Iex.Binop.op;
		IRExpr *arg1 = expr->Iex.Binop.arg1, *arg2 = expr->Iex.Binop.arg2;

		/* prepare arguments for function call */
		Int      nargs = 2;
		IRExpr** args;
		args  = mkIRExprVec_2( mkIRExpr_HWord((HWord)clone) ,
				       convert_Value(&mce, IRExpr_RdTmp( tmp ))
				       );

		void*    fn;
		const HChar*   nm;

		if ( mce.hWordTy == Ity_I32 ){
		  if ( arg1->tag == Iex_RdTmp && arg2->tag == Iex_Const ) {
		    fn = &LK_(h32_binop_tc);
		    nm = "LK_(h32_binop_tc)";
		  } else if ( arg1->tag == Iex_Const && arg2->tag == Iex_RdTmp ) {
		    fn = &LK_(h32_binop_ct);
		    nm = "LK_(h32_binop_ct)";
		  } else if ( arg1->tag == Iex_Const && arg2->tag == Iex_Const ) {
		    fn = &LK_(h32_binop_cc);
		    nm = "LK_(h32_binop_cc)";
		  } else if ( arg1->tag == Iex_RdTmp && arg2->tag == Iex_RdTmp ) {
		    fn = &LK_(h32_binop_tt);
		    nm = "LK_(h32_binop_tt)";
		  } else {
		    VG_(tool_panic)("lk_translate.c: create_dirty_BINOP");
		  }
		}else if( mce.hWordTy == Ity_I64 ){
		  if ( arg1->tag == Iex_RdTmp && arg2->tag == Iex_Const ) {
		    fn = &LK_(h64_binop_tc);
		    nm = "LK_(h64_binop_tc)";
		  } else if ( arg1->tag == Iex_Const && arg2->tag == Iex_RdTmp ) {
		    fn = &LK_(h64_binop_ct);
		    nm = "LK_(h64_binop_ct)";
		  } else if ( arg1->tag == Iex_Const && arg2->tag == Iex_Const ) {
		    fn = &LK_(h64_binop_cc);
		    nm = "LK_(h64_binop_cc)";
		  } else if ( arg1->tag == Iex_RdTmp && arg2->tag == Iex_RdTmp ) {
		    fn = &LK_(h64_binop_tt);
		    nm = "LK_(h64_binop_tt)";
		  } else {
		    VG_(tool_panic)("lk_translate.c: create_dirty_BINOP");
		  }
		}else
		  VG_(tool_panic)("lk_translate.c: create_dirty_BINOP: Unknown platform");

		IRDirty *d = unsafeIRDirty_0_N ( nargs/*regparms*/, nm, VG_(fnptr_to_fnentry)( fn ), args );

		stmt( 'V', &mce, IRStmt_Dirty(d));
	      }
	      break;
	    default:
	      addStmtToIRSB( sbOut, st );
	      break;
	    }
	    break;
 	 }

         case Ist_Store: {
            IRExpr* data = st->Ist.Store.data;
            IRType  type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            if (clo_trace_mem) {
               addEvent_Dw( sbOut, st->Ist.Store.addr,
                            sizeofIRType(type) );
            }
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpStore, type, NULL/*guard*/ );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_StoreG: {
            IRStoreG* sg   = st->Ist.StoreG.details;
            IRExpr*   data = sg->data;
            IRType    type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            if (clo_trace_mem) {
               addEvent_Dw_guarded( sbOut, sg->addr,
                                    sizeofIRType(type), sg->guard );
            }
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpStore, type, sg->guard );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_LoadG: {
            IRLoadG* lg       = st->Ist.LoadG.details;
            IRType   type     = Ity_INVALID; /* loaded type */
            IRType   typeWide = Ity_INVALID; /* after implicit widening */
            typeOfIRLoadGOp(lg->cvt, &typeWide, &type);
            tl_assert(type != Ity_INVALID);
            if (clo_trace_mem) {
               addEvent_Dr_guarded( sbOut, lg->addr,
                                    sizeofIRType(type), lg->guard );
            }
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpLoad, type, lg->guard );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_Dirty: {
            if (clo_trace_mem) {
               Int      dsize;
               IRDirty* d = st->Ist.Dirty.details;
               if (d->mFx != Ifx_None) {
                  // This dirty helper accesses memory.  Collect the details.
                  tl_assert(d->mAddr != NULL);
                  tl_assert(d->mSize != 0);
                  dsize = d->mSize;
                  if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                     addEvent_Dr( sbOut, d->mAddr, dsize );
                  if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                     addEvent_Dw( sbOut, d->mAddr, dsize );
               } else {
                  tl_assert(d->mAddr == NULL);
                  tl_assert(d->mSize == 0);
               }
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_CAS: {
            /* We treat it as a read and a write of the location.  I
               think that is the same behaviour as it was before IRCAS
               was introduced, since prior to that point, the Vex
               front ends would translate a lock-prefixed instruction
               into a (normal) read followed by a (normal) write. */
            Int    dataSize;
            IRType dataTy;
            IRCAS* cas = st->Ist.CAS.details;
            tl_assert(cas->addr != NULL);
            tl_assert(cas->dataLo != NULL);
            dataTy   = typeOfIRExpr(tyenv, cas->dataLo);
            dataSize = sizeofIRType(dataTy);
            if (cas->dataHi != NULL)
               dataSize *= 2; /* since it's a doubleword-CAS */
            if (clo_trace_mem) {
               addEvent_Dr( sbOut, cas->addr, dataSize );
               addEvent_Dw( sbOut, cas->addr, dataSize );
            }
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpLoad, dataTy, NULL/*guard*/ );
               if (cas->dataHi != NULL) /* dcas */
                  instrument_detail( sbOut, OpLoad, dataTy, NULL/*guard*/ );
               instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
               if (cas->dataHi != NULL) /* dcas */
                  instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_LLSC: {
            IRType dataTy;
            if (st->Ist.LLSC.storedata == NULL) {
               /* LL */
               dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
               if (clo_trace_mem) {
                  addEvent_Dr( sbOut, st->Ist.LLSC.addr,
                                      sizeofIRType(dataTy) );
                  /* flush events before LL, helps SC to succeed */
                  flushEvents(sbOut);
	       }
               if (clo_detailed_counts)
                  instrument_detail( sbOut, OpLoad, dataTy, NULL/*guard*/ );
            } else {
               /* SC */
               dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
               if (clo_trace_mem)
                  addEvent_Dw( sbOut, st->Ist.LLSC.addr,
                                      sizeofIRType(dataTy) );
               if (clo_detailed_counts)
                  instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_Exit:
            if (clo_basic_counts) {
               // The condition of a branch was inverted by VEX if a taken
               // branch is in fact a fall trough according to client address
               tl_assert(iaddr != 0);
               dst = (sizeof(Addr) == 4) ? st->Ist.Exit.dst->Ico.U32 :
                                           st->Ist.Exit.dst->Ico.U64;
               condition_inverted = (dst == iaddr + ilen);

               /* Count Jcc */
               if (!condition_inverted)
                  di = unsafeIRDirty_0_N( 0, "add_one_Jcc",
                                          VG_(fnptr_to_fnentry)( &add_one_Jcc ),
                                          mkIRExprVec_0() );
               else
                  di = unsafeIRDirty_0_N( 0, "add_one_inverted_Jcc",
                                          VG_(fnptr_to_fnentry)(
                                             &add_one_inverted_Jcc ),
                                          mkIRExprVec_0() );

               addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
            }
            if (clo_trace_mem) {
               flushEvents(sbOut);
            }

            addStmtToIRSB( sbOut, st );      // Original statement

            if (clo_basic_counts) {
               /* Count non-taken Jcc */
               if (!condition_inverted)
                  di = unsafeIRDirty_0_N( 0, "add_one_Jcc_untaken",
                                          VG_(fnptr_to_fnentry)(
                                             &add_one_Jcc_untaken ),
                                          mkIRExprVec_0() );
               else
                  di = unsafeIRDirty_0_N( 0, "add_one_inverted_Jcc_untaken",
                                          VG_(fnptr_to_fnentry)(
                                             &add_one_inverted_Jcc_untaken ),
                                          mkIRExprVec_0() );

               addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
            }
            break;

         default:
            ppIRStmt(st);
            tl_assert(0);
      }
   }

   if (clo_basic_counts) {
      /* Count this basic block. */
      di = unsafeIRDirty_0_N( 0, "add_one_SB_completed",
                                 VG_(fnptr_to_fnentry)( &add_one_SB_completed ),
                                 mkIRExprVec_0() );
      addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
   }

   if (clo_trace_mem) {
      /* At the end of the sbIn.  Flush outstandings. */
      flushEvents(sbOut);
   }

   return sbOut;
}

static void lk_fini(Int exitcode)
{
   tl_assert(clo_fnname);
   tl_assert(clo_fnname[0]);

   if (clo_basic_counts) {
      ULong total_Jccs = n_Jccs + n_IJccs;
      ULong taken_Jccs = (n_Jccs - n_Jccs_untaken) + n_IJccs_untaken;

      VG_(umsg)("Counted %'llu call%s to %s()\n",
                n_func_calls, ( n_func_calls==1 ? "" : "s" ), clo_fnname);

      VG_(umsg)("\n");
      VG_(umsg)("Jccs:\n");
      VG_(umsg)("  total:         %'llu\n", total_Jccs);
      VG_(umsg)("  taken:         %'llu (%.0f%%)\n",
                taken_Jccs, taken_Jccs * 100.0 / total_Jccs ?: 1);

      VG_(umsg)("\n");
      VG_(umsg)("Executed:\n");
      VG_(umsg)("  SBs entered:   %'llu\n", n_SBs_entered);
      VG_(umsg)("  SBs completed: %'llu\n", n_SBs_completed);
      VG_(umsg)("  guest instrs:  %'llu\n", n_guest_instrs);
      VG_(umsg)("  IRStmts:       %'llu\n", n_IRStmts);

      VG_(umsg)("\n");
      VG_(umsg)("Ratios:\n");
      tl_assert(n_SBs_entered); // Paranoia time.
      VG_(umsg)("  guest instrs : SB entered  = %'llu : 10\n",
         10 * n_guest_instrs / n_SBs_entered);
      VG_(umsg)("       IRStmts : SB entered  = %'llu : 10\n",
         10 * n_IRStmts / n_SBs_entered);
      tl_assert(n_guest_instrs); // Paranoia time.
      VG_(umsg)("       IRStmts : guest instr = %'llu : 10\n",
         10 * n_IRStmts / n_guest_instrs);
   }

   if (clo_detailed_counts) {
      VG_(umsg)("\n");
      VG_(umsg)("IR-level counts by type:\n");
      print_details();
   }

   if (clo_basic_counts) {
      VG_(umsg)("\n");
      VG_(umsg)("Exit code:       %d\n", exitcode);
   }
}

static
void lk_new_mem_mmap ( Addr a, SizeT len, Bool rr, Bool ww, Bool xx, ULong di_handle )
{

}

static
void lk_pre_syscall(ThreadId tid, UInt syscallno,
                           UWord* args, UInt nArgs)
{
}

#define FD_MAX 256
#define FD_MAX_PATH 256
#define FD_READ 0x1
#define FD_WRITE 0x2
#define FD_STAT 0x4

static
void resolve_filename(UWord fd, HChar *path, Int max)
{
   HChar src[FD_MAX_PATH];
   Int len = 0;

   // TODO: Cache resolved fds by also catching open()s and close()s
   VG_(sprintf)(src, "/proc/%d/fd/%d", VG_(getpid)(), (int)fd);
   len = VG_(readlink)(src, path, max);

   // Just give emptiness on error.
   if (len == -1) len = 0;
   path[len] = '\0';
}


static void lk_syscall_open(ThreadId tid, UWord* args, UInt nArgs, SysRes res) {

   HChar fdpath[FD_MAX_PATH];
   Int fd = sr_Res(res);
   Bool verbose = False;
   (void)verbose;
   resolve_filename(fd, fdpath, FD_MAX_PATH-1);
   if (!VG_(strcmp)(fdpath, "/mnt/btfs0/eda/Xilinx-2016.3/Vivado/2016.3/lib/lnx64.o/libisl_iostreams.so"))
       enable_trace = 1;
   VG_(printf)("O %d:%s\n", fd,fdpath);
}

static void lk_syscall_mmap(ThreadId tid, UWord* args, UInt nArgs, SysRes res) {

  VG_(printf)("M %p:l=%d:p=0x%x:f=0x%x:fd=%d:off:0x%x=>0x%p\n", (void*)args[0],(int)args[1],(int)args[2],(int)args[3],(int)args[4],(int)args[5], (void*)sr_Res(res));

}

static
void lk_post_syscall(ThreadId tid, UInt syscallno,
                            UWord* args, UInt nArgs, SysRes res)
{
  //TNT_(syscall_allowed_check)(tid, syscallno);

   switch ((int)syscallno) {
    // Should be defined by respective vki/vki-arch-os.h
    case __NR_read:
      //TNT_(syscall_read)(tid, args, nArgs, res);
      break;
    case __NR_write:
      //TNT_(syscall_write)(tid, args, nArgs, res);
      break;
    case __NR_open:
    case __NR_openat:
      lk_syscall_open(tid, args, nArgs, res);
      break;
    case __NR_mmap:
      lk_syscall_mmap(tid, args, nArgs, res);
      break;
    case __NR_close:
      //TNT_(syscall_close)(tid, args, nArgs, res);
      break;
    case __NR_lseek:
      //TNT_(syscall_lseek)(tid, args, nArgs, res);
      break;
#ifdef __NR_llseek
    case __NR_llseek:
      //TNT_(syscall_llseek)(tid, args, nArgs, res);
      break;
#endif
    case __NR_pread64:
      //TNT_(syscall_pread)(tid, args, nArgs, res);
      break;
  }
}


static void lk_pre_clo_init(void)
{
  //bin_file = VG_(fopen)("/mnt/btfs0/eda/log/trace.bin", VKI_O_CREAT|VKI_O_WRONLY|VKI_O_TRUNC, 0);

   VG_(details_name)            ("Lackey");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("an example Valgrind tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2015, and GNU GPL'd, by Nicholas Nethercote.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);
   VG_(details_avg_translation_sizeB) ( 200 );

   VG_(basic_tool_funcs)          (lk_post_clo_init,
                                   lk_instrument,
                                   lk_fini);
   VG_(needs_command_line_options)(lk_process_cmd_line_option,
                                   lk_print_usage,
                                   lk_print_debug_usage);

   VG_(needs_syscall_wrapper)     ( lk_pre_syscall,
                                    lk_post_syscall );

   VG_(track_new_mem_mmap)        ( lk_new_mem_mmap );

}

VG_DETERMINE_INTERFACE_VERSION(lk_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                lk_main.c ---*/
/*--------------------------------------------------------------------*/
