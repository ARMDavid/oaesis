#include "types.h"

void link_in(void);
void link_remove(void);
void set_stack(void *addr);
void newmvec(void);
void newbvec(void);
void newtvec(void);
void vdicall(void *contrl);
void accstart(void);
void VsetScreen(void *log,void *phys,WORD mode,WORD modecode);
WORD VsetMode(WORD mode);
