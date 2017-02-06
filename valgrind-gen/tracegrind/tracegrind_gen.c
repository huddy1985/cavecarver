#include "tracegrind.h"
#include "distorm.h"


/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
/****************************************************************/

void traverse_expr(LCEnv *mce, IRExpr *e) {

    int i;
    IRDirty*   di;
    if (!e)
        return;

    switch( e->tag ){

    case Iex_Get:
        di = unsafeIRDirty_0_N( 0, "print_Get",
                                VG_(fnptr_to_fnentry)( &TR_(print_Get) ),
                                mkIRExprVec_2(mkIRExpr_HWord(e->Iex.Get.offset),
                                              mkIRExpr_HWord(sizeofIRType(e->Iex.Get.ty)))
            );
        /* if using IRExpr_BBPTR():
           patch VEC/priv/ir_defs.c at "but no fxState declared"
         */

        addStmtToIRSB( mce->sb, IRStmt_Dirty(di) );



        break;
    case Iex_GetI:
        break;

    case Iex_RdTmp:
        break;

    case Iex_Qop:
        if (e->Iex.Qop.details) {
            traverse_expr(mce, e->Iex.Qop.details->arg1);
            traverse_expr(mce, e->Iex.Qop.details->arg2);
            traverse_expr(mce, e->Iex.Qop.details->arg3);
            traverse_expr(mce, e->Iex.Qop.details->arg4);
        }
        break;

    case Iex_Triop:
        if (e->Iex.Triop.details) {
            traverse_expr(mce, e->Iex.Triop.details->arg1);
            traverse_expr(mce, e->Iex.Triop.details->arg2);
            traverse_expr(mce, e->Iex.Triop.details->arg3);
        }
        break;

    case Iex_Binop:
        traverse_expr(mce, e->Iex.Binop.arg1);
        traverse_expr(mce, e->Iex.Binop.arg2);
        break;

    case Iex_Unop:
        traverse_expr(mce, e->Iex.Unop.arg);
        break;

    case Iex_Load:
        traverse_expr(mce, e->Iex.Load.addr);
        break;

    case Iex_CCall:
        if (e->Iex.CCall.args) {
            for (i = 0; e->Iex.CCall.args[i]; i++) {
                traverse_expr(mce, e->Iex.CCall.args[i]);
            }
        }
        break;
    case Iex_ITE:
        traverse_expr(mce, e->Iex.ITE.cond);
        traverse_expr(mce, e->Iex.ITE.iftrue);
        traverse_expr(mce, e->Iex.ITE.iffalse);
        break;
    case Iex_Const:
    case Iex_Binder:
    case Iex_VECRET:
    case Iex_BBPTR:
        break;
    default:
        VG_(printf)("\n");
        ppIRExpr(e);
        VG_(printf)("\n");
        VG_(tool_panic)("tr_expr.c: Unhandled expression");
        break;
    }
}


void traverse_stmt(LCEnv *mce, IRStmt* st) {

    switch (st->tag) {

    case Ist_Put:
        break;
    case Ist_PutI:
        break;

    case Ist_NoOp:
    case Ist_AbiHint:
    case Ist_MBE:
        break;
    case Ist_IMark:
        break;
    case Ist_WrTmp:
        traverse_expr(mce, st->Ist.WrTmp.data) ;
        break;
    case Ist_Store:
        traverse_expr(mce, st->Ist.Store.addr) ;
        traverse_expr(mce, st->Ist.Store.data) ;
        break;
    case Ist_StoreG:
        traverse_expr(mce, st->Ist.StoreG.details->addr) ;
        traverse_expr(mce, st->Ist.StoreG.details->data) ;
        traverse_expr(mce, st->Ist.StoreG.details->guard) ;
        break;
    case Ist_LoadG:
        traverse_expr(mce, st->Ist.LoadG.details->addr) ;
        traverse_expr(mce, st->Ist.LoadG.details->alt) ;
        traverse_expr(mce, st->Ist.LoadG.details->guard) ;
        break;
    case Ist_Dirty:
        break;
    case Ist_CAS:
        traverse_expr(mce, st->Ist.CAS.details->addr) ;
        traverse_expr(mce, st->Ist.CAS.details->expdHi) ;
        traverse_expr(mce, st->Ist.CAS.details->expdLo) ;
        traverse_expr(mce, st->Ist.CAS.details->dataHi) ;
        traverse_expr(mce, st->Ist.CAS.details->dataLo) ;
        break;
    case Ist_LLSC:
        traverse_expr(mce, st->Ist.LLSC.addr) ;
        traverse_expr(mce, st->Ist.LLSC.storedata) ;
        break;
    case Ist_Exit:
        traverse_expr(mce, st->Ist.Exit.guard) ;
        break;
    default:
        break;
    }

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
