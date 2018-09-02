
#include "tracegrind.h"
#include "distorm.h"
#include "pub_tool_wordfm.h"

/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

/* Command line options controlling instrumentation kinds, as described at
 * the top of this file. */
static Bool clo_basic_counts    = True;
static Bool clo_detailed_counts = False;
static Bool clo_trace_mem       = True;
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
      return VG_(replacement_malloc_process_cmd_line_option)(arg);

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

//------------------------------------------------------------//
//--- an Interval Tree of live blocks                      ---//
//------------------------------------------------------------//

#define HISTOGRAM_SIZE_LIMIT 1024
#define FD_MAX 256
#define FD_MAX_PATH 256
#define FD_READ 0x1
#define FD_WRITE 0x2
#define FD_STAT 0x4

// Number of guest instructions executed so far.  This is
// incremented directly from the generated code.
static ULong g_guest_instrs_executed = 0;

// Summary statistics for the entire run.
static ULong g_tot_blocks = 0;   // total blocks allocated
static ULong g_tot_bytes  = 0;   // total bytes allocated

static ULong g_cur_blocks_live = 0; // curr # blocks live
static ULong g_cur_bytes_live  = 0; // curr # bytes live

static ULong g_max_blocks_live = 0; // bytes and blocks at
static ULong g_max_bytes_live  = 0; // the max residency point

/* Tracks information about live blocks. */
typedef
   struct {
      Addr        payload;
      SizeT       req_szB;
      ExeContext* ap;  /* allocation ec */
      ULong       allocd_at; /* instruction number */
      ULong       n_reads;
      ULong       n_writes;
      /* Approx histogram, one byte per payload byte.  Counts latch up
         therefore at 0xFFFF.  Can be NULL if the block is resized or if
         the block is larger than HISTOGRAM_SIZE_LIMIT. */
      UShort*     histoW; /* [0 .. req_szB-1] */
   }
   Block;

/* May not contain zero-sized blocks.  May not contain
   overlapping blocks. */
static WordFM* interval_tree = NULL;  /* WordFM* Block* void */

/* Here's the comparison function.  Since the tree is required
to contain non-zero sized, non-overlapping blocks, it's good
enough to consider any overlap as a match. */
static Word interval_tree_Cmp ( UWord k1, UWord k2 )
{
   Block* b1 = (Block*)k1;
   Block* b2 = (Block*)k2;
   tl_assert(b1->req_szB > 0);
   tl_assert(b2->req_szB > 0);
   if (b1->payload + b1->req_szB <= b2->payload) return -1;
   if (b2->payload + b2->req_szB <= b1->payload) return  1;
   return 0;
}

// 2-entry cache for find_Block_containing
static Block* fbc_cache0 = NULL;
static Block* fbc_cache1 = NULL;

static UWord stats__n_fBc_cached = 0;
static UWord stats__n_fBc_uncached = 0;
static UWord stats__n_fBc_notfound = 0;

static Block* find_Block_containing ( Addr a )
{
   if (LIKELY(fbc_cache0
              && fbc_cache0->payload <= a
              && a < fbc_cache0->payload + fbc_cache0->req_szB)) {
      // found at 0
      stats__n_fBc_cached++;
      return fbc_cache0;
   }
   if (LIKELY(fbc_cache1
              && fbc_cache1->payload <= a
              && a < fbc_cache1->payload + fbc_cache1->req_szB)) {
      // found at 1; swap 0 and 1
      Block* tmp = fbc_cache0;
      fbc_cache0 = fbc_cache1;
      fbc_cache1 = tmp;
      stats__n_fBc_cached++;
      return fbc_cache0;
   }
   Block fake;
   fake.payload = a;
   fake.req_szB = 1;
   UWord foundkey = 1;
   UWord foundval = 1;
   Bool found = VG_(lookupFM)( interval_tree,
                               &foundkey, &foundval, (UWord)&fake );
   if (!found) {
      stats__n_fBc_notfound++;
      return NULL;
   }
   tl_assert(foundval == 0); // we don't store vals in the interval tree
   tl_assert(foundkey != 1);
   Block* res = (Block*)foundkey;
   tl_assert(res != &fake);
   // put at the top position
   fbc_cache1 = fbc_cache0;
   fbc_cache0 = res;
   stats__n_fBc_uncached++;
   return res;
}

// delete a block; asserts if not found.  (viz, 'a' must be
// known to be present.)
static void delete_Block_starting_at ( Addr a )
{
   Block fake;
   fake.payload = a;
   fake.req_szB = 1;
   Bool found = VG_(delFromFM)( interval_tree,
                                NULL, NULL, (Addr)&fake );
   tl_assert(found);
   fbc_cache0 = fbc_cache1 = NULL;
}


//------------------------------------------------------------//
//--- a FM of allocation points (APs)                      ---//
//------------------------------------------------------------//

typedef
   struct {
      // the allocation point that we're summarising stats for
      ExeContext* ap;
      // used when printing results
      Bool shown;
      // The current number of blocks and bytes live for this AP
      ULong cur_blocks_live;
      ULong cur_bytes_live;
      // The number of blocks and bytes live at the max-liveness
      // point.  Note this is a bit subtle.  max_blocks_live is not
      // the maximum number of live blocks, but rather the number of
      // blocks live at the point of maximum byte liveness.  These are
      // not necessarily the same thing.
      ULong max_blocks_live;
      ULong max_bytes_live;
      // Total number of blocks and bytes allocated by this AP.
      ULong tot_blocks;
      ULong tot_bytes;
      // Sum of death ages for all blocks allocated by this AP,
      // that have subsequently been freed.
      ULong death_ages_sum;
      ULong deaths;
      // Total number of reads and writes in all blocks allocated
      // by this AP.
      ULong n_reads;
      ULong n_writes;
      /* Histogram information.  We maintain a histogram aggregated for
         all retiring Blocks allocated by this AP, but only if:
         - this AP has only ever allocated objects of one size
         - that size is <= HISTOGRAM_SIZE_LIMIT
         What we need therefore is a mechanism to see if this AP
         has only ever allocated blocks of one size.

         3 states:
            Unknown          because no retirement yet
            Exactly xsize    all retiring blocks are of this size
            Mixed            multiple different sizes seen
      */
      enum { Unknown=999, Exactly, Mixed } xsize_tag;
      SizeT xsize;
      UInt* histo; /* [0 .. xsize-1] */
   }
   APInfo;

/* maps ExeContext*'s to APInfo*'s.  Note that the keys must match the
   .ap field in the values. */
static WordFM* apinfo = NULL;  /* WordFM* ExeContext* APInfo* */


/* 'bk' is being introduced (has just been allocated).  Find the
   relevant APInfo entry for it, or create one, based on the block's
   allocation EC.  Then, update the APInfo to the extent that we
   actually can, to reflect the allocation. */
static void intro_Block ( Block* bk )
{
   tl_assert(bk);
   tl_assert(bk->ap);

   APInfo* api   = NULL;
   UWord   keyW  = 0;
   UWord   valW  = 0;
   Bool    found = VG_(lookupFM)( apinfo,
                                  &keyW, &valW, (UWord)bk->ap );
   if (found) {
      api = (APInfo*)valW;
      tl_assert(keyW == (UWord)bk->ap);
   } else {
      api = VG_(malloc)( "dh.main.intro_Block.1", sizeof(APInfo) );
      VG_(memset)(api, 0, sizeof(*api));
      api->ap = bk->ap;
      Bool present = VG_(addToFM)( apinfo,
                                   (UWord)bk->ap, (UWord)api );
      tl_assert(!present);
      // histo stuff
      tl_assert(api->deaths == 0);
      api->xsize_tag = Unknown;
      api->xsize = 0;
      if (0) VG_(printf)("api %p   -->  Unknown\n", api);
   }

   tl_assert(api->ap == bk->ap);

   /* So: update stats to reflect an allocation */

   // # live blocks
   api->cur_blocks_live++;

   // # live bytes
   api->cur_bytes_live += bk->req_szB;
   if (api->cur_bytes_live > api->max_bytes_live) {
      api->max_bytes_live  = api->cur_bytes_live;
      api->max_blocks_live = api->cur_blocks_live;
   }

   // total blocks and bytes allocated here
   api->tot_blocks++;
   api->tot_bytes += bk->req_szB;

   // update summary globals
   g_tot_blocks++;
   g_tot_bytes += bk->req_szB;

   g_cur_blocks_live++;
   g_cur_bytes_live += bk->req_szB;
   if (g_cur_bytes_live > g_max_bytes_live) {
      g_max_bytes_live = g_cur_bytes_live;
      g_max_blocks_live = g_cur_blocks_live;
   }
}


/* 'bk' is retiring (being freed).  Find the relevant APInfo entry for
   it, which must already exist.  Then, fold info from 'bk' into that
   entry.  'because_freed' is True if the block is retiring because
   the client has freed it.  If it is False then the block is retiring
   because the program has finished, in which case we want to skip the
   updates of the total blocks live etc for this AP, but still fold in
   the access counts and histo data that have so far accumulated for
   the block. */
static void retire_Block ( Block* bk, Bool because_freed )
{
   tl_assert(bk);
   tl_assert(bk->ap);

   APInfo* api   = NULL;
   UWord   keyW  = 0;
   UWord   valW  = 0;
   Bool    found = VG_(lookupFM)( apinfo,
                                  &keyW, &valW, (UWord)bk->ap );

   tl_assert(found);
   api = (APInfo*)valW;
   tl_assert(api->ap == bk->ap);

   // update stats following this free.
   if (0)
   VG_(printf)("ec %p  api->c_by_l %llu  bk->rszB %llu\n",
               bk->ap, api->cur_bytes_live, (ULong)bk->req_szB);

   // update total blocks live etc for this AP
   if (because_freed) {
      tl_assert(api->cur_blocks_live >= 1);
      tl_assert(api->cur_bytes_live >= bk->req_szB);
      api->cur_blocks_live--;
      api->cur_bytes_live -= bk->req_szB;

      api->deaths++;

      tl_assert(bk->allocd_at <= g_guest_instrs_executed);
      api->death_ages_sum += (g_guest_instrs_executed - bk->allocd_at);

      // update global summary stats
      tl_assert(g_cur_blocks_live > 0);
      g_cur_blocks_live--;
      tl_assert(g_cur_bytes_live >= bk->req_szB);
      g_cur_bytes_live -= bk->req_szB;
   }

   // access counts
   api->n_reads  += bk->n_reads;
   api->n_writes += bk->n_writes;

   // histo stuff.  First, do state transitions for xsize/xsize_tag.
   switch (api->xsize_tag) {

      case Unknown:
         tl_assert(api->xsize == 0);
         tl_assert(api->deaths == 1 || api->deaths == 0);
         tl_assert(!api->histo);
         api->xsize_tag = Exactly;
         api->xsize = bk->req_szB;
         if (0) VG_(printf)("api %p   -->  Exactly(%lu)\n", api, api->xsize);
         // and allocate the histo
         if (bk->histoW) {
            api->histo = VG_(malloc)("dh.main.retire_Block.1",
                                     api->xsize * sizeof(UInt));
            VG_(memset)(api->histo, 0, api->xsize * sizeof(UInt));
         }
         break;

      case Exactly:
         //tl_assert(api->deaths > 1);
         if (bk->req_szB != api->xsize) {
            if (0) VG_(printf)("api %p   -->  Mixed(%lu -> %lu)\n",
                               api, api->xsize, bk->req_szB);
            api->xsize_tag = Mixed;
            api->xsize = 0;
            // deallocate the histo, if any
            if (api->histo) {
               VG_(free)(api->histo);
               api->histo = NULL;
            }
         }
         break;

      case Mixed:
         //tl_assert(api->deaths > 1);
         break;

      default:
        tl_assert(0);
   }

   // See if we can fold the histo data from this block into
   // the data for the AP
   if (api->xsize_tag == Exactly && api->histo && bk->histoW) {
      tl_assert(api->xsize == bk->req_szB);
      UWord i;
      for (i = 0; i < api->xsize; i++) {
         // FIXME: do something better in case of overflow of api->histo[..]
         // Right now, at least don't let it overflow/wrap around
         if (api->histo[i] <= 0xFFFE0000)
            api->histo[i] += (UInt)bk->histoW[i];
      }
      if (0) VG_(printf)("fold in, AP = %p\n", api);
   }



#if 0
   if (bk->histoB) {
      VG_(printf)("block retiring, histo %lu: ", bk->req_szB);
      UWord i;
      for (i = 0; i < bk->req_szB; i++)
        VG_(printf)("%u ", (UInt)bk->histoB[i]);
      VG_(printf)("\n");
   } else {
      VG_(printf)("block retiring, no histo %lu\n", bk->req_szB);
   }
#endif
}

/* This handles block resizing.  When a block with AP 'ec' has a
   size change of 'delta', call here to update the APInfo. */
static void apinfo_change_cur_bytes_live( ExeContext* ec, Long delta )
{
   APInfo* api   = NULL;
   UWord   keyW  = 0;
   UWord   valW  = 0;
   Bool    found = VG_(lookupFM)( apinfo,
                                  &keyW, &valW, (UWord)ec );

   tl_assert(found);
   api = (APInfo*)valW;
   tl_assert(api->ap == ec);

   if (delta < 0) {
      tl_assert(api->cur_bytes_live >= -delta);
      tl_assert(g_cur_bytes_live >= -delta);
   }

   // adjust current live size
   api->cur_bytes_live += delta;
   g_cur_bytes_live += delta;

   if (delta > 0 && api->cur_bytes_live > api->max_bytes_live) {
      api->max_bytes_live  = api->cur_bytes_live;
      api->max_blocks_live = api->cur_blocks_live;
   }

   // update global summary stats
   if (delta > 0 && g_cur_bytes_live > g_max_bytes_live) {
      g_max_bytes_live = g_cur_bytes_live;
      g_max_blocks_live = g_cur_blocks_live;
   }
   if (delta > 0)
      g_tot_bytes += delta;

   // adjust total allocation size
   if (delta > 0)
      api->tot_bytes += delta;
}


//------------------------------------------------------------//
//--- update both Block and APInfos after {m,re}alloc/free ---//
//------------------------------------------------------------//

static
void* new_block ( ThreadId tid, void* p, SizeT req_szB, SizeT req_alignB,
                  Bool is_zeroed )
{
   tl_assert(p == NULL); // don't handle custom allocators right now
   SizeT actual_szB /*, slop_szB*/;

   if ((SSizeT)req_szB < 0) return NULL;

   if (req_szB == 0)
      req_szB = 1;  /* can't allow zero-sized blocks in the interval tree */

   // Allocate and zero if necessary
   if (!p) {
      p = VG_(cli_malloc)( req_alignB, req_szB );
      if (!p) {
         return NULL;
      }
      if (is_zeroed) VG_(memset)(p, 0, req_szB);
      actual_szB = VG_(cli_malloc_usable_size)(p);
      tl_assert(actual_szB >= req_szB);
      /* slop_szB = actual_szB - req_szB; */
   } else {
      /* slop_szB = 0; */
   }

   // Make new HP_Chunk node, add to malloc_list
   Block* bk = VG_(malloc)("dh.new_block.1", sizeof(Block));
   bk->payload   = (Addr)p;
   bk->req_szB   = req_szB;
   bk->ap        = VG_(record_ExeContext)(tid, 0/*first word delta*/);
   bk->allocd_at = g_guest_instrs_executed;
   bk->n_reads   = 0;
   bk->n_writes  = 0;
   // set up histogram array, if the block isn't too large
   bk->histoW = NULL;
   if (req_szB <= HISTOGRAM_SIZE_LIMIT) {
      bk->histoW = VG_(malloc)("dh.new_block.2", req_szB * sizeof(UShort));
      VG_(memset)(bk->histoW, 0, req_szB * sizeof(UShort));
   }

   Bool present = VG_(addToFM)( interval_tree, (UWord)bk, (UWord)0/*no val*/);
   tl_assert(!present);
   fbc_cache0 = fbc_cache1 = NULL;

   intro_Block(bk);

   if (0) VG_(printf)("ALLOC %lu -> %p\n", req_szB, p);

   return p;
}

static
void die_block ( void* p, Bool custom_free )
{
   tl_assert(!custom_free);  // at least for now

   Block* bk = find_Block_containing( (Addr)p );

   if (!bk) {
     return; // bogus free
   }

   tl_assert(bk->req_szB > 0);
   // assert the block finder is behaving sanely
   tl_assert(bk->payload <= (Addr)p);
   tl_assert( (Addr)p < bk->payload + bk->req_szB );

   if (bk->payload != (Addr)p) {
      return; // bogus free
   }

   if (0) VG_(printf)(" FREE %p %llu\n",
                      p, g_guest_instrs_executed - bk->allocd_at);

   retire_Block(bk, True/*because_freed*/);

   VG_(cli_free)( (void*)bk->payload );
   delete_Block_starting_at( bk->payload );
   if (bk->histoW) {
      VG_(free)( bk->histoW );
      bk->histoW = NULL;
   }
   VG_(free)( bk );
}


static
void* renew_block ( ThreadId tid, void* p_old, SizeT new_req_szB )
{
   if (0) VG_(printf)("REALL %p %lu\n", p_old, new_req_szB);
   void* p_new = NULL;

   tl_assert(new_req_szB > 0); // map 0 to 1

   // Find the old block.
   Block* bk = find_Block_containing( (Addr)p_old );
   if (!bk) {
      return NULL;   // bogus realloc
   }

   tl_assert(bk->req_szB > 0);
   // assert the block finder is behaving sanely
   tl_assert(bk->payload <= (Addr)p_old);
   tl_assert( (Addr)p_old < bk->payload + bk->req_szB );

   if (bk->payload != (Addr)p_old) {
      return NULL; // bogus realloc
   }

   // Keeping the histogram alive in any meaningful way across
   // block resizing is too darn complicated.  Just throw it away.
   if (bk->histoW) {
      VG_(free)(bk->histoW);
      bk->histoW = NULL;
   }

   // Actually do the allocation, if necessary.
   if (new_req_szB <= bk->req_szB) {

      // New size is smaller or same; block not moved.
      apinfo_change_cur_bytes_live(bk->ap,
                                   (Long)new_req_szB - (Long)bk->req_szB);
      bk->req_szB = new_req_szB;
      return p_old;

   } else {

      // New size is bigger;  make new block, copy shared contents, free old.
      p_new = VG_(cli_malloc)(VG_(clo_alignment), new_req_szB);
      if (!p_new) {
         // Nb: if realloc fails, NULL is returned but the old block is not
         // touched.  What an awful function.
         return NULL;
      }
      tl_assert(p_new != p_old);

      VG_(memcpy)(p_new, p_old, bk->req_szB);
      VG_(cli_free)(p_old);

      // Since the block has moved, we need to re-insert it into the
      // interval tree at the new place.  Do this by removing
      // and re-adding it.
      delete_Block_starting_at( (Addr)p_old );
      // now 'bk' is no longer in the tree, but the Block itself
      // is still alive

      // Update the metadata.
      apinfo_change_cur_bytes_live(bk->ap,
                                   (Long)new_req_szB - (Long)bk->req_szB);
      bk->payload = (Addr)p_new;
      bk->req_szB = new_req_szB;

      // and re-add
      Bool present
         = VG_(addToFM)( interval_tree, (UWord)bk, (UWord)0/*no val*/);
      tl_assert(!present);
      fbc_cache0 = fbc_cache1 = NULL;

      return p_new;
   }
   /*NOTREACHED*/
   tl_assert(0);
}

//------------------------------------------------------------//
//--- malloc() et al replacement wrappers                  ---//
//------------------------------------------------------------//

static void* lk_malloc ( ThreadId tid, SizeT szB )
{
   return new_block( tid, NULL, szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* lk___builtin_new ( ThreadId tid, SizeT szB )
{
   return new_block( tid, NULL, szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* lk___builtin_vec_new ( ThreadId tid, SizeT szB )
{
   return new_block( tid, NULL, szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* lk_calloc ( ThreadId tid, SizeT m, SizeT szB )
{
   return new_block( tid, NULL, m*szB, VG_(clo_alignment), /*is_zeroed*/True );
}

static void *lk_memalign ( ThreadId tid, SizeT alignB, SizeT szB )
{
   return new_block( tid, NULL, szB, alignB, False );
}

static void lk_free ( ThreadId tid __attribute__((unused)), void* p )
{
   die_block( p, /*custom_free*/False );
}

static void lk___builtin_delete ( ThreadId tid, void* p )
{
   die_block( p, /*custom_free*/False);
}

static void lk___builtin_vec_delete ( ThreadId tid, void* p )
{
   die_block( p, /*custom_free*/False );
}

static void* lk_realloc ( ThreadId tid, void* p_old, SizeT new_szB )
{
   if (p_old == NULL) {
      return lk_malloc(tid, new_szB);
   }
   if (new_szB == 0) {
      lk_free(tid, p_old);
      return NULL;
   }
   return renew_block(tid, p_old, new_szB);
}

static SizeT lk_malloc_usable_size ( ThreadId tid, void* p )
{
   Block* bk = find_Block_containing( (Addr)p );
   return bk ? bk->req_szB : 0;
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

#define ENABLE_OUTPUT 1
#define DISABLE_PRINTF

static Event events[N_EVENTS];
static Int   events_used = 0;
//static VgFile *bin_file = 0;
static int enable_trace = 1;
static unsigned long inscnt = 0;
static int out_reached = 0;

static VG_REGPARM(2) void trace_instr(Addr addr, SizeT size)
{
  int i;
  const unsigned char*p = (const unsigned char*)(uint8_t*)addr;
  if (!(out_reached &&
        p[0] == 0x48 &&
        p[1] == 0xf7 ))
      return;



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

#ifndef DISABLE_PRINTF
    VG_(printf)("I  %llx %-24s %s%s%s\r\n", (long long)decodedInstructions[0].offset,
	   (char*)decodedInstructions[0].instructionHex.p,
	   (char*)decodedInstructions[0].mnemonic.p, decodedInstructions[0].operands.length != 0 ? " " : "",
	   (char*)decodedInstructions[0].operands.p);
#endif

    if (VG_(strncmp)(decodedInstructions[0].mnemonic.p, "IMUL", 4) == 0) {
        ExeContext *ec = VG_(record_ExeContext)( VG_(get_running_tid)(), 0 );
        //ec->n_ips = 32;
        VG_(pp_ExeContext) ( ec );
        VG_(printf)("--- addr: 0x%x\n", addr);

        VG_(printf)("I %llx %llx %-24s %s%s%s\r\n", inscnt, (long long)decodedInstructions[0].offset,
	   (char*)decodedInstructions[0].instructionHex.p,
	   (char*)decodedInstructions[0].mnemonic.p, decodedInstructions[0].operands.length != 0 ? " " : "",
	   (char*)decodedInstructions[0].operands.p);


    }

  }


  inscnt++;

  //VG_(printf)("\n");
}

void print_mem(char *mem, int len) {
    int i;
    for (i = 0; i < len; i++) {
        VG_(printf)("%02x", ((unsigned char*)mem)[i]);
    }
}

/* Return true iff [aSmall,aSmall+nSmall) is entirely contained
   within [aBig,aBig+nBig). */
inline
static Bool is_subinterval_of ( Addr aBig, SizeT nBig,
                                Addr aSmall, SizeT nSmall ) {
   tl_assert(nBig > 0 && nSmall > 0);
   return aBig <= aSmall && aSmall + nSmall <= aBig + nBig;
}

static const char searchfor[] = {
    0x62 ,0x70 ,0x30 ,0xef ,0x17 ,0xba ,0x58 ,0xb3
};

static search_address_reagion(Addr addr) {
   Block* bk = find_Block_containing(addr);
   if (bk) {
       if (is_subinterval_of(bk->payload, bk->req_szB, addr-7, 8)) {
           int i = 0;
           for (i = 0; i <= 8; i++) {
               if ((VG_(memcmp)(((char*)addr)+ i-7, searchfor, 8) == 0)) {
                   //char b[1];
                   VG_(printf)(" vvvvvvvvvv -----------  found read mem:%08lx   --------\n", addr);
                   ExeContext *ec = VG_(record_ExeContext)( VG_(get_running_tid)(), 0 );
                   //ec->n_ips = 32;
                   VG_(pp_ExeContext) ( ec );
                   //while(1) {}
                   VG_(printf)(" ^^^^^^^^^^\n");
               }
           }
       }
   }
}


static VG_REGPARM(2) void trace_load(Addr addr, SizeT size)
{

    //if (out_reached)
    {
        search_address_reagion(addr);
    }

#ifndef DISABLE_PRINTF
    VG_(printf)("> LDD %08lx,%lu: ", addr, size);
    print_mem(addr, size);
    VG_(printf)(" \n");
#endif
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size)
{
    // if (out_reached)
    {
        search_address_reagion(addr);
    }
#ifndef DISABLE_PRINTF
    VG_(printf)("< STD %08lx,%lu: ", addr, size);
    print_mem(addr, size);
    VG_(printf)(" \n");
#endif
}

static VG_REGPARM(2) void trace_modify(Addr addr, SizeT size)
{
#ifndef DISABLE_PRINTF
    VG_(printf)("<<<<< M %08lx,%lu: ", addr, size);
    print_mem(addr, size);
    VG_(printf)(" \n");
#endif
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

   flushEvents(sb);
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

   flushEvents(sb);
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

   flushEvents(sb);
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
      flushEvents(sb);
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

   flushEvents(sb);
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

      /**************************/
      traverse_stmt(&mce, st);

      {
          /*
          IRDirty *di2;
          IRStmt *clone = deepMallocIRStmt(st);

          di2 = unsafeIRDirty_0_N( 1, "print_ir",
                                  VG_(fnptr_to_fnentry)( &TR_(print_ir) ),
                                  mkIRExprVec_1(mkIRExpr_HWord((HWord)clone)) );

          addStmtToIRSB( mce.sb, IRStmt_Dirty(di2) );
          */
      }

      /**************************/


      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_PutI:
         case Ist_MBE:
            addStmtToIRSB( sbOut, st );
            break;
         case Ist_Put:
             addStmtToIRSB( sbOut, st );
             /*
             {
             IRDirty*   di_put;
             IRType tyE    = typeOfIRExpr(sbOut->tyenv, st->Ist.Put.data);

             di_put = unsafeIRDirty_0_N( 0, "print_Put",
                                         VG_(fnptr_to_fnentry)( &TR_(print_Put) ),
                                         mkIRExprVec_2(mkIRExpr_HWord(st->Ist.Put.offset),
                                                       mkIRExpr_HWord(sizeofIRType(tyE))));

             addStmtToIRSB( sbOut, IRStmt_Dirty(di_put) );

             }*/
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
               /*addEvent_Ir( sbOut, mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ),
                 st->Ist.IMark.len );*/

                IRExpr**   argv;
                IRDirty*   di;
                argv = mkIRExprVec_2( mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ), mkIRExpr_HWord( st->Ist.IMark.len ) );
                di   = unsafeIRDirty_0_N( /*regparms*/2,
                                          "trace_instr", VG_(fnptr_to_fnentry)( trace_instr ),
                                          argv );
                addStmtToIRSB( sbOut, IRStmt_Dirty(di) );





            }
            addStmtToIRSB( sbOut, st );
            break;

        case Ist_WrTmp: {
            // Add a call to trace_load() if --trace-mem=yes.
            if (clo_trace_mem) {
               IRExpr* data = st->Ist.WrTmp.data;

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
	      break;
            case Iex_Get:
              break;
	    default:
	      break;
	    }

            addStmtToIRSB( sbOut, st );
	    break;
 	 }

         case Ist_Store: {
            IRExpr* data = st->Ist.Store.data;
            IRType  type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpStore, type, NULL/*guard*/ );
            }
            addStmtToIRSB( sbOut, st );
            if (clo_trace_mem) {
               addEvent_Dw( sbOut, st->Ist.Store.addr,
                            sizeofIRType(type) );
            }
            break;
         }

         case Ist_StoreG: {
            IRStoreG* sg   = st->Ist.StoreG.details;
            IRExpr*   data = sg->data;
            IRType    type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpStore, type, sg->guard );
            }
            addStmtToIRSB( sbOut, st );
            if (clo_trace_mem) {
               addEvent_Dw_guarded( sbOut, sg->addr,
                                    sizeofIRType(type), sg->guard );
            }
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
             Int      dsize;
             IRDirty* d = st->Ist.Dirty.details;

             if (d->mFx != Ifx_None) {
                  // This dirty helper accesses memory.  Collect the details.
                  tl_assert(d->mAddr != NULL);
                  tl_assert(d->mSize != 0);
                  dsize = d->mSize;
                  if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                     addEvent_Dr( sbOut, d->mAddr, dsize );
                  /*if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                    addEvent_Dw( sbOut, d->mAddr, dsize );*/
             } else {
                 tl_assert(d->mAddr == NULL);
                 tl_assert(d->mSize == 0);
             }

             addStmtToIRSB( sbOut, st );

             if (d->mFx != Ifx_None) {
                 dsize = d->mSize;
                 if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                     addEvent_Dr( sbOut, d->mAddr, dsize );
             }
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
            addEvent_Dr( sbOut, cas->addr, dataSize );

            if (clo_detailed_counts) {
               instrument_detail( sbOut, OpLoad, dataTy, NULL/*guard*/ );
               if (cas->dataHi != NULL) /* dcas */
                  instrument_detail( sbOut, OpLoad, dataTy, NULL/*guard*/ );
               instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
               if (cas->dataHi != NULL) /* dcas */
                  instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
            }
            addStmtToIRSB( sbOut, st );

            addEvent_Dw( sbOut, cas->addr, dataSize );

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
               addStmtToIRSB( sbOut, st );
            } else {
                addStmtToIRSB( sbOut, st );

                /* SC */
                dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
               if (clo_trace_mem)
                  addEvent_Dw( sbOut, st->Ist.LLSC.addr,
                                      sizeofIRType(dataTy) );
               if (clo_detailed_counts)
                  instrument_detail( sbOut, OpStore, dataTy, NULL/*guard*/ );
            }
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

static void lk_syscall_write(ThreadId tid, UWord* args, UInt nArgs, SysRes res) {
    if (args[0] == 1) {
        VG_(printf)("W stdout:\n");
        out_reached = 1;
    }
}

static void lk_syscall_open(ThreadId tid, UWord* args, UInt nArgs, SysRes res) {

   HChar fdpath[FD_MAX_PATH];
   Int fd = sr_Res(res);
   Bool verbose = False;
   (void)verbose;
   resolve_filename(fd, fdpath, FD_MAX_PATH-1);
   if (!VG_(strcmp)(fdpath, "/mnt/usb/logfiles/vivado_2018.1/microblaze_mcs_err.vhd")) {
       enable_trace = 1;
       VG_(printf)("O %d:%s\n", fd,fdpath);
   }
   //VG_(printf)("O %d:%s\n", fd,fdpath);
}

static void lk_syscall_mmap(ThreadId tid, UWord* args, UInt nArgs, SysRes res) {

    //VG_(printf)("M %p:l=%d:p=0x%x:f=0x%x:fd=%d:off:0x%x=>0x%p\n", (void*)args[0],(int)args[1],(int)args[2],(int)args[3],(int)args[4],(int)args[5], (void*)sr_Res(res));

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
      lk_syscall_write(tid, args, nArgs, res);
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

   VG_(needs_libc_freeres)();
   VG_(needs_client_requests)     (lk_handle_client_request);
   VG_(needs_command_line_options)(lk_process_cmd_line_option,
                                   lk_print_usage,
                                   lk_print_debug_usage);

   VG_(needs_malloc_replacement)  (lk_malloc,
                                   lk___builtin_new,
                                   lk___builtin_vec_new,
                                   lk_memalign,
                                   lk_calloc,
                                   lk_free,
                                   lk___builtin_delete,
                                   lk___builtin_vec_delete,
                                   lk_realloc,
                                   lk_malloc_usable_size,
                                   0 );

   //VG_(track_new_mem_mmap)        ( lk_new_mem_mmap );

   tl_assert(!interval_tree);
   tl_assert(!fbc_cache0);
   tl_assert(!fbc_cache1);


   interval_tree = VG_(newFM)( VG_(malloc),
                               "dh.main.interval_tree.1",
                               VG_(free),
                               interval_tree_Cmp );


   apinfo = VG_(newFM)( VG_(malloc),
                        "dh.main.apinfo.1",
                        VG_(free),
                        NULL/*unboxedcmp*/ );

   VG_(needs_syscall_wrapper)     ( lk_pre_syscall,
                                    lk_post_syscall );


   //VG_(clo_vex_control).iropt_register_updates_default = VexRegUpdAllregsAtEachInsn;

}

VG_DETERMINE_INTERFACE_VERSION(lk_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                lk_main.c ---*/
/*--------------------------------------------------------------------*/

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
