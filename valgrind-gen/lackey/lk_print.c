#include "lackey.h"

/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
/****************************************************************/
/*************** taintgrind copy ********************************/

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
    //ppIRStmt(stmt); vex_printf("0x%x\n", a);

    VG_(printf)("V h64_binop_tt\n");

    IROp op = stmt->Ist.WrTmp.data->Iex.Binop.op;
    IRExpr* arg1 = stmt->Ist.WrTmp.data->Iex.Binop.arg1;
    IRExpr* arg2 = stmt->Ist.WrTmp.data->Iex.Binop.arg2;
    UInt rtmp1 = arg1->Iex.RdTmp.tmp;
    UInt rtmp2 = arg2->Iex.RdTmp.tmp;


    if ((a & 0xffff) == 0xe91b) {
        VG_(printf)("---------------------- found --------------------\n");
    }

}

VG_REGPARM(3) void LK_(h64_binop_cc) ( IRStmt *stmt, ULong a, ULong b )
{
    VG_(printf)("V h64_binop_cc\n");
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
