#ifndef __LACKEY_H_
#define __LACKEY_H_

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
#include "copy.h"

#define TR_(str)    VGAPPEND(vgLackey_,str)

typedef
struct _LCEnv {
  IRSB* sb;
  int trace;
  IRType hWordTy;
} LCEnv;

typedef IRExpr IRAtom;

void TR_(stmt) ( HChar cat, LCEnv* mce, IRStmt* st );

IRExpr* TR_(convert_Value)( LCEnv* mce, IRAtom* value );

VG_REGPARM(3) void TR_(h32_binop_tc) ( IRStmt *stmt, UInt a , UInt b );
VG_REGPARM(3) void TR_(h32_binop_ct) ( IRStmt *stmt, UInt a , UInt b );
VG_REGPARM(3) void TR_(h32_binop_tt) ( IRStmt *stmt, UInt a , UInt b );
VG_REGPARM(3) void TR_(h32_binop_cc) ( IRStmt *stmt, UInt a , UInt b );

VG_REGPARM(3) void TR_(h64_binop_tc) ( IRStmt *stmt, ULong a , ULong b );
VG_REGPARM(3) void TR_(h64_binop_ct) ( IRStmt *stmt, ULong a, ULong b );
VG_REGPARM(3) void TR_(h64_binop_tt) ( IRStmt *stmt, ULong a , ULong b );
VG_REGPARM(3) void TR_(h64_binop_cc) ( IRStmt *stmt, ULong a, ULong b );

VG_REGPARM(3) void TR_(h32_get) (IRStmt *clone, UInt value, UInt taint );
VG_REGPARM(3) void TR_(h64_get) (IRStmt *clone, ULong value, ULong taint );

VG_REGPARM(2) void TR_(print_Get)(Int offset);
VG_REGPARM(2) void TR_(print_ir)(IRStmt *clone);

void traverse_stmt(LCEnv *mce, IRStmt* st) ;
void traverse_expr(LCEnv *mce, IRExpr *e) ;

#endif
