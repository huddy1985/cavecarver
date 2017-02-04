#include "lackey.h"

/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
/****************************************************************/
/*************** taintgrind copy ********************************/
HChar* client_binary_name = NULL;

static void infer_client_binary_name(UInt pc) {

   if (client_binary_name == NULL) {
      DebugInfo* di = VG_(find_DebugInfo)(pc);
      if (di && VG_(strcmp)(VG_(DebugInfo_get_soname)(di), "NONE") == 0) {
         //VG_(printf)("client_binary_name: %s\n", VG_(DebugInfo_get_filename)(di));
         client_binary_name = (HChar*)VG_(malloc)("client_binary_name",sizeof(HChar)*(VG_(strlen)(VG_(DebugInfo_get_filename)(di)+1)));
         VG_(strcpy)(client_binary_name, VG_(DebugInfo_get_filename)(di));
      }
   }
}

static Int extract_IRConst( IRConst* con ){
   switch(con->tag){
      case Ico_U1:
         return con->Ico.U1;
      case Ico_U8:
         return con->Ico.U8;
      case Ico_U16:
         return con->Ico.U16;
      case Ico_U32:
         return con->Ico.U32;
      case Ico_U64: // Taintgrind: Re-cast it and hope for the best
         return con->Ico.U64;
      case Ico_F64:
         return con->Ico.F64;
      case Ico_F64i:
         return con->Ico.F64i;
      case Ico_V128:
         return con->Ico.V128;
      default:
         ppIRConst(con);
         VG_(tool_panic)("tnt_translate.c: convert_IRConst");
   }
}

/* add stmt to a bb */
void LK_(stmt) ( HChar cat, LCEnv* mce, IRStmt* st ) { //385
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
    LK_(stmt)(cat, mce, IRStmt_WrTmp(tmp,expr));
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

IRExpr* LK_(convert_Value)( LCEnv* mce, IRAtom* value ){
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

#define H_EXIT_EARLY \

#define H_SMT2( fn ) \

#define H64_PC_OP \
   ULong pc = VG_(get_IP)( VG_(get_running_tid)() ); \
   HChar aTmp1[128], aTmp2[128]; \
   infer_client_binary_name(pc); \
   const HChar *fnname = VG_(describe_IP) ( pc, NULL );

#define H64_PRINT_OP(value, taint)        \
   VG_(printf)("%s | %s", fnname, aTmp1); \
   ppIROp(op); \
   VG_(printf)("%s | 0x%llx  | 0x%llx ", aTmp2, value, taint);

#define H_WRTMP_BOOKKEEPING \
   UInt ltmp = stmt->Ist.WrTmp.tmp; \

VG_REGPARM(3) void LK_(h32_binop_tc) ( IRStmt *stmt, UInt a , UInt b )
{
    VG_(printf)("V h32_binop_tc\n");
}

VG_REGPARM(3) void LK_(h32_binop_ct) ( IRStmt *stmt, UInt a , UInt b )
{
    VG_(printf)("V h32_binop_ct\n");
}

VG_REGPARM(3) void LK_(h32_binop_tt) ( IRStmt *stmt, UInt a , UInt b )
{
    VG_(printf)("V h32_binop_tt\n");
}

VG_REGPARM(3) void LK_(h32_binop_cc) ( IRStmt *stmt, UInt a , UInt b )
{
    VG_(printf)("V h32_binop_cc\n");
}


VG_REGPARM(3) void LK_(h64_binop_tc) ( IRStmt *stmt, ULong a , ULong b )
{
    VG_(printf)("V h64_binop_tc\n");
    if ((a  & 0xffff) == 0xe91b) {
        VG_(printf)("---------------------- found --------------------\n");
    }

}

VG_REGPARM(3) void LK_(h64_binop_ct) ( IRStmt *stmt, ULong a, ULong b )
{
    VG_(printf)("V h64_binop_ct\n");


    if ((a & 0xffff) == 0xe91b) {
        VG_(printf)("---------------------- found --------------------\n");
    }
}

VG_REGPARM(3) void LK_(h64_binop_tt) ( IRStmt *stmt, ULong a , ULong b )
{
    // //ppIRStmt(stmt); vex_printf("0x%x\n", a);
    // H_WRTMP_BOOKKEEPING;

    // VG_(printf)("V h64_binop_tt\n");

    // H_EXIT_EARLY;
    // H_SMT2(smt2_binop_tt);
    // H64_PC_OP;

    // IROp op = stmt->Ist.WrTmp.data->Iex.Binop.op;
    // IRExpr* arg1 = stmt->Ist.WrTmp.data->Iex.Binop.arg1;
    // IRExpr* arg2 = stmt->Ist.WrTmp.data->Iex.Binop.arg2;
    // UInt rtmp1 = arg1->Iex.RdTmp.tmp;
    // UInt rtmp2 = arg2->Iex.RdTmp.tmp;


    // VG_(snprintf)( aTmp1, sizeof(aTmp1), "t%d_%d = ",
    //                ltmp, (ltmp) );
    // VG_(snprintf)( aTmp2, sizeof(aTmp2), " t%d_%d t%d_%d",
    //                rtmp1, (rtmp1),
    //                rtmp2, (rtmp2) );
    // H64_PRINT_OP(a,b);

    // // Information flow
    // VG_(printf)( "t%d_%d <- t%d_%d\n", ltmp, (ltmp), rtmp2, (rtmp2) );

}

VG_REGPARM(3) void LK_(h64_binop_cc) ( IRStmt *stmt, ULong a, ULong b )
{
    VG_(printf)("V h64_binop_cc\n");
}


VG_REGPARM(3)
void LK_(h32_get) (
   IRStmt *clone,
   UInt value,
   UInt taint ) {
    VG_(printf)("V h32_get\n");
}

VG_REGPARM(3)
void LK_(h64_get) (
   IRStmt *clone,
   ULong value,
   ULong taint ) {
    VG_(printf)("V h64_get\n");
}


/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */

/*
  Local Variables:
  compile-command:"gcc parse.c -o parse"
  mode:c++
  c-basic-offset:4
  c-file-style:"bsd"
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
