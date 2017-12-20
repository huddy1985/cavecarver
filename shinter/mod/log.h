#ifndef _log_header_h_
#define _log_header_h_

struct interpose  {
  spinlock_t lock;

  /* debugfs entries */
  struct dentry *dent_u;
  int rcnt, monitor;
  struct list_head readers;
};

int  interpose_text_init(struct interpose *sim);
void interpose_text_exit(struct interpose *sim);


#endif
