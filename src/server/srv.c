/****************************************************************************

 Module
  srv.c
  
 Description
  Window, menu, and application server in oAESis.
  
 Author(s)
 	cg (Christer Gustavsson <d2cg@dtek.chalmers.se>)
 	
 Revision history
 
  960623 cg
   Creation date.
 
 Copyright notice
  The copyright to the program code herein belongs to the authors. It may
  be freely duplicated and distributed without fee, but not charged for.
 
 ****************************************************************************/

/*
#define SRV_DEBUG
*/

/****************************************************************************
 * Used interfaces                                                          *
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_BASEPAGE_H
#include <basepage.h>
#endif

#ifdef HAVE_MINTBIND_H
#include <mintbind.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "oconfig.h"
#include "debug.h"
#include "gemdefs.h"
#include "srv_global.h"
#include "lxgemdos.h"
#include "mesagdef.h"
#include "mintdefs.h"
#include "srv_misc.h"
/*#include "objc.h"*/
#include "resource.h"
#include "rlist.h"
#include "srv.h"
#include "srv_get.h"
#include "srv_interface.h"
#include "types.h"
#include "vdi.h"


/****************************************************************************
 * Macros                                                                   *
 ****************************************************************************/


#define SRV_QUE_SIZE 32

/* appl_* related */

#define TOP_APPL       (-1)
#define DESK_OWNER     (-2)
#define TOP_MENU_OWNER (-3)

/*window status codes*/

#define	WIN_OPEN       0x0001
#define	WIN_UNTOPPABLE 0x0002
#define	WIN_DESKTOP    0x0004
#define	WIN_TOPPED     0x0008
#define	WIN_DIALOG     0x0010
#define	WIN_MENU       0x0020
#define WIN_ICONIFIED  0x0040

#define IMOVER 0x8000  /* Used with set_win_elem() to make icon window */

#define D3DSIZE       2

#define INTS2LONG(a,b) (((((LONG)a)<<16)&0xffff0000L)|(((LONG)b)&0xffff))


/****************************************************************************
 * Typedefs of module global interest                                       *
 ****************************************************************************/


/****************************************************************************
 * Module global variables                                                  *
 ****************************************************************************/

/* appl_* related */

static WORD	next_ap = 0;

static AP_INFO apps[MAX_NUM_APPS];

static AP_LIST *ap_resvd = NULL;

static struct {
	WORD menu_cur_apid;
	WORD winbar;          /* Window handle of the menu bar window         */
} mglob = {
	-1,
	-1
};

static WINLIST *win_list = 0L;
static WINLIST *win_free = 0L;

static WORD	win_next = 0;

static WORD	elemnumber = -1;
static WORD	tednumber;

static OBJC_COLORWORD top_colour[20] = {
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,0,7,LYELLOW},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,0,7,WHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE}
};

static OBJC_COLORWORD untop_colour[20] = {
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,0,7,WHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE},
  {BLACK,BLACK,1,7,LWHITE}
};

static WORD widgetmap[] = {
	0,0,WCLOSER,WMOVER,WFULLER,
	WINFO,0,0,WSIZER,0,
	WUP,WDOWN,WVSB,WVSLIDER,0,
	WLEFT,WRIGHT,WHSB,WHSLIDER,WSMALLER
};

/* Physical VDI workstation id */
static WORD svid;

static BYTE **environ;
static AP_LIST *ap_pri = NULL;
static WINLIST *win_vis = NULL;

/* This has to be fixed */
void accstart (void) {}

/****************************************************************************
 * srv_appl_exit                                                            *
 *  Implementation of appl_exit().                                          *
 ****************************************************************************/
void
srv_appl_exit(      /*                                                      */
C_APPL_EXIT * par,  /* Application id.                                      */
R_APPL_EXIT * ret);
/****************************************************************************/

void
srv_appl_write (C_APPL_WRITE * msg,
                R_APPL_WRITE * ret);

/****************************************************************************
 * srv_wind_find                                                            *
 *  Find window on known coordinates.                                       *
 ****************************************************************************/
WORD           /* Window handle.                                            */
srv_wind_find( /*                                                           */
C_WIND_FIND *msg);
/****************************************************************************/

/****************************************************************************
 * srv_wind_get                                                             *
 *  Implementation of wind_get().                                           *
 ****************************************************************************/
void
srv_wind_get(  /*                                                           */
C_WIND_GET * msg,
R_WIND_GET * ret);
/****************************************************************************/

/****************************************************************************
 * srv_wind_new                                                             *
 *  Implementation of wind_new().                                           *
 ****************************************************************************/
static WORD    /* 0 if error or 1 if ok.                                    */
srv_wind_new(  /*                                                           */
WORD apid);    /* Application whose windows should be erased.               */
/****************************************************************************/

WORD srv_wind_set(WORD apid, C_WIND_SET *msg);

/****************************************************************************
 * draw_wind_elements                                                       *
 *  Draw the elements of the window win that intersects with the rectangle  *
 *  r.                                                                      *
 ****************************************************************************/
static void          /*                                                     */
draw_wind_elements(  /*                                                     */
WINSTRUCT *win,      /* Window description.                                 */
RECT *r,             /* Clipping rectangle.                                 */
WORD start);         /* Start object.                                       */
/****************************************************************************/

/****************************************************************************
 * find_wind_description                                                    *
 *  Find the window structure of the window with identification number id.  *
 ****************************************************************************/
static WINSTRUCT	*     /* Found description or NULL.                       */
find_wind_description(  /*                                                  */
WORD id);               /* Identification number of window.                 */
/****************************************************************************/

/****************************************************************************
 * top_appl                                                                 *
 *  Top application.                                                        *
 ****************************************************************************/
static WORD       /* Previously topped application.                         */
top_appl(         /*                                                        */
WORD apid);       /* Id of application.                                     */
/****************************************************************************/


/****************************************************************************
 * internal_appl_info                                                       *
 *  Get internal information about application.                             *
 ****************************************************************************/
static AP_INFO *    /* Application description or NULL.                     */
internal_appl_info( /*                                                      */
WORD apid)          /* Id of application.                                   */
/****************************************************************************/
{
  switch(apid) {
  case TOP_APPL:
    if(ap_pri) {
      return ap_pri->ai;
    }
    else {
      return NULL;
    };

  case DESK_OWNER:
  {
    AP_LIST *al = ap_pri;
			
    while(al) {
      if(al->ai->deskbg) {
        return al->ai;
      };
				
      al = al->next;
    };
  };			
  return NULL;

  case TOP_MENU_OWNER:
  {
    AP_LIST *al = ap_pri;
			
    while(al) {
      if(al->ai->menu) {
        return al->ai;
      };
				
      al = al->next;
    };
  };		
  return NULL;
	
  default:
    if(apps[apid].id == apid) {
      return &apps[apid];
    };
    return NULL;
  };
}


/* 
 * Find MiNT-PID & return AP_LIST entry for that 
 */
static AP_LIST *search_mpid(WORD pid)
{
	AP_LIST	*al;

	al = ap_pri;
	
	while(al) 
	{
		if( al->ai->pid == pid) 
			break;
		al = al->next;
	}
	return al;	
}

/****************************************************************************
 * srv_info_alloc                                                           *
 *  Reserve structure of internal application information.                  *
 ****************************************************************************/
static AP_INFO *    /* Application description or NULL.                     */
srv_info_alloc(     /*                                                      */
WORD pid,           /* MiNT process id.                                     */
WORD type,          /* Type of application (appl, acc, desktop etc)         */
WORD alloc_only)    /* Should the structure only be allocated?              */
/****************************************************************************/
{
  AP_LIST	*al;
  
  al = (AP_LIST *)Mxalloc(sizeof(AP_LIST),GLOBALMEM);
  
  if(!al) {
    DB_printf("%s: Line %d: srv_info_alloc:\r\n"
	      "out of memory!\r\n",__FILE__,__LINE__);
    return NULL;
  };
  
  while(apps[next_ap].id != -1) {
    next_ap = ((next_ap + 1) % MAX_NUM_APPS);
  };
  
  al->ai = &apps[next_ap];
  
  al->ai->id = next_ap;
  al->ai->pid = pid;
  al->ai->deskbg = NULL;
  al->ai->menu = NULL;
  al->ai->type = type;
  al->ai->newmsg = 0;
  al->ai->killtries = 0;
 
  al->ai->rshdr = 0L;

  /* Message handling initialization */
  al->ai->next_message = 0;
  al->ai->number_of_messages = 0;
  
  if(!alloc_only) {
    al->next = ap_pri;
    ap_pri = al;
  }
  else {
    al->next = ap_resvd;
    ap_resvd = al;
  };
  
  return al->ai;
}

static void apinfofree(WORD id) {
	AP_LIST	**al;
	
	al = &ap_pri;
	
	while(*al) {
		if((*al)->ai->id == id) {
			AP_LIST	*altemp = *al;
			
			*al = (*al)->next;
	
			altemp->ai->vid = -1;
			altemp->ai->msgpipe = -1;
			altemp->ai->eventpipe = -1;
			
			if(altemp->ai->rshdr) {
				Mfree(altemp->ai->rshdr);	/*free resource memory*/
			};

			altemp->ai->id = -1;
			Mfree(altemp);

			break;
		};
		al = &(*al)->next;
	};
}

/****************************************************************************
 * search_apid                                                              *
 *  Find AES-id & return AP_LIST entry for that.                            *
 ****************************************************************************/
AP_LIST *         /* Entry or NULL.                                         */
search_apid(      /*                                                        */
WORD apid)        /* Application id of the searched application.            */
/****************************************************************************/
{
	AP_LIST	*al;

	al = ap_pri;
	
	while(al) {
		if(al->ai->id == apid) 
			break;
		al = al->next;
	};
	
	return al;	
}

/*********************************************************************8�`***
 * get_top_appl                                                             *
 *  Get currently topped application.                                       *
 ****************************************************************************/
WORD               /* Id of topped application.                             */
get_top_appl(void) /*                                                       */
/****************************************************************************/
{
	if(ap_pri) {
		return ap_pri->ai->id;
	}
	else {
		return -1;
	};
}

/****************************************************************************
 * update_appl_menu                                                         *
 *  Update the application menu.                                            *
 ****************************************************************************/
WORD                   /* Top application id.                               */
update_appl_menu(void) /*                                                   */
/****************************************************************************/
{
  AP_LIST *mwalk;	WORD    rwalk;
  WORD    topappl;
	
  topappl = get_top_appl();

  mwalk = globals.applmenu;
  rwalk = PMENU_FIRST;
	
  while(mwalk) {
    strcpy(globals.pmenutad[rwalk].ob_spec.free_string,mwalk->ai->name);
    if(mwalk->ai->id == topappl) {			globals.pmenutad[rwalk].ob_state |= CHECKED;
    }
    else {
      globals.pmenutad[rwalk].ob_state &= ~CHECKED;
    };
	
    globals.pmenutad[rwalk].ob_flags &= ~HIDETREE;
    globals.pmenutad[rwalk].ob_state &= ~DISABLED;

    mwalk = mwalk->mn_next;		rwalk++;
  };
	
  if(globals.accmenu) {
    strcpy(globals.pmenutad[rwalk].ob_spec.free_string,
           "----------------------");
    globals.pmenutad[rwalk].ob_flags &= ~HIDETREE;
    globals.pmenutad[rwalk].ob_state &= ~CHECKED;
    globals.pmenutad[rwalk].ob_state |= DISABLED;
    rwalk++;
  };

  mwalk = globals.accmenu;
	
  while(mwalk) {
    strcpy(globals.pmenutad[rwalk].ob_spec.free_string,mwalk->ai->name);
    if(mwalk->ai->id == topappl) {			globals.pmenutad[rwalk].ob_state |= CHECKED;
    }
    else {
      globals.pmenutad[rwalk].ob_state &= ~CHECKED;
    };
	
    globals.pmenutad[rwalk].ob_flags &= ~HIDETREE;
    globals.pmenutad[rwalk].ob_state &= ~DISABLED;

    mwalk = mwalk->mn_next;		rwalk++;
  };

  globals.pmenutad[rwalk].ob_flags |= HIDETREE;
	
  globals.pmenutad[0].ob_height = globals.pmenutad[rwalk].ob_y;
	
  return topappl;}

static void set_widget_colour(WINSTRUCT *win,WORD widget,
			      OBJC_COLORWORD *untop,OBJC_COLORWORD *top) {
  U_OB_SPEC      *ob_spec;
  WORD           object = 0;
  OBJC_COLORWORD *colour;
  
  if(win->tree) {
    object = widgetmap[widget];
    
    if(win->tree[object].ob_flags & INDIRECT) {
      ob_spec = (U_OB_SPEC *)win->tree[object].ob_spec.indirect;
    }
    else {
      ob_spec = (U_OB_SPEC *)&win->tree[object].ob_spec;
    };
    
    switch(win->tree[object].ob_type & 0xff) {
    case G_BOX:
    case G_IBOX:
    case G_BOXCHAR:
      colour = &((OBJC_COLORWORD *)ob_spec)[1];
      break;
      
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
      colour = (OBJC_COLORWORD *)&ob_spec->tedinfo->te_color;
      break;
      
    case G_IMAGE:
      colour = (OBJC_COLORWORD *)&ob_spec->bitblk->bi_color;
      break;
      
    case G_BUTTON:
    case G_PROGDEF:
    case G_STRING:
    case G_TITLE:
      return;
      
    default:
      DB_printf("Unsupported type %d in set_widget_colour.",
		win->tree[object].ob_type);
      return;
    };
    
    if(win->status & WIN_TOPPED) {
      *colour = *top;
    }
    else {
      *colour = *untop;
    };
  };
}

static void packelem(OBJECT *tree,WORD object,WORD left,WORD right,
                     WORD top,WORD bottom) {

	if((left != -1) && (right != -1)) {
		if(left == 0) {
			tree[object].ob_x = D3DSIZE;
		}
		else {
			tree[object].ob_x = tree[left].ob_x + tree[left].ob_width + D3DSIZE * 2;
		};
		
		if(right == 0) {
			tree[object].ob_width = tree[0].ob_width - tree[object].ob_x - D3DSIZE;
		}
		else {
			tree[object].ob_width = tree[right].ob_x - tree[object].ob_x - D3DSIZE * 2;
		}
	}
	else if(left != -1) {
		if(left == 0) {
			tree[object].ob_x = D3DSIZE;
		}
		else {
			tree[object].ob_x = tree[left].ob_x + tree[left].ob_width + D3DSIZE * 2;
		};		
	}
	else if(right != -1) {
		if(right == 0) {
			tree[object].ob_x = tree[0].ob_width - tree[object].ob_width - D3DSIZE;
		}
		else {
			tree[object].ob_x = tree[right].ob_x - tree[object].ob_width - D3DSIZE * 2;
		};
	};
	
	
	if((top != -1) && (bottom != -1)) {
		if(top == 0) {
			tree[object].ob_y = D3DSIZE;
		}
		else {
			tree[object].ob_y = tree[top].ob_y + tree[top].ob_height + D3DSIZE * 2;
		};
		
		if(bottom == 0) {
			tree[object].ob_height = tree[0].ob_height - tree[object].ob_y - D3DSIZE;
		}
		else {
			tree[object].ob_height = tree[bottom].ob_y - tree[object].ob_y - D3DSIZE * 2;
		}
	}
	else if(top != -1) {
		if(top == 0) {
			tree[object].ob_y = D3DSIZE;
		}
		else {
			tree[object].ob_y = tree[top].ob_y + tree[top].ob_height + D3DSIZE * 2;
		};		
	}
	else if(bottom != -1) {
		if(bottom == 0) {
			tree[object].ob_y = tree[0].ob_height - tree[object].ob_height - D3DSIZE;
		}
		else {
			tree[object].ob_y = tree[bottom].ob_y - tree[object].ob_height - D3DSIZE * 2;
		};
	};	
}

static OBJECT *alloc_win_elem(void) {
	WORD    i = 0,tnr = 0;
	OBJECT  *t;
	TEDINFO *ti;
	LONG    size;

	while(elemnumber == -1) {
		switch(globals.windowtad[i].ob_type) {
			case	G_TEXT		:
			case	G_BOXTEXT	:
			case	G_FTEXT		:
			case	G_FBOXTEXT	:
				tnr++;
		}
		
		if(globals.windowtad[i].ob_flags & LASTOB)
		{
			elemnumber = i + 1;
			tednumber = tnr;
		}
		
		i++;
	}
	
	size = sizeof(OBJECT) * elemnumber + sizeof(TEDINFO) * tednumber;
	
	t = (OBJECT *)Mxalloc(size,GLOBALMEM);
	
	if(t != NULL) {
		ti = (TEDINFO *)&t[elemnumber];
		
		memcpy(t,globals.windowtad,sizeof(OBJECT) * elemnumber);
		
		for(i = 0; i < elemnumber; i++) {
			switch(globals.windowtad[i].ob_type) {
				case	G_TEXT		:
				case	G_BOXTEXT	:
				case	G_FTEXT		:
				case	G_FBOXTEXT	:
					t[i].ob_spec.tedinfo = ti;
					memcpy(ti, globals.windowtad[i].ob_spec.tedinfo, sizeof(TEDINFO));
					ti++;
			};
		};
	};
	
	return t;
}

static void set_win_elem(OBJECT *tree,WORD elem) {
	WORD bottomsize = 0;
	WORD rightsize = 0;
	WORD left = 0,right = 0,top = 0,bottom = 0;

	if((HSLIDE | LFARROW | RTARROW) & elem) {
		bottomsize = tree[WLEFT].ob_height + (D3DSIZE << 1);
	};
	
	if((LFARROW | HSLIDE | RTARROW) & elem) {
		bottomsize = tree[WLEFT].ob_height + (D3DSIZE << 1);
	};
	
	if(((bottomsize == 0) && (SIZER & elem))
		|| ((VSLIDE | UPARROW | DNARROW) & elem)) {
		rightsize = tree[WSIZER].ob_width + (D3DSIZE << 1);
	};
	
	if(CLOSER & elem) {
		tree[WCLOSER].ob_flags &= ~HIDETREE;	
		
		packelem(tree,WCLOSER,0,-1,0,-1);
		left = WCLOSER;
	}	
	else {
		tree[WCLOSER].ob_flags |= HIDETREE;
	}
	
	if(FULLER & elem) {
		tree[WFULLER].ob_flags &= ~HIDETREE;	
		
		packelem(tree,WFULLER,-1,0,0,-1);
		right = WFULLER;
	}	
	else {
		tree[WFULLER].ob_flags |= HIDETREE;
	}
		
	if(SMALLER & elem) {
		tree[WSMALLER].ob_flags &= ~HIDETREE;	
		
		packelem(tree,WSMALLER,-1,right,0,-1);
		right = WSMALLER;
	}	
	else {
		tree[WSMALLER].ob_flags |= HIDETREE;
	}
		
	if(MOVER & elem) {
		tree[WMOVER].ob_flags &= ~HIDETREE;
		tree[TFILLOUT].ob_flags |= HIDETREE;
		
		tree[WMOVER].ob_height = tree[WCLOSER].ob_height;
		tree[WMOVER].ob_spec.tedinfo->te_font = IBM;

		packelem(tree,WMOVER,left,right,0,-1);
		top = WMOVER;
	}
	else {
		tree[WMOVER].ob_flags |= HIDETREE;

		if((left != 0) || (right != 0)) {
			tree[TFILLOUT].ob_flags &= ~HIDETREE;

			packelem(tree,TFILLOUT,left,right,0,-1);
			top = TFILLOUT;
		}
		else {
			tree[TFILLOUT].ob_flags |= HIDETREE;
		};
	};
	
	if(INFO & elem) {
		tree[WINFO].ob_flags &= ~HIDETREE;

		packelem(tree,WINFO,0,0,top,-1);
		top = WINFO;		
	}
	else {
		tree[WINFO].ob_flags |= HIDETREE;
	};

	right = 0;
	left = 0;

	if(elem & UPARROW) {
		tree[WUP].ob_flags &= ~HIDETREE;
		
		packelem(tree,WUP,-1,0,top,-1);
		top = WUP;
	}
	else {
		tree[WUP].ob_flags |= HIDETREE;
	};

	if(SIZER & elem) {
		tree[WSIZER].ob_flags &= ~HIDETREE;
		tree[SFILLOUT].ob_flags |= HIDETREE;	
		
		packelem(tree,WSIZER,-1,0,-1,0);
		bottom = right = WSIZER;
	}	
	else {
		tree[WSIZER].ob_flags |= HIDETREE;
		
		if((bottomsize > 0) && (rightsize > 0)) {
			tree[SFILLOUT].ob_flags &= ~HIDETREE;
			
			packelem(tree,SFILLOUT,-1,0,-1,0);
			bottom = right = SFILLOUT;
		}
		else {
			tree[SFILLOUT].ob_flags |= HIDETREE;
		}
	}
	
	if(elem & DNARROW) {
		tree[WDOWN].ob_flags &= ~HIDETREE;

		packelem(tree,WDOWN,-1,0,-1,bottom);
		bottom = WDOWN;		
	}
	else {
		tree[WDOWN].ob_flags |= HIDETREE;
	};
	
	if(elem & VSLIDE) {
		tree[WVSB].ob_flags &= ~HIDETREE;

		packelem(tree,WVSB,-1,0,top,bottom);		
	}
	else
	{
		tree[WVSB].ob_flags |= HIDETREE;
	}
	
	if(!(VSLIDE & elem) && (rightsize > 0))
	{
		tree[RFILLOUT].ob_flags &= ~HIDETREE;

		packelem(tree,RFILLOUT,-1,0,top,bottom);		
	}
	else {
		tree[RFILLOUT].ob_flags |= HIDETREE;
	}
	
	if(LFARROW & elem) {
		tree[WLEFT].ob_flags &= ~HIDETREE;
		
		packelem(tree,WLEFT,0,-1,-1,0);
		left = WLEFT;
	}
	else {
		tree[WLEFT].ob_flags |= HIDETREE;
	}
	
	if(RTARROW & elem) {
		tree[WRIGHT].ob_flags &= ~HIDETREE;
		
		packelem(tree,WRIGHT,-1,right,-1,0);
		right = WRIGHT;
	}
	else {
		tree[WRIGHT].ob_flags |= HIDETREE;
	};
	
	if(elem & HSLIDE) {
		tree[WHSB].ob_flags &= ~HIDETREE;
		
		packelem(tree,WHSB,left,right,-1,0);
	}
	else {
		tree[WHSB].ob_flags |= HIDETREE;
	}
	
	if(!(HSLIDE & elem) && (bottomsize > 0)) {
		tree[BFILLOUT].ob_flags &= ~HIDETREE;
		
		packelem(tree,BFILLOUT,left,right,-1,0);
	}
	else {
		tree[BFILLOUT].ob_flags |= HIDETREE;
	};
	
	if(IMOVER & elem) {
		tree[WMOVER].ob_flags &= ~HIDETREE;
		tree[WMOVER].ob_height = globals.csheight + 2;
		tree[WMOVER].ob_spec.tedinfo->te_font = SMALL;
		packelem(tree,WMOVER,0,0,0,-1);

		tree[WAPP].ob_flags |= HIDETREE;
	}
	else {
		tree[WAPP].ob_flags &= ~HIDETREE;
	};
}

/* delwinelem deletes an object created with crwinelem*/

static void	delwinelem(OBJECT *o) {
	if(o) {
		Mfree(o);
	};
}

/*calcworksize calculates the worksize or the total size of
a window. If dir == WC_WORK the worksize will be calculated and 
otherwise the total size will be calculated.*/

static void	calcworksize(WORD elem,RECT *orig,RECT *new,WORD dir) {
	WORD	bottomsize = 1;
	WORD	headsize = 1;
	WORD	leftsize = 1;
	WORD	rightsize = 1;
	WORD	topsize;
	
	if((HSLIDE | LFARROW | RTARROW) & elem) {
		bottomsize = globals.windowtad[WLEFT].ob_height + (D3DSIZE << 1);
	};
	
	if((CLOSER | MOVER | FULLER | NAME) & elem) {
		topsize = globals.windowtad[WMOVER].ob_height + (D3DSIZE << 1);
	}
	else if(IMOVER & elem) {
		topsize = globals.csheight + 2 + D3DSIZE * 2;
	}
	else {
		topsize = 0;
	};
	
	if(INFO & elem) {
		headsize = topsize + globals.windowtad[WINFO].ob_height + 2 * D3DSIZE;
	}
	else {
		if(topsize)
			headsize = topsize;
		else
			headsize = 1;
	};
	
	if((LFARROW | HSLIDE | RTARROW) & elem) {
		bottomsize = globals.windowtad[WLEFT].ob_height + (D3DSIZE << 1);
	};
	
	if(((bottomsize < globals.windowtad[WLEFT].ob_height) && (SIZER & elem))
		|| ((VSLIDE | UPARROW | DNARROW) & elem))
	{
		rightsize = globals.windowtad[WSIZER].ob_width + (D3DSIZE << 1);
	};

	if(dir == WC_WORK) {
		new->x = orig->x + leftsize;
		new->y = orig->y + headsize;
		new->width = orig->width - leftsize - rightsize;
		new->height = orig->height - headsize - bottomsize;
	}
	else {
		new->x = orig->x - leftsize;
		new->y = orig->y - headsize;
		new->width = orig->width + leftsize + rightsize;
		new->height = orig->height + headsize + bottomsize;
	};
}


/****************************************************************************
 * srv_click_owner                                                          *
 *  Find out which application that "owns" mouse clicks.                    *
 ****************************************************************************/
WORD                    /* Application to receive clicks.                   */
srv_click_owner(void)   /*                                                  */
/****************************************************************************/
{
  if(globals.mouse_owner != -1) {
    return globals.mouse_owner;
  }
  else {
    WORD        status;
    WORD        owner;
    C_WIND_FIND c_wind_find;
    C_WIND_GET  c_wind_get;
    R_WIND_GET  r_wind_get;

    c_wind_find.x = globals.mouse_x;
    c_wind_find.y = globals.mouse_y;
    srv_wind_find (&c_wind_find);
    
    c_wind_get.handle = c_wind_find.retval;
    c_wind_get.mode = WF_OWNER;
    srv_wind_get (&c_wind_get, &r_wind_get);
    owner = r_wind_get.parm1;
    status = r_wind_get.parm2;
    
    if(status & WIN_DESKTOP) {
      AP_INFO *ai;
      
      ai = internal_appl_info(DESK_OWNER);
      
      if(ai) {
	return ai->id;
      };
    }
    else {
      return owner;
    };
  };
  
  return 0;
}

/****************************************************************************
 * srv_get_appl_info                                                        *
 ****************************************************************************/
WORD                       /* 0 if ok or -1.                                */
srv_get_appl_info(         /*                                               */
WORD apid,                 /* Application id.                               */
SRV_APPL_INFO *appl_info)  /* Returned info.                                */
/****************************************************************************/
{
  AP_INFO *ai;
  WORD    retval = 0;

  ai = internal_appl_info(apid);			

  if(ai) {
    appl_info->eventpipe = ai->eventpipe;
    appl_info->msgpipe = ai->msgpipe;
    appl_info->vid = ai->vid;
  }
  else
  {
    DB_printf(
              "%s: Line %d: Couldn't find description of application %d\r\n",
              __FILE__,__LINE__,apid);

    retval = -1;
  };

  return retval;
}


/*
** Description
** Get physical vdi handle of oaesis
**
** 1998-11-15 CG
*/
static
void
srv_get_vdi_id (C_GET_VDI_ID * par,
            R_GET_VDI_ID * ret) {
  ret->vid = svid;
}


/****************************************************************************
 * srv_get_wm_info                                                          *
 *  Get window manager info on window.                                      *
 ****************************************************************************/
int                  /*                                                     */
srv_get_wm_info(     /*                                                     */
C_GET_WM_INFO * msg) /*                                                     */
/****************************************************************************/
{
  WINSTRUCT *win = find_wind_description (msg->id);
  
  if(win) {
    msg->retval = win->tree;
  } else {
    msg->retval = NULL;
  }

  return 1;
}

/****************************************************************************
 * set_menu                                                                 *
 *  Set the resource tree of the menu of an application                     *
 ****************************************************************************/
static WORD       /* 0 if ok or -1.                                         */
set_menu(         /*                                                        */
WORD apid,        /* Id of application.                                     */
OBJECT *tree)     /* Resource tree.                                         */
/****************************************************************************/
{
	if(apps[apid].id != -1) {
		apps[apid].menu = tree;
		return 0;
	};
	
	return -1;
}

/****************************************************************************
 * srv_get_top_menu                                                         *
 *  Get the resource tree of the menu of an application                     *
 ****************************************************************************/
int                                     /*                                  */
srv_get_top_menu(C_GET_TOP_MENU * msg)  /*                                  */
/****************************************************************************/
{
  AP_INFO *ai = internal_appl_info(TOP_MENU_OWNER);
  
  if(ai) {
    msg->retval = ai->menu;
  } else {
    msg->retval = NULL;
  }
  
  return 1;
}

/****************************************************************************
 * unregister_menu                                                          *
 *  Remove menu entry of application.                                       *
 ****************************************************************************/
static void       /*                                                        */
unregister_menu(  /*                                                        */
WORD apid)        /* Application id.                                        */
/****************************************************************************/
{
  AP_LIST **mwalk;	
  mwalk = &globals.applmenu;
  
  while(*mwalk) {
    if((*mwalk)->ai->id == apid) {
      *mwalk = (*mwalk)->mn_next;
      break;
    };
    mwalk = &(*mwalk)->mn_next;
  };
  mwalk = &globals.accmenu;
  
  while(*mwalk) {
    if((*mwalk)->ai->id == apid) {
      *mwalk = (*mwalk)->mn_next;
      break;
    };
    mwalk = &(*mwalk)->mn_next;
  };
}

static void redraw_menu_bar(void) {
  OBJECT *       menu;
  C_GET_TOP_MENU c_get_top_menu;
  
  srv_get_top_menu(&c_get_top_menu);
  menu = c_get_top_menu.retval;

  if(menu) {
    RECT r;
    C_WIND_GET c_wind_get;
    R_WIND_GET r_wind_get;
    
    c_wind_get.handle = mglob.winbar;
    c_wind_get.mode = WF_FIRSTXYWH;
    srv_wind_get (&c_wind_get, &r_wind_get);
    r = *((RECT *)&r_wind_get.parm1);

    while((r.width > 0) && (r.height > 0)) {
      C_WIND_GET c_wind_get;
      R_WIND_GET r_wind_get;

      /*
      Objc_do_draw(menu,0,9,&r);
      */

      c_wind_get.handle = mglob.winbar;
      c_wind_get.mode = WF_NEXTXYWH;
      srv_wind_get (&c_wind_get, &r_wind_get);
      r = *((RECT *)&r_wind_get.parm1);
    };
  };
}

/*
 *	Menu install code.. 
 *
 *	Creates and updates the menu list entries for the specified menu. 
 *	Menu capid is unique. New capid => new menu. Old capid => update menu.
 *
 */
static WORD       /* .w return code.*/
menu_bar_install( /* menu_bar(MENU_INSTALL) */
OBJECT *tree,     /* object tree. Pointer to the menu's OBJECT	*/
WORD   capid)     /* Current application id. Menu owner	*/
{
	WORD         i;

	set_menu(capid,tree);

	/* Modify height of bar and titles*/
	
	tree[tree[0].ob_head].ob_height = globals.clheight + 3;
	tree[tree[tree[0].ob_head].ob_head].ob_height = globals.clheight + 3;

	i = tree[tree[tree[0].ob_head].ob_head].ob_head;

	while(i != -1) {		
		tree[i].ob_height = globals.clheight + 3;
		
		if(i == tree[tree[i].ob_next].ob_tail) {
			break;
		};
		
		i = tree[i].ob_next;
	};

	/* Mark all drop down menus with HIDETREE and set y position */
	tree[tree[tree[0].ob_head].ob_next].ob_y = globals.clheight + 3;

	i = tree[tree[tree[0].ob_head].ob_next].ob_head;

	while(i != -1) {		
		tree[i].ob_flags |= HIDETREE;
		
		if(i == tree[tree[i].ob_next].ob_tail) {
			break;
		};
		
		i = tree[i].ob_next;
	};

	redraw_menu_bar();

	return 1;
}

/*
 * 	This one is simple.. Return the apid of the currently topped menu owner.
 * 
 */
static WORD         /* .w return code	*/
menu_bar_inquire(   /*	*/
void)               /*	*/
{
	return mglob.menu_cur_apid;
}

/****************************************************************************
 * menu_bar_remove                                                          *
 *  Remove menu.                                                            *
 ****************************************************************************/
static WORD       /*                                                        */
menu_bar_remove(  /*                                                        */
WORD apid)        /* Application whose menu is to be removed.               */
/****************************************************************************/
{
	set_menu(apid,NULL);

	redraw_menu_bar();

	return 1;
}

/****************************************************************************
 *  srv_menu_bar                                                            *
 *   0x001e menu_bar() library code.                                        *
 ****************************************************************************/
static WORD       /*                                                        */
srv_menu_bar(     /*                                                        */
C_MENU_BAR * msg) /*                                                        */
/****************************************************************************/
{
  switch(msg->mode) {
  case MENU_INSTALL: 
    return menu_bar_install(msg->tree, msg->apid);
    
  case MENU_REMOVE:
    return menu_bar_remove(msg->apid);
    
  case MENU_INQUIRE:
    return menu_bar_inquire();
    
  default:
    return 0;
  };
}

/****************************************************************************
 * srv_menu_register                                                        *
 *  Implementation of menu_register().                                      *
 ****************************************************************************/
WORD               /* Menu identification, or -1.                           */
srv_menu_register( /*                                                       */
WORD apid,         /* Application id, or -1.                                */
BYTE *title)       /* Title to register application under.                  */
/****************************************************************************/
{
  AP_LIST **mwalk;
  AP_LIST *ap;
  WORD    n_menu = apid;	
  
  ap = search_apid(apid);
  
  if(!ap) {
    return -1;
  };
  
  /* if the menu have been registered then unlink it again */
  if(ap->ai->type & APP_ACCESSORY) {
    mwalk = &globals.accmenu;
  }
  else {
    mwalk = &globals.applmenu;
  };
  
  while(*mwalk) {
    if(*mwalk == ap) {
      *mwalk = (*mwalk)->mn_next;
      break;
    };
    mwalk = &(*mwalk)->mn_next;
  }
  /* now find a new position to enter the menu */	
  if(ap->ai->type & APP_ACCESSORY) {
    mwalk = &globals.accmenu;
  }
  else {
    mwalk = &globals.applmenu;
  }
  
  while(*mwalk) {	
    if(strcasecmp((*mwalk)->ai->name, title) > 0) {
      break;
    };
    mwalk = &(*mwalk)->mn_next;
  };
  /* insert the menu */	
  ap->mn_next = *mwalk;
  *mwalk = ap;
  strncpy(ap->ai->name,title,20);	
  update_appl_menu();
  
  return n_menu;
}

/****************************************************************************
 * srv_appl_control                                                         *
 ****************************************************************************/
WORD                  /* 0 if error or >0.                                  */
srv_appl_control(     /*                                                    */
C_APPL_CONTROL * msg) /*                                                    */
/****************************************************************************/
{
  switch(msg->mode) {
  case APC_TOPNEXT:
    {
      AP_LIST *al = ap_pri;
      WORD    nexttop = -1;
      
      while(al) {
	nexttop = al->ai->id;
	
	al = al->next;
      };
      
      if(nexttop != -1) {
	top_appl(nexttop);
      };
      
      return 1;
    };
    
  case APC_KILL:
    {
      AP_INFO *ai = internal_appl_info(msg->apid);
      
      if((ai->newmsg & 0x1) && (ai->killtries < 20)) {
	COMMSG       m;
	C_APPL_WRITE c_appl_write;
        R_APPL_WRITE r_appl_write;
	
	ai->killtries++;

	DB_printf("Sending AP_TERM to %d", msg->apid);
	
	m.type = AP_TERM;
	m.sid = 0;
	m.length = 0;
	m.msg2 = AP_TERM;
	
	c_appl_write.addressee = msg->apid;
	c_appl_write.length = sizeof (COMMSG);
        c_appl_write.is_reference = TRUE;
	c_appl_write.msg.ref = &m;
	srv_appl_write (&c_appl_write, &r_appl_write);
      }
      else {
        C_APPL_EXIT par;
        R_APPL_EXIT ret;

	DB_printf("Killing apid %d", msg->apid);
	
	(void)Pkill(ai->pid,SIGKILL);

	par.common.apid = msg->apid;
	srv_appl_exit (&par, &ret);
      };
      
      return 1;
    };
    
  case APC_TOP:
    top_appl (msg->apid);
    return 1;
    
  default:
    DB_printf("srv_appl_control doesn't support mode %d", msg->mode);
    return 0;
  }
}

/****************************************************************************
 * srv_appl_exit                                                            *
 *  Implementation of appl_exit().                                          *
 ****************************************************************************/
void
srv_appl_exit(     /*                                                       */
C_APPL_EXIT * par,
R_APPL_EXIT * ret)
/****************************************************************************/
{
  C_WIND_SET cws = {0,WF_NEWDESK,0,0,0,0};
  C_MENU_BAR c_menu_bar;

  /*clean up*/

  c_menu_bar.apid = par->common.apid;
  c_menu_bar.tree = NULL;
  c_menu_bar.mode = MENU_REMOVE;
  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Removing menu bar\n");
  srv_menu_bar(&c_menu_bar);
  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Unregistering menu bar\n");
  unregister_menu(par->common.apid);
  /*  srv_wind_set(par->common.apid,&cws);*/
  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Removing windows\n");
  srv_wind_new(par->common.apid);
  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Removing apinf\n");
  apinfofree(par->common.apid);
  
  /*
  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Updating application menu\n");
  update_appl_menu();
  */

  fprintf (stderr, "oaesis: srv.c: srv_appl_exit: Returning\n");
  ret->common.retval = 1;
}

/****************************************************************************
 * srv_appl_find                                                            *
 *  Implementation of appl_find().                                          *
 ****************************************************************************/
WORD              /* Application id, or -1.                                 */
srv_appl_find(    /*                                                        */
BYTE *fname)      /* File name of application to seek.                      */
/****************************************************************************/
{
  AP_LIST *al;
  LONG    w;
  _DTA    *olddta,newdta;
  BYTE    pname[ 30];
  
  w = (LONG)fname & 0xffff0000l;
  if(w == 0xffff0000l) {
    /* convert MiNT to AES pid */
    al = search_mpid((WORD)((LONG)fname));
    
    if(al) {
      return al->ai->id;
    };
    
    return -1;
  }
  
  if ( w == 0xfffe0000l)
    {
      /* convert from AES to MINT pid */
      al = search_apid((WORD)((LONG)fname));
      if(al)
	return al->ai->pid;
      return -1;
    }
  
  /* Now find the pid of process with the passed name */
  olddta = Fgetdta();
  
  Fsetdta( &newdta);
  w = -1;
  sprintf(pname, "u:\\proc\\%s.*", fname);
  
  if(Fsfirst(pname, 0) == 0) {
    w = atoi(&newdta.dta_name[9]);
  };
  
  Fsetdta(olddta);
  
  /* map the MINT-pid to aes */
  if(w != -1) {
    al = search_mpid((WORD)w);
    
    if(al) {
      return al->ai->id;
    };
  };
  
  return -1;
}

/*
** Description
** appl_init help call
**
** 1998-09-26 CG
** 1998-11-15 CG
*/
static void
srv_appl_init(C_APPL_INIT * par,
              R_APPL_INIT * ret) {
  AP_INFO	*ai;
  AP_LIST	*al;
  
  fprintf (stderr, "oaesis: srv.c: srv_appl_init: Beginning\n");

  al = search_mpid(par->common.pid);
  
  if(!al) {     
    /* Has an info structure already been reserved? */
    
    AP_LIST **awalk = &ap_resvd;
    
    while(*awalk) {
      if((*awalk)->ai->pid == par->common.pid) {
	break;
      };
      
      awalk = &(*awalk)->next;
    };
    
    if(*awalk) {
      al = *awalk;
      *awalk = al->next;
      
      al->next = ap_pri;
      ap_pri = al;
      
      ai = al->ai;
    }
    else {
      ai = srv_info_alloc(par->common.pid,APP_APPLICATION,0);
    };
  }
  else {
    ai = al->ai;
  };
  
  if(ai) {
    /*    if(par->msghandle) {
      ai->msgpipe = par->msghandle;
    };
    
    if(par->eventhandle) {
      ai->eventpipe = par->eventhandle;
    };
    
    strcpy(ai->msgname,par->msgname);
    strcpy(ai->eventname,par->eventname);
    
    ai->vid = par->vid;
    */
    
    ret->apid = ai->id;

    fprintf (stderr,
             "oaesis: srv.c: srv_appl_init: apid=%d\n",
             (int)ret->apid);
  }
  else {
    /*
    par->global->apid = -1;
    */
    ret->apid = -1;
  };

  ret->physical_vdi_id = svid;
  /*
  if(ret->apid >= 0) {
    BYTE fname[128],cmdlin[128],menuentry[21],*tmp;
    
    Misc_get_loadinfo(pid,128,cmdlin,fname);
    
    strcpy(menuentry,"  ");
    
    tmp = strrchr(fname,'\\');
    
    if(tmp) {
      tmp++;
    }
    else {
      tmp = fname;
    };
    
    strncat(menuentry,tmp,20);
    
    if(ai->type & APP_APPLICATION) {
      srv_menu_register(ai->id, menuentry);
    };
  };
  */
}


/****************************************************************************
 * srv_appl_search                                                          *
 *  Implementation of appl_search().                                        *
 ****************************************************************************/
WORD                /* 0 if no more applications exist, or 1.               */
srv_appl_search(    /*                                                      */
WORD          apid, /* pid of caller..                                      */
C_APPL_SEARCH *msg)
/****************************************************************************/
{
  AP_LIST *this,*p;
  
  this = search_apid(apid);
  
  if(!this) {
    return 0;
  };
  
  switch(msg->mode) {
  case APP_FIRST:
    this->ai->ap_search_next = ap_pri;
    /* there will always have atleast ourself to return */
    
  case APP_NEXT:
    p = this->ai->ap_search_next;
    
    if(!p) {
      return 0;
    };
    
    strncpy(msg->name, p->ai->name, 18); /* the 'pretty name' */
    
    msg->type =  p->ai->type;           /* sys/app/acc */
    
    msg->ap_id = p->ai->id;
    
    /* get the next... */	
    this->ai->ap_search_next = p->next;
    
    return (p->next) ? 1: 0;
    
  case 2:        /* search system shell (??) */
    DB_printf("srv_appl_search(2,...) not implemented yet.\r\n");
    break;
    
  default:
    DB_printf("%s: Line %d: srv_appl_search\r\n"
	      "Unknown mode %d",__FILE__,__LINE__,msg->mode);
  }
  
  return 0;
}

/*
** Description
**  Implementation of appl_write().                                         *
**
** 1998-10-04 CG
*/
void
srv_appl_write (C_APPL_WRITE * msg,
                R_APPL_WRITE * ret)
{
  if (msg->length != sizeof (COMMSG)) {
    DB_printf ("srv.c: srv_appl_write: Sorry, only standard messages supported yet");
    ret->common.retval = 1;
  } else {
    AP_INFO	*ai;
   
    ai = internal_appl_info (msg->addressee);
    
    if (ai != NULL) {
      if (ai->number_of_messages < MAX_MSG) {
        if (msg->is_reference) {
          memcpy (&ai->messages[(ai->next_message + ai->number_of_messages)
                               % MAX_MSG],
                  msg->msg.ref,
                  msg->length);
        } else {
          ai->messages[(ai->next_message + ai->number_of_messages) % MAX_MSG] =
            msg->msg.event;
        }
        ai->number_of_messages++;

        ret->common.retval = 0;
      } else {
        ret->common.retval = 1;
      }
    } else {
      ret->common.retval = 1;
    }
  }    
}


/*
** Description
** Server part of evnt_multi
**
** 1998-10-04 CG
*/
void
srv_evnt_multi (
C_EVNT_MULTI * par,
R_EVNT_MULTI * ret)
{
  /* Reset event flag */
  ret->eventout.events = 0;

  if (par->eventin.events & MU_MESAG) {
    /* Check for new messages */
    AP_INFO * ai = internal_appl_info (par->common.apid);

    if (ai != NULL) {
      if (ai->number_of_messages > 0) {
        DB_printf ("srv.c: srv_evnt_multi: New message");
        ret->msg = ai->messages[ai->next_message];
        ai->next_message = (ai->next_message + 1) % MAX_MSG;
        ai->number_of_messages--;
        ret->eventout.events |= MU_MESAG;
      } else {
        /*
        DB_printf ("srv.c: srv_evnt_multi: No new messages");
        */
      }
    }
  }

  /* Cheat for now and always return MU_TIMER */
  ret->eventout.events |= MU_TIMER;
}



/****************************************************************************
 * update_deskbg                                                            *
 *  Update all of the desk background.                                      *
 ****************************************************************************/
void                     /*                                                 */
update_deskbg(void)      /*                                                 */
/****************************************************************************/
{
	WINSTRUCT *win;

	win = find_wind_description(0);
	draw_wind_elements(win,&globals.screen,0);
}

/****************************************************************************
 * get_deskbg                                                               *
 *  Get the resource tree of the top desktop.                               *
 ****************************************************************************/
static OBJECT *   /* Resource tree, or NULL.                                */
get_deskbg(void)  /*                                                        */
/****************************************************************************/
{
	OBJECT  *retval = NULL;
	AP_INFO *ai;
		
	ai = internal_appl_info(DESK_OWNER);
		
	if(ai) {
		retval = ai->deskbg;
	};
		
	return retval;
}

/****************************************************************************
 * set_deskbg                                                               *
 *  Set the resource tree of the desktop of an application                  *
 ****************************************************************************/
static WORD       /* 0 if ok or -1.                                         */
set_deskbg(       /*                                                        */
WORD apid,        /* Id of application.                                     */
OBJECT *tree)     /* Resource tree.                                         */
/****************************************************************************/
{
	OBJECT *deskbg,*olddeskbg = apps[apid].deskbg;
	
	if(apps[apid].id != -1) {
		apps[apid].deskbg = tree;

		deskbg = get_deskbg();
	
		if(((deskbg == tree) && (deskbg != olddeskbg)) || !tree) {
			update_deskbg();
		};
	
		return 0;
	};

	return -1;
}

/****************************************************************************
 * top_appl                                                                 *
 *  Top application.                                                        *
 ****************************************************************************/
static WORD       /* Previously topped application.                         */
top_appl(         /*                                                        */
WORD apid)        /* Id of application.                                     */
/****************************************************************************/
{
  AP_LIST **al;
  OBJECT  *deskbg = NULL;
  WORD    deskbgcount = 0;
  WORD    lasttop;
  
  lasttop = ap_pri->ai->id;
  al = &ap_pri;
  
  while(*al) {
    if((*al)->ai->id == apid) {
      AP_LIST *tmp = *al;
      
      *al = (*al)->next;
      
      tmp->next = ap_pri;
      ap_pri = tmp;
      
      deskbg = tmp->ai->deskbg;
      
      break;
    };
    
    if((*al)->ai->deskbg) {
      deskbgcount++;
    }
    
    al = &(*al)->next;
  };
  
  if(deskbg && deskbgcount) {
    update_deskbg();
  };
  
  update_appl_menu();
  redraw_menu_bar();
  
  return lasttop;
}

static void del_env(const BYTE *strng) {
  BYTE **var;
  BYTE *name;
  LONG len = 0;
  
  if (!environ)
    return; /* find the length of "tag" in "tag=value" */
  
  for (name = (char *)strng; *name && (*name != '='); name++)
    len++; /* find the tag in the environment */

  for (var = environ; (name = *var) != NULL; var++) {
    if(!strncmp(name, strng, len) && name[len] == '=') {
      break;
    };
  } /* if it's found, move all the other environment variables down by 1 to
       delete it */
  
  if(name) {
    Mfree(name);
    
    while (name) {
      name = var[1];
      *var++ = name;
    }
  }
}

static WORD srv_putenv(const BYTE *strng) {
  WORD i = 0;
  BYTE **e;
  del_env(strng);
  if(!environ) {
    e = (BYTE **)Mxalloc(2*sizeof(BYTE *),GLOBALMEM);
  }
  else {
    while(environ[i]) 
      i++;
    e = (BYTE **)Mxalloc((i+2)*sizeof(BYTE *),GLOBALMEM);
    if (!e) {
      return -1;
    }
    
    bcopy(environ, e,(i + 1) * sizeof(BYTE *));
    Mfree(environ);
    environ = e;
  }
  
  if(!e) {
    return -1;
  };
  
  environ = e;
  environ[i] = (BYTE *)Mxalloc(strlen(strng) + 1,GLOBALMEM);
  strcpy(environ[i],strng);
  environ[i+1] = 0;
  return 0;
}

static BYTE *srv_getenv(const char *tag) {
  BYTE **var;
  BYTE *name;
  LONG len = strlen(tag);
	
  if(strrchr(tag,'=')) {
    len--;
  };
  if (!environ) return 0;
  
  for (var = environ; (name = *var) != 0; var++) {
    if (!strncmp(name, tag, len) && name[len] == '=')
      return name+len+1;
  }
  return 0;
}

/****************************************************************************
 * srv_shel_envrn                                                           *
 *  Implementation of shel_envrn().                                         *
 ****************************************************************************/
WORD                /*                                                      */
srv_shel_envrn(     /*                                                      */
C_SHEL_ENVRN * msg) /*                                                      */
/****************************************************************************/
{
  *msg->value = srv_getenv (msg->name);

  return 1;
}

/****************************************************************************
 * srv_shel_write                                                           *
 *  Implementation of shel_write().                                         *
 ****************************************************************************/
WORD                /*                                                      */
srv_shel_write(     /*                                                      */
WORD apid,          /* Application id.                                      */
C_SHEL_WRITE * msg) /*                                                      */
/****************************************************************************/
{
  AP_INFO  *ai;
  WORD     r = 0;
  BYTE     *tmp,*ddir = NULL,*envir = NULL;
  BYTE     oldpath[128];
  BYTE     exepath[128];			
  SHELW    *shelw;
  BASEPAGE *b;
  
  NOT_USED(msg->wiscr);
  
  shelw = (SHELW *)msg->cmd;
  ddir = NULL;
  envir = NULL;
  
  if(msg->mode & 0xff00) /* should we use extended info? */
    {
      msg->cmd = shelw->newcmd;
      
      /*	
		if(mode & SW_PSETLIMIT) {
		v_Psetlimit = shelw->psetlimit;
		};
		
		if(mode & SW_PRENICE) {
		v_Prenice = shelw->prenice;
		};
		*/
      
      if(msg->mode & SW_DEFDIR) {
	ddir = shelw->defdir;
      };
      
      if(msg->mode & SW_ENVIRON) {
	envir = shelw->env;
      };
    };
  
  msg->mode &= 0xff;
  
  if(msg->mode == SWM_LAUNCH)	/* - run application */ 
    {
      tmp = strrchr(msg->cmd, '.');
      if(!tmp) {
	tmp = "";
      };
      
      /* use enviroment GEMEXT, TOSEXT, and ACCEXT. */
      
      if((strcasecmp(tmp,".app") == 0) || (strcasecmp(tmp,".prg") == 0)) {
	msg->mode = SWM_LAUNCHNOW;
	msg->wisgr = 1;
      }
      else if (strcasecmp(tmp,".acc") == 0) {
	msg->mode = SWM_LAUNCHACC;
	msg->wisgr = 3;
      }
      else {
	msg->mode = SWM_LAUNCHNOW;
	msg->wisgr = 0;
      };
    };
  
  switch (msg->mode) {
  case SWM_LAUNCH: 	/* - run application */
    /* we just did take care of this case */
    break;
    
  case SWM_LAUNCHNOW: /* - run another application in GEM or TOS mode */
    if(msg->wisgr == GEMAPP) {
      Dgetpath(oldpath,0);
      
      strcpy(exepath, msg->cmd);
      tmp = exepath;
      
      if(ddir) {
	Misc_setpath(ddir);
      }
      else {
	tmp = strrchr(exepath,'\\');
	if(tmp) {
	  *tmp = 0;
	  Misc_setpath(exepath);
	  tmp++;
	}
	else {
	  tmp = exepath;
	};
      };
      
      r = (WORD)Pexec(100, tmp, msg->tail, envir);
      
      if(r < 0) {
	r = 0;
      }
      else if((ai = srv_info_alloc(r,APP_APPLICATION,1))) {
	r = ai->id;
      }
      else {
	r = 0;
      };
      
      Misc_setpath(oldpath);
    }
    else if(msg->wisgr == TOSAPP) {
      WORD fd;
      BYTE new_cmd[300];
      WORD t;
      
      Dgetpath(oldpath,0);
      
      strcpy(exepath, msg->cmd);
      tmp = exepath;
      
      if(!ddir) {
	ddir = oldpath;
      };
      
      sprintf(new_cmd, "%s %s %s", ddir, msg->cmd, msg->tail + 1);
      
      fd = (int)Fopen("U:\\PIPE\\TOSRUN", 2);
      t = (short)strlen(new_cmd) + 1;
      
      Fwrite(fd, t, new_cmd);
      
      Fclose(fd);
      
      r = 1;
    };
    break;
    
  case SWM_LAUNCHACC: /* - run an accessory */
    Dgetpath(oldpath,0);
    
    strcpy(exepath, msg->cmd);
    tmp = exepath;
    if(ddir) {
      Misc_setpath(ddir);
    }
    else {
      tmp = strrchr(exepath,'\\');
      
      if(tmp) {
	BYTE c = tmp[1];
	
	tmp[1] = 0;
	Misc_setpath(exepath);
	tmp[1] = c;
	tmp++;
      }
      else {
	tmp = exepath;
      };
    };
    
    b = (BASEPAGE *)Pexec(3, tmp, msg->tail, envir);
    
    if(((LONG)b) > 0) {
      Mshrink(b,256 + b->p_tlen + b->p_dlen + b->p_blen);
      
      b->p_dbase = b->p_tbase;
      b->p_tbase = (BYTE *)accstart;
      
      r = (WORD)Pexec(104,tmp,b,NULL);
      
      if(r < 0) {
	r = 0;
      }
      else if((ai = srv_info_alloc(r,APP_ACCESSORY,1))) {
	r = ai->id;
      }
      else {
	r = 0;
      };
    }
    else {
      DB_printf("Pexec failed: code %ld",(LONG)b);
      r = 0;
    };
    
    Misc_setpath(oldpath);
    break;
    
  case SWM_SHUTDOWN: /* - set shutdown mode */
  case SWM_REZCHANGE: /* - resolution change */
  case SWM_BROADCAST: /* - send message to all processes */
    break;
    
  case SWM_ENVIRON: /* - AES environment */
    switch(msg->wisgr) {
    case ENVIRON_CHANGE:
      srv_putenv (msg->cmd);
      r = 1;
      break;
      
    default:
      DB_printf("shel_write(SWM_ENVIRON,%d,...) not implemented.", msg->wisgr);
      r = 0;
    };
    break;
    
  case SWM_NEWMSG: /* - I know about: bit 0: AP_TERM */
    if(apps[apid].id != -1) {
      apps[apid].newmsg = msg->wisgr;
      r = 1;
    }
    else {
      r = 0;
    };
    
    break;
    
  case SWM_AESMSG: /* - send message to the AES */
  default:
    ;
  };
  
  return r;
}

/****************************************************************************
 * draw_wind_elements                                                       *
 *  Draw the elements of the window win that intersects with the rectangle  *
 *  r.                                                                      *
 ****************************************************************************/
static void          /*                                                     */
draw_wind_elements(  /*                                                     */
WINSTRUCT *win,      /* Window description.                                 */
RECT *r,             /* Clipping rectangle.                                 */
WORD start)          /* Start object.                                       */
/****************************************************************************/
{
  RLIST	*rl = win->rlist;

  if(win->id == 0) {
    OBJECT *deskbg;
	
    deskbg = get_deskbg();
		
    if(deskbg) {
      while(rl) {		
        RECT	r2;
			
        if(Misc_intersect(&rl->r,r,&r2)) {
          /*
            Objc_do_draw(deskbg,start,9,&r2);
            */
        };
					
        rl = rl->next;
      };
    };
  }
  else if(win->tree) {	
    while(rl) {		
      RECT	r2;
		
      if(Misc_intersect(&rl->r,r,&r2)) {
        /*
          Objc_do_draw(win->tree,start,3,&r2);
          */
      };
			
      rl = rl->next;
    };
  };
}

/****************************************************************************
 * draw_wind_elemfast                                                       *
 *  Draw the elements of the window win that intersects with the rectangle  *
 *  r.                                                                      *
 ****************************************************************************/
void                 /*                                                     */
draw_wind_elemfast(  /*                                                     */
WINSTRUCT *win,      /* Window description.                                 */
RECT *r,             /* Clipping rectangle.                                 */
WORD start)          /* Start object.                                       */
/****************************************************************************/
{
  if(win->id == 0) {
    OBJECT *deskbg;
	
    deskbg = get_deskbg();
		
    if(deskbg) {
      /*
      Objc_do_draw(deskbg,start,9,r);
      */
    };
  }
  else if(win->tree) {	
    /*
    Objc_do_draw(win->tree,start,3,r);
    */
  };
}

static WORD	changeslider(WINSTRUCT *win,WORD redraw,WORD which,
				WORD position,WORD size) {	
	WORD redraw2 = 0;

	if(which & VSLIDE) {
		WORD newheight,newy;
		
		if(position != -1) {
			if(position > 1000) {
				win->vslidepos = 1000;
			}
			else if(position < 1) {
				win->vslidepos = 1;
			}
			else {
				win->vslidepos = position;
			};
		};
		
		if(size != -1) {
			if(size > 1000) {
				win->vslidesize = 1000;
			}
			else if(size < 1) {
				win->vslidesize = 1;
			}
			else {
				win->vslidesize = size;
			};
		};

		newy = (WORD)(((LONG)win->vslidepos * (LONG)(win->tree[WVSB].ob_height - win->tree[WVSLIDER].ob_height)) / 1000L);
		newheight = (WORD)(((LONG)win->vslidesize * (LONG)win->tree[WVSB].ob_height) / 1000L);

		if((win->tree[WVSLIDER].ob_y != newy) ||
			(win->tree[WVSLIDER].ob_height != newheight)) {
			win->tree[WVSLIDER].ob_y = newy;
			win->tree[WVSLIDER].ob_height = newheight;
			
			redraw2 = 1;
		};
	}
	
	if(which & HSLIDE) {
		WORD newx,newwidth;
		
		if(position != -1) {
			if(position > 1000) {
				win->hslidepos = 1000;
			}
			else if(position < 1) {
				win->hslidepos = 1;
			}
			else {
				win->hslidepos = position;
			};
		};
		
		if(size != -1) {
			if(size > 1000) {
				win->hslidesize = 1000;
			}
			else if(size < 1) {
				win->hslidesize = 1;
			}
			else {
				win->hslidesize = size;
			};
		};
		
		newx = (WORD)(((LONG)win->hslidepos * (LONG)(win->tree[WHSB].ob_width - win->tree[WHSLIDER].ob_width)) / 1000L);
		newwidth = (WORD)(((LONG)win->hslidesize * (LONG)win->tree[WHSB].ob_width) / 1000L);

		if((win->tree[WHSLIDER].ob_x != newx) ||
			(win->tree[WHSLIDER].ob_width != newwidth)) {
			win->tree[WHSLIDER].ob_x = newx;
			win->tree[WHSLIDER].ob_width = newwidth;
			
			redraw2 = 1;
		};
	};

	if(redraw && redraw2 && (win->status & WIN_OPEN)) {							
		if(which & VSLIDE) {
			draw_wind_elements(win,&globals.screen,WVSB);
		};
		
		if(which & HSLIDE) {
			draw_wind_elements(win,&globals.screen,WHSB);
		};
	};

	return 1;
}

/*
** Description
** setwinsize sets the size and position of window win to size
**
** 1998-10-04 CG
*/
static
void
setwinsize (WINSTRUCT * win,
            RECT      * size) {
  WORD	dx,dy,dw,dh;

  win->lastsize = win->totsize;
	
  win->totsize = * size;
	
  dx = size->x - win->lastsize.x;
  dy = size->y - win->lastsize.y;
  dw = size->width - win->lastsize.width;
  dh = size->height - win->lastsize.height;
	
  win->worksize.x += dx;
  win->worksize.y += dy;
  win->worksize.width += dw;
  win->worksize.height += dh;

  /*
  if(win->tree) {
    win->tree[0].ob_x = win->totsize.x;
    win->tree[0].ob_y = win->totsize.y;
    win->tree[0].ob_width = win->totsize.width;
    win->tree[0].ob_height = win->totsize.height;
	
    win->tree[WMOVER].ob_width += dw;
		
    win->tree[WFULLER].ob_x += dw;

    win->tree[WSMALLER].ob_x += dw;
	
    win->tree[WDOWN].ob_x += dw;
    win->tree[WDOWN].ob_y += dh;	
	
    win->tree[WSIZER].ob_x += dw;
    win->tree[WSIZER].ob_y += dh;	
	
    win->tree[WRIGHT].ob_x += dw;
    win->tree[WRIGHT].ob_y += dh;	
	
    win->tree[WLEFT].ob_y += dh;	
	
    win->tree[WVSB].ob_x += dw;
    win->tree[WVSB].ob_height += dh;	
	
    win->tree[WHSB].ob_y += dh;
    win->tree[WHSB].ob_width += dw;	
	
    win->tree[WINFO].ob_width += dw;
		
    win->tree[WUP].ob_x += dw;
		
    win->tree[TFILLOUT].ob_width += dw;
	
    win->tree[RFILLOUT].ob_height += dh;
    win->tree[RFILLOUT].ob_x += dw;
	
    win->tree[BFILLOUT].ob_width += dw;
    win->tree[BFILLOUT].ob_y += dy;
	
    win->tree[SFILLOUT].ob_x += dw;
    win->tree[SFILLOUT].ob_y += dh;

    win->tree[WAPP].ob_width = win->tree[WMOVER].ob_width;
		
    changeslider(win,0,HSLIDE | VSLIDE,-1,-1);
  }
  */
}

/*winalloc creates/fetches a free window structure/id*/

static WINSTRUCT	*winalloc(void) {
	WINLIST	*wl;
	
	if(win_free) {
		wl = win_free;
		win_free = win_free->next;
	}
	else {
		wl = (WINLIST *)Mxalloc(sizeof(WINLIST),GLOBALMEM);
		wl->win = (WINSTRUCT *)Mxalloc(sizeof(WINSTRUCT),GLOBALMEM);
		wl->win->id = win_next;
		win_next++;
	};
	
	wl->next = win_list;
	win_list = wl;
	
	return wl->win;
}

/****************************************************************************
 * find_wind_description                                                    *
 *  Find the window structure of the window with identification number id.  *
 ****************************************************************************/
static WINSTRUCT	*     /* Found description or NULL.                       */
find_wind_description(  /*                                                  */
WORD id)                /* Identification number of window.                 */
/****************************************************************************/
{
	WINLIST	*wl;

	wl = win_list;
	
	while(wl) {
		if(wl->win->id == id)
		{
			return wl->win;
		}

		wl = wl->next;
	};

	return NULL;
}

static WORD	changewinsize(WINSTRUCT *win,RECT *size,WORD vid,WORD drawall) {	
  WORD dx = size->x - win->totsize.x;
  WORD dy = size->y - win->totsize.y;
  WORD dw = size->width - win->totsize.width;
  WORD dh = size->height - win->totsize.height;
  RECT oldtotsize = win->totsize;
  RECT oldworksize = win->worksize;
  
  setwinsize(win,size);
  
  if(win->status & WIN_OPEN) {
    if(dx || dy) { /* pos (and maybe size) is to be changed */
      REDRAWSTRUCT	m;
      
      WINLIST	*wl = win_vis;
      
      while(wl)
	{
	  if(wl->win == win)
	    {
	      wl = wl->next;
	      break;
	    };
	  
	  wl = wl->next;
	};
      
      if(wl) {
	RLIST	*rlwalk;
	RLIST	*rlournew = NULL;
	RLIST	*rlourold = win->rlist;
	RLIST	*old_rlist = Rlist_duplicate(win->rlist);
	RLIST	*rlmove = NULL;
	RLIST *rlmove1 = NULL;
	WINLIST *wlwalk = wl;
	
	win->rlist = 0L;
			
	/*grab the rectangles we need from our old list*/
	
	Rlist_rectinter(&rlournew,size,&rlourold);
	
	while(wlwalk) {
	  /*get the new rectangles we need*/
	  
	  Rlist_rectinter(&rlournew,size,&wlwalk->win->rlist);
	  
	  wlwalk = wlwalk->next;
	};
	
	Rlist_insert(&win->rlist,&rlournew);
	
	if(drawall) {
	  C_APPL_WRITE c_appl_write;
	  R_APPL_WRITE r_appl_write;

	  m.type = WM_REDRAW;
	  
	  if(globals.realmove) {
	    m.sid = -1;
	  }
	  else {
	    m.sid = 0;
	  };
	  
	  m.length = 0;
	  m.wid = win->id;
	  
	  m.area = *size;
	  
	  draw_wind_elements(win,&m.area,0);
	  
	  if(globals.realmove) {
	    m.area.x = 0;
	    m.area.y = 0;
	  };
	  
	  c_appl_write.addressee = win->owner;
	  c_appl_write.length = 16;
          c_appl_write.is_reference = TRUE;
	  c_appl_write.msg.ref = &m;
	  srv_appl_write (&c_appl_write, &r_appl_write);
	}
	else {			
	  /*figure out which rectangles that are moveable*/
	  
	  if(dw || dh) {
	    Rlist_rectinter(&rlmove1,&win->worksize,&win->rlist);
	  }
	  else {
	    rlmove1 = win->rlist;
	    win->rlist = NULL;
	  };
	  
	  rlwalk = old_rlist;
	  
	  while(rlwalk) {
	    rlwalk->r.x += dx;
	    rlwalk->r.y += dy;
	    
	    Rlist_rectinter(&rlmove,&rlwalk->r,&rlmove1);
	    
	    rlwalk = rlwalk->next;
	  };
	  
	  /*move the rectangles that are moveable*/
	  
	  Rlist_sort(&rlmove,dx,dy);
	  
	  rlwalk = rlmove;
	  
	  Vdi_v_hide_c(vid);
	  
	  while(rlwalk) {
	    MFDB	mfdbd,mfdbs;
	    WORD	koordl[8];
	    
	    mfdbd.fd_addr = 0L;
	    mfdbs.fd_addr = 0L;
	    
	    koordl[4] = rlwalk->r.x;
	    koordl[5] = rlwalk->r.y + rlwalk->r.height - 1;
	    koordl[6] = rlwalk->r.x + rlwalk->r.width - 1;
	    koordl[7] = rlwalk->r.y;
	    koordl[0] = koordl[4] - dx;
	    koordl[1] = koordl[5] - dy;
	    koordl[2] = koordl[6] - dx;
	    koordl[3] = koordl[7] - dy;
	    
	    Vdi_vro_cpyfm(vid,S_ONLY,koordl,&mfdbs,&mfdbd);
	    
	    rlwalk = rlwalk->next;
	  };
	  
	  Vdi_v_show_c(vid,1);
	  
	  /*update rectangles that are not moveable*/
	  
	  m.type = WM_REDRAW;
	  
	  if(globals.realmove) {
	    m.sid = -1;
	  }
	  else {
	    m.sid = 0;
	  };
	  
	  m.length = 0;
	  m.wid = win->id;
	  
	  Rlist_insert(&win->rlist,&rlmove1);
	  
	  rlwalk = win->rlist;
	  
	  while(rlwalk) {
	    C_APPL_WRITE c_appl_write;
            R_APPL_WRITE r_appl_write;

	    m.area.x = rlwalk->r.x;
	    m.area.y = rlwalk->r.y;
	    
	    m.area.width = rlwalk->r.width;
	    m.area.height = rlwalk->r.height;
	    
	    draw_wind_elemfast(win,&m.area,0);
	    
	    if(globals.realmove) {
	      m.area.x -= size->x;
	      m.area.y -= size->y;
	    };
	    
	    c_appl_write.addressee = win->owner;
	    c_appl_write.length = 16;
            c_appl_write.is_reference = TRUE;
	    c_appl_write.msg.ref = &m;
	    srv_appl_write (&c_appl_write, &r_appl_write);
	    
	    rlwalk = rlwalk->next;
	  };
	  
	  Rlist_insert(&win->rlist,&rlmove);
	};
	
	Rlist_erase(&old_rlist);
	
	wlwalk = wl;
	
	while(wlwalk) {
	  RLIST	*rltheirnew = NULL;
	  
	  /*give away the rectangles we don't need*/
	  
	  Rlist_rectinter(&rltheirnew,&wlwalk->win->totsize,&rlourold);
	  
	  /*update the new rectangles*/
	  
	  rlwalk = rltheirnew;
	  
	  while(rlwalk) {
	    m.area.width = rlwalk->r.width;
	    m.area.height = rlwalk->r.height;
	    
	    m.area.x = rlwalk->r.x;
	    m.area.y = rlwalk->r.y;
	    
	    draw_wind_elemfast(wlwalk->win,&m.area,0);
	    
	    rlwalk = rlwalk->next;
	  };				
	  
	  if(rltheirnew && !(wlwalk->win->status & WIN_DESKTOP)) {
	    C_APPL_WRITE c_appl_write;
            R_APPL_WRITE r_appl_write;

	    m.type = WM_REDRAW;
	    
	    if(globals.realmove) {
	      m.sid = -1;
	    }
	    else {
	      m.sid = 0;
	    };
	    
	    m.length = 0;
	    m.wid = wlwalk->win->id;
	    
	    m.area = oldtotsize;
	    
	    if(globals.realmove) {
	      m.area.x -= wlwalk->win->totsize.x;
	      m.area.y -= wlwalk->win->totsize.y;
	    };
	    
	    c_appl_write.addressee = wlwalk->win->owner;
	    c_appl_write.length = 16;
            c_appl_write.is_reference = TRUE;
	    c_appl_write.msg.ref = &m;
	    srv_appl_write (&c_appl_write, &r_appl_write);
	  }
	  
	  Rlist_insert(&wlwalk->win->rlist,&rltheirnew);
	  
	  wlwalk->win->rpos = wlwalk->win->rlist;
	  
	  wlwalk = wlwalk->next;
	};
	
	win->rpos = win->rlist;
      };			
    }
    else if(dw || dh)	/*only size changed*/ {
      RECT	rt;
      RECT	dn;
      
      REDRAWSTRUCT	m;
      
      RLIST	*rl = 0L;
      RLIST	*rl2;
      RLIST	*rltop = 0L;
      
      WINLIST	*wl = win_vis;
      
      
      rt.x = size->x + size->width;
      
      if(dw < 0) {
	rt.width = -dw;
      }
      else {
	rt.x -= dw;
	rt.width = dw;
      };
      
      rt.y = size->y;
      rt.height = size->height;
      
      dn.y = size->y + size->height;
      
      if(dh < 0) {
	dn.height = -dh;
	dn.width = oldtotsize.width;
      }
      else {
	dn.y -= dh;
	dn.height = dh;
	dn.width = size->width;
      };
      
      dn.x = size->x;
      
      if(dw < 0) {
	Rlist_rectinter(&rl,&rt,&win->rlist);
      };
      
      if(dh < 0) {
	Rlist_rectinter(&rl,&dn,&win->rlist);
      };
      
      /* Find our window */
      
      while(wl) {
	if(wl->win == win) {
	  wl = wl->next;
	  break;
	};
	
	wl = wl->next;
      };
      
      while(wl) {
	RLIST	*rd = 0;
	
	if(dw < 0) {
	  Rlist_rectinter(&rd,&wl->win->totsize,&rl);
	}
	else if(dw > 0) {
	  Rlist_rectinter(&rltop,&rt,&wl->win->rlist);
	};
	
	if(dh < 0) {
	  Rlist_rectinter(&rd,&wl->win->totsize,&rl);
	}
	else if(dh > 0) {
	  Rlist_rectinter(&rltop,&dn,&wl->win->rlist);
	};
	
	rl2 = rd;
	
	while(rl2) {
	  draw_wind_elemfast(wl->win,&rl2->r,0);
	  
	  
	  rl2 = rl2->next;
	};
	
	m.type = WM_REDRAW;
	
	if(globals.realmove) {
	  m.sid = -1;
	}
	else {
	  m.sid = 0;
	};
	
	m.length = 0;

	if(rd && !(wl->win->status & WIN_DESKTOP)) {
	  C_APPL_WRITE c_appl_write;
          R_APPL_WRITE r_appl_write;
          
	  m.wid = wl->win->id;
	  
	  m.area = oldtotsize;
	  
	  if(globals.realmove) {
	    m.area.x -= wl->win->totsize.x;
	    m.area.y -= wl->win->totsize.y;
	  };

	  c_appl_write.addressee = wl->win->owner;
	  c_appl_write.length = 16;
          c_appl_write.is_reference = TRUE;
	  c_appl_write.msg.ref = &m;
	  srv_appl_write (&c_appl_write, &r_appl_write);
	};
	
	Rlist_insert(&wl->win->rlist,&rd);
				
	wl->win->rpos = wl->win->rlist;
	
	wl = wl->next;
      };
      
      rl2 = 0;
      
      Rlist_rectinter(&rl2,&oldworksize,&win->rlist);
      
      Rlist_rectinter(&rltop,&win->worksize,&win->rlist);
      
      Rlist_insert(&win->rlist,&rl2);
      
      rl2 = rltop;
      rltop = 0L;
      
      Rlist_insert(&rltop,&rl2);
      
      m.wid = win->id;
      
      rl2 = rltop;
      
      while(rl2) {
	C_APPL_WRITE c_appl_write;
        R_APPL_WRITE r_appl_write;

	m.area.x = rl2->r.x;
	m.area.y = rl2->r.y;
	
	if(globals.realmove) {
	  m.area.x -= size->x;
	  m.area.y -= size->y;
	};
	
	m.area.width = rl2->r.width;
	m.area.height = rl2->r.height;
	
	c_appl_write.addressee = win->owner;
	c_appl_write.length = 16;
        c_appl_write.is_reference = TRUE;
	c_appl_write.msg.ref = &m;
	srv_appl_write (&c_appl_write, &r_appl_write);
	
	rl2 = rl2->next;
      };
      
      Rlist_insert(&win->rlist,&rltop);
      
      rl2 = win->rlist;
      
      while(rl2) {
	draw_wind_elemfast(win,&rl2->r,0);
	
	rl2 = rl2->next;
      };
      
      win->rpos = win->rlist;
    };
  };
  
  return 1;
}

static WORD	bottom_window(WORD winid) { 
  WORD          wastopped = 0;
  WINSTRUCT     *newtop = 0L;
  REDRAWSTRUCT	m;
  
  WINLIST	**wl = &win_vis;
  WINLIST	*ourwl;
  
  while(*wl) {
    if((*wl)->win->id == winid)
      break;
    
    wl = &(*wl)->next;
  };
  
  if(!*wl) {
    return 0;
  };
  
  ourwl = *wl;
  
  *wl = (*wl)->next;
  
  if(*wl) {
    if((*wl)->win->status & WIN_MENU) {
      wl = &(*wl)->next;
    };
  };
  
  if((ourwl->win->status & WIN_TOPPED) && *wl) {
    if(!((*wl)->win->status & WIN_DESKTOP)) {
      newtop = (*wl)->win;
      (*wl)->win->status |= WIN_TOPPED;
      ourwl->win->status &= ~WIN_TOPPED;
      wastopped = 1;
    };
  };
  
  m.type = WM_REDRAW;
  
  if(globals.realmove) {
    m.sid = -1;
  }
  else {
    m.sid = 0;
  };
  
  m.length = 0;
  
  while(*wl) {
    RLIST *newrects = 0L;
    
    if((*wl)->win->status & WIN_DESKTOP) {
      break;
    };
    
    Rlist_rectinter(&newrects,&(*wl)->win->totsize,&ourwl->win->rlist);
    
    Rlist_insert(&(*wl)->win->rlist,&newrects);
    
    if(!((*wl)->win->status & WIN_MENU)) {
      C_APPL_WRITE c_appl_write;
      R_APPL_WRITE r_appl_write;

      m.wid = (*wl)->win->id;
      
      m.area.x = ourwl->win->totsize.x;
      m.area.y = ourwl->win->totsize.y;
      
      m.area.width = ourwl->win->totsize.width;
      m.area.height = ourwl->win->totsize.height;
      
      if((*wl)->win != newtop) {
	draw_wind_elements((*wl)->win,&m.area,0);
      };
      
      if(globals.realmove) {
	m.area.x -= (*wl)->win->totsize.x;
	m.area.y -= (*wl)->win->totsize.y;
      };

      c_appl_write.addressee = (*wl)->win->owner;
      c_appl_write.length = 16;
      c_appl_write.is_reference = TRUE;
      c_appl_write.msg.ref = &m;
      srv_appl_write (&c_appl_write, &r_appl_write);
    };
    
    wl = &(*wl)->next;
  };
  
  ourwl->next = *wl;
  *wl = ourwl;
  
  if(wastopped) {
    WORD           i;
    C_APPL_CONTROL c_appl_control;

    for(i = 0; i <= W_SMALLER; i++) {
      set_widget_colour(ourwl->win,i,&ourwl->win->untop_colour[i],&ourwl->win->top_colour[i]);
      set_widget_colour(newtop,i,&newtop->top_colour[i],&newtop->top_colour[i]);
    };
    
    draw_wind_elements(ourwl->win,&ourwl->win->totsize,0);
    draw_wind_elements(newtop,&newtop->totsize,0);
    
    c_appl_control.apid = newtop->owner;
    c_appl_control.mode = APC_TOP;
    srv_appl_control (&c_appl_control);
  };
  
  return 1;
}

static WORD top_window(WORD winid) { 
  WORD         wastopped = 0;
  WINSTRUCT    *oldtop = NULL;
  REDRAWSTRUCT m;
  
  RLIST        *rl = 0L;
  RLIST	       *rl2 = 0L;
  
  WINLIST      **wl = &win_vis;
  WINLIST      *wl2;
  WINLIST      *ourwl;
  
  WORD	dx,dy;
	
  if(winid == 0) {
    if(win_vis && (win_vis->win->status & WIN_TOPPED)) {
      WORD i;
      
      win_vis->win->status &= ~WIN_TOPPED;
      
      for(i = 0; i <= W_SMALLER; i++) {
	set_widget_colour(win_vis->win,i,&win_vis->win->untop_colour[i],
			  &win_vis->win->top_colour[i]);
      };
      
      draw_wind_elements(win_vis->win,&win_vis->win->totsize,0);
    };
  }
  else {
    while(*wl) {
      if((*wl)->win->id == winid)
	break;
      
      wl = &(*wl)->next;
    };
    
    if(!*wl)
      return 0;
    
    wl2 = *wl;
    
    *wl = (*wl)->next;
    
    if(win_vis) {
      if(win_vis->win->status & WIN_DIALOG) {
	wl2->next = win_vis->next;
	win_vis->next = wl2;
      }
      else {
	oldtop = win_vis->win;
	wl2->next = win_vis;
	win_vis = wl2;
	
	if(!(wl2->win->status & WIN_TOPPED)) {
	  wl2->win->status |= WIN_TOPPED;
	  oldtop->status &= ~WIN_TOPPED;
	  
	  wastopped = 1;
	};
      };
    }
    else {	
      wl2->next = 0L;
      win_vis = wl2;
    };
    
    m.type = WM_REDRAW;
    
    if(globals.realmove) {
      m.sid = -1;
    }
    else {
      m.sid = 0;
    };
    
    ourwl = wl2;
    
    m.wid = ourwl->win->id;
    m.length = 0;
    
    dx = wl2->win->totsize.x;
    dy = wl2->win->totsize.y;	
    
    wl2 = wl2->next;
	
    while(wl2) {
      RLIST	*rd = 0L;
      
      Rlist_rectinter(&rl,&ourwl->win->totsize,&wl2->win->rlist);
      
      Rlist_insert(&rd,&wl2->win->rlist);
      
      wl2->win->rlist = rd;
      wl2->win->rpos = wl2->win->rlist;
      
      wl2 = wl2->next;
    };
    
    
    Rlist_insert(&rl2,&rl);
    
    rl = rl2;
    
    while(rl) {
      C_APPL_WRITE c_appl_write;
      R_APPL_WRITE r_appl_write;

      m.area.x = rl->r.x;
      m.area.y = rl->r.y;
      
      m.area.width = rl->r.width;
      m.area.height = rl->r.height;
      
      if(!wastopped) {
	draw_wind_elemfast(ourwl->win,&rl->r,0);
      };
      
      if(globals.realmove) {
	m.area.x -= dx;
	m.area.y -= dy;
      };

      c_appl_write.addressee = ourwl->win->owner;
      c_appl_write.length = 16;
      c_appl_write.is_reference = TRUE;
      c_appl_write.msg.ref = &m;
      srv_appl_write (&c_appl_write, &r_appl_write);
      
      rl = rl->next;
    };
    
    Rlist_insert(&ourwl->win->rlist,&rl2);
    
    ourwl->win->rpos = ourwl->win->rlist;
    
    if(wastopped) {
      WORD i;
      C_APPL_CONTROL c_appl_control;
      
      for(i = 0; i <= W_SMALLER; i++) {
	set_widget_colour(oldtop,i,&oldtop->untop_colour[i],
			  &oldtop->top_colour[i]);
	set_widget_colour(ourwl->win,i,&ourwl->win->untop_colour[i],
			  &ourwl->win->top_colour[i]);
      };
	
      draw_wind_elements(ourwl->win,&ourwl->win->totsize,0);
      draw_wind_elements(oldtop,&oldtop->totsize,0);
      
      c_appl_control.apid = ourwl->win->owner;
      c_appl_control.mode = APC_TOP;
      srv_appl_control(&c_appl_control);
    };
  };
  
  return 1;
}

/****************************************************************************
 * srv_wind_change                                                          *
 *  Change state of window widget.                                          *
 ****************************************************************************/
static WORD        /* 1 if ok, 0 if error.                                 */
srv_wind_change(   /*                                                       */
C_WIND_CHANGE *msg)
/****************************************************************************/
{
  WINSTRUCT *win = find_wind_description(msg->id);
	
  if(win) {
    if(msg->newstate != win->tree[widgetmap[msg->object]].ob_state) {
      win->tree[widgetmap[msg->object]].ob_state = msg->newstate;
      draw_wind_elements(win,&globals.screen,widgetmap[msg->object]);
    };
    
    return 1;
  };
  
  return 0;
}

/****************************************************************************
 * srv_wind_close                                                           *
 *  Implementation of wind_close().                                         *
 ****************************************************************************/
WORD            /* 0 if error or 1 if ok.                                   */
srv_wind_close( /*                                                          */
C_WIND_CLOSE *msg)
/****************************************************************************/
{
  WINLIST   **wl = &win_vis;
  WINSTRUCT *newtop = NULL;
	
  while(*wl) {
    if((*wl)->win->id == msg->id)
      break;
    
    wl = &(*wl)->next;
  };
  
  if(*wl) {
    WINLIST      *wp = (*wl)->next;
    REDRAWSTRUCT m;
    
    if(((*wl)->win->status & WIN_TOPPED) && wp) {
      (*wl)->win->status &= ~WIN_TOPPED;
      wp->win->status |= WIN_TOPPED;
      newtop = wp->win;
    };
    
    while(wp) {			
      RLIST	*rl = NULL;
      RLIST	*tl;
      
      Rlist_rectinter(&rl,&wp->win->totsize,&(*wl)->win->rlist);
      
      /* Redraw "new" rectangles of windows below */
      
      if(rl) {
	C_APPL_WRITE c_appl_write;
        R_APPL_WRITE r_appl_write;

	if(!(wp->win->status & (WIN_DESKTOP | WIN_MENU))) {
	  m.type = WM_REDRAW;
	  m.length = 0;
	  m.wid = wp->win->id;
	  
	  if(globals.realmove) {
	    m.sid = -1;
	    m.area.x = (*wl)->win->totsize.x - wp->win->totsize.x;
	    m.area.y = (*wl)->win->totsize.y - wp->win->totsize.y;
	  }
	  else {
	    m.sid = 0;
	    m.area.x = (*wl)->win->totsize.x;
	    m.area.y = (*wl)->win->totsize.y;
	  };
	  
	  m.area.width = (*wl)->win->totsize.width;
	  m.area.height = (*wl)->win->totsize.height;	

	  c_appl_write.addressee = wp->win->owner;
	  c_appl_write.length = 16;
          c_appl_write.is_reference = TRUE;
	  c_appl_write.msg.ref = &m;

	  srv_appl_write (&c_appl_write, &r_appl_write);
	};
	
	if(wp->win != newtop) {
	  tl = rl;
	  
	  while(tl) {				
	    draw_wind_elemfast(wp->win,&tl->r,0);
	    
	    tl = tl->next;
	  };
	};
	
	Rlist_insert(&wp->win->rlist,&rl);
	wp->win->rpos = wp->win->rlist;
      };		
      
      wp = wp->next;
    }		
    
    wp = *wl;
    
    *wl = (*wl)->next;
    
    wp->win->status &= ~WIN_OPEN;
    Mfree(wp);
    
    if(newtop) {
      WORD i;
      
      for(i = 0; i <= W_SMALLER; i++) {
	set_widget_colour(newtop,i,&newtop->untop_colour[i],
			  &newtop->top_colour[i]);
      };
      
      draw_wind_elements(newtop,&newtop->totsize,0);
    };
    
    return 1;
  }
  else {
    return 0;
  }
}


/****************************************************************************
 * srv_wind_create                                                          *
 *  Implementation of wind_create().                                        *
 ****************************************************************************/
void
srv_wind_create( /*                                                         */
C_WIND_CREATE * msg,
R_WIND_CREATE * ret)
/****************************************************************************/
{
  WINSTRUCT	*ws;
  
  ws = winalloc();
  
  ws->status = msg->status;
  ws->owner = msg->common.apid; 
	
  ws->maxsize = msg->maxsize;

  ws->rlist = NULL;
  ws->rpos = NULL;
  
  ws->vslidepos = 1;
  ws->vslidesize = 1000;
  ws->hslidepos = 1;
  ws->hslidesize = 1000;

  /*
  if((ws->status & WIN_DIALOG) || (ws->status & WIN_MENU)) {
    ws->tree = 0L;
    ws->totsize = msg->maxsize;
    ws->worksize = ws->totsize;
  }
  else {
    WORD    i;
    AP_INFO *ai;
    
    ws->tree = alloc_win_elem();
    set_win_elem(ws->tree,msg->elements);
    ws->elements = msg->elements;
	
    ai = internal_appl_info(msg->common.apid);
		
    if(ai) {
      ws->tree[WAPP].ob_spec.tedinfo->te_ptext =
	(char *)Mxalloc(strlen(&ai->name[2]) + 1,GLOBALMEM);
      strcpy(ws->tree[WAPP].ob_spec.tedinfo->te_ptext,&ai->name[2]);
      
      if(globals.wind_appl == 0) {
	ws->tree[WAPP].ob_spec.tedinfo->te_ptext[0] = 0;
      };
    };
    
    ws->totsize.x = ws->tree[0].ob_x;
    ws->totsize.y = ws->tree[0].ob_y;
    ws->totsize.width = ws->tree[0].ob_width;
    ws->totsize.height = ws->tree[0].ob_height;
    
    calcworksize(msg->elements,&ws->totsize,&ws->worksize,WC_WORK);
		
    for(i = 0; i <= W_SMALLER; i++) {
      ws->top_colour[i] = top_colour[i];
      ws->untop_colour[i] = untop_colour[i];
    }
    
    ws->own_colour = 0;
  };
  */

  ret->common.retval = ws->id;
}

/****************************************************************************
 * srv_wind_delete                                                          *
 *  Implementation of wind_delete().                                        *
 ****************************************************************************/
WORD             /* 0 if error or 1 if ok.                                  */
srv_wind_delete( /*                                                         */
C_WIND_DELETE * msg,
R_WIND_DELETE * ret)
/****************************************************************************/
{
  WINLIST	**wl;
  
  wl = &win_list;
  
  while(*wl) {		
    if((*wl)->win->id == msg->id)
      break;
    
    wl = &(*wl)->next;
  };
  
  if(!(*wl)) {
    /*no such window found! return*/
    return 0;
  };
  
  if((*wl)->win->status & WIN_OPEN) {
    C_WIND_CLOSE c_wind_close;

    c_wind_close.id = msg->id;

    srv_wind_close(&c_wind_close);
  };
  
  delwinelem((*wl)->win->tree);
  
  if(msg->id == (win_next - 1)) {
    WORD	found;
    WINLIST	*wt = *wl;
    
    *wl = (*wl)->next;
    
    Mfree(wt->win);
    Mfree(wt);
    win_next--;
    
    do {
      WINLIST 	**wlwalk = &win_list;
      
      found = 0;
      
      while(*wlwalk) {
	if((*wlwalk)->win->id == (win_next - 1))
	  {
	    found = 1;
	    break;
	  };
	
	wlwalk = &(*wlwalk)->next;
      };
      
      if(!found) {
	wlwalk = &win_free;
	
	while(*wlwalk) {
	  if((*wlwalk)->win->id == (win_next - 1)) {
	    WINLIST	*wltemp = *wlwalk;
	    
	    *wlwalk = (*wlwalk)->next;
	    
	    Mfree(wltemp->win);
	    Mfree(wltemp);
	    
	    win_next--;
	    
	    break;
	  };
	  
	  wlwalk = &(*wlwalk)->next;
	};
      };
      
    }while(!found && win_next);
  }
  else {
    WINLIST	*wt = *wl;
    
    *wl = (*wl)->next;
    
    wt->next = win_free;
    win_free = wt;
  };
  
  ret->common.retval = 1;
}

/****************************************************************************
 * srv_wind_draw                                                            *
 *  Draw window widgets.                                                    *
 ****************************************************************************/
static WORD      /* 0 if an error occured, or >0                            */
srv_wind_draw(   /*                                                         */
C_WIND_DRAW *msg)
/****************************************************************************/
{
  WINSTRUCT *win = find_wind_description(msg->handle);
	
  if(win) {
    draw_wind_elements(win,&globals.screen,widgetmap[msg->object]);
    
    return 1;
  };
  
  return 0;
}

/****************************************************************************
 * srv_wind_find                                                            *
 *  Find window on known coordinates.                                       *
 ****************************************************************************/
WORD           /* Window handle.                                            */
srv_wind_find( /*                                                           */
C_WIND_FIND *msg)
/****************************************************************************/
{
  WINLIST *l = win_vis;
  
  while(l) {
    if(Misc_inside(&l->win->totsize,msg->x,msg->y)) {
      return l->win->id;
    };
    
    l = l->next;
  };
  
  return 0;
}

/****************************************************************************
 * srv_wind_get                                                             *
 *  Implementation of wind_get().                                           *
 ****************************************************************************/
void
srv_wind_get(  /*                                                           */
C_WIND_GET * msg,
R_WIND_GET * ret)
/****************************************************************************/
{
  WINSTRUCT	*win;
  
  win = find_wind_description(msg->handle);
  
  if(win) {
    switch(msg->mode) {	
    case WF_KIND: /* 0x0001 */
      ret->common.retval = 1;
      ret->parm1 = win->elements;
      break;
      
    case WF_WORKXYWH	:	/*0x0004*/
      ret->common.retval = 1;
      ret->parm1 = win->worksize.x;
      ret->parm2 = win->worksize.y;
      ret->parm3 = win->worksize.width;
      ret->parm4 = win->worksize.height;
      break;
      
    case	WF_CURRXYWH	:	/*0x0005*/
      ret->common.retval = 1;
      ret->parm1 = win->totsize.x;
      ret->parm2 = win->totsize.y;
      ret->parm3 = win->totsize.width;
      ret->parm4 = win->totsize.height;
				break;
      
    case	WF_PREVXYWH	:	/*0x0006*/
      ret->common.retval = 1;
      ret->parm1 = win->lastsize.x;
      ret->parm2 = win->lastsize.y;
      ret->parm3 = win->lastsize.width;
      ret->parm4 = win->lastsize.height;
      break;
      
    case	WF_FULLXYWH	:	/*0x0007*/
      ret->common.retval = 1;
      ret->parm1 = win->maxsize.x;
      ret->parm2 = win->maxsize.y;
      ret->parm3 = win->maxsize.width;
      ret->parm4 = win->maxsize.height;
      break;
      
    case WF_HSLIDE: /*0x08*/
      ret->common.retval = 1;
      ret->parm1 = win->hslidepos;
      break;
      
    case WF_VSLIDE: /*0x09*/
      ret->common.retval = 1;
      ret->parm1 = win->vslidepos;
      break;
      
    case WF_TOP: /*0x000a*/
      if(win_vis) {
	ret->parm1 = win_vis->win->id;
	ret->parm2 = win_vis->win->owner;
	
	if(win_vis && win_vis->next) {
	  ret->parm3 = win_vis->next->win->id;
	}
	else {
	  ret->parm3 = -1;
	};
	
	ret->common.retval = 1;
      }
      else {
	ret->common.retval = 0;
      };
      break;
      
    case	WF_FIRSTXYWH:	/*0x000b*/
      win->rpos = win->rlist;
    case	WF_NEXTXYWH:	/*0x000c*/
      {
	RECT r;
	
	ret->common.retval = 1;
	
	while(win->rpos) {
	  if(Misc_intersect(&win->rpos->r,&win->worksize,&r)) {
	    break;
	  };
	  
	  win->rpos = win->rpos->next;
	};					
	
	if(!win->rpos) {
	  ret->parm1 = 0;
	  ret->parm2 = 0;
	  ret->parm3 = 0;
	  ret->parm4 = 0;
	}
	else {
	  win->rpos = win->rpos->next;
	  
	  ret->parm1 = r.x;
	  ret->parm2 = r.y;
	  ret->parm3 = r.width;
	  ret->parm4 = r.height;
	};
      };
      break;
      
    case WF_HSLSIZE: /*0x0f*/
      ret->common.retval = 1;
      ret->parm1 = win->hslidesize;
      break;				
      
    case WF_VSLSIZE: /*0x10*/
      ret->common.retval = 1;
      ret->parm1 = win->vslidesize;
      break;				
      
    case WF_SCREEN: /*0x11*/
      ret->common.retval = 1;
      ret->parm1 = 0;
      ret->parm2 = 0;
      ret->parm3 = 0;
      ret->parm4 = 0;
      break;
      
    case WF_OWNER: /*0x14*/
      if(win) {
	ret->common.retval = 1;
	ret->parm1 = win->owner;
	ret->parm2 = win->status;
	
	if(win->status & WIN_OPEN) {
	  WINLIST *wl = win_vis;
	  
	  
	  if(wl->win == win) {
	    ret->parm3 = -1;
	  }
	  else if(wl) {
	    while(wl->next) {
	      if(wl->next->win == win) {
		ret->parm3 = wl->win->id;
		break;
	      };
	      
	      wl = wl->next;
	    };
	    
	    wl = wl->next;
	  };
	  
	  if(wl) {
	    wl = wl->next;
	    
	    if(wl) {
	      ret->parm4 = wl->win->id;
	    }
	    else {
	      ret->parm4 = -1;
	    };
						}
	  else {
	    ret->parm4 = -1;
	  };							
	}
	else {
	  ret->parm3 = -1;
	  ret->parm4 = -1;
	};
      }
      else {
	ret->common.retval = 0;
      };
      break;
      
    case WF_ICONIFY: /* 0x1a */
      if(win->status & WIN_ICONIFIED) {
	ret->parm1 = 1;
      }
      else {
	ret->parm1 = 0;
      };
      
      ret->parm2 = globals.icon_width;
      ret->parm3 = globals.icon_height;
      
      ret->common.retval = 1;
      break;
      
    case WF_UNICONIFY: /* 0x1b */
      ret->common.retval = 1;
      ret->parm1 = win->origsize.x;
      ret->parm2 = win->origsize.y;
      ret->parm3 = win->origsize.width;
      ret->parm4 = win->origsize.height;
      break;
      
    case WF_WINX:
    case WF_WINXCFG:
      ret->common.retval = 0;
      break;
      
    default:
      DB_printf("%s: Line %d: srv_wind_get:\r\n"
		"Unknown mode %d  (0x%x) wind_get(%d,%d,...)",
		__FILE__,__LINE__,msg->mode,msg->mode,
		msg->handle,msg->mode);
      ret->common.retval = 0;
    };
  }
  else
    ret->common.retval = 0;
  
  return ret;
}

/****************************************************************************
 * srv_wind_open                                                            *
 *  Implementation of wind_open().                                          *
 ****************************************************************************/
void
srv_wind_open(     /*                                                       */
C_WIND_OPEN * msg,
R_WIND_OPEN * ret)
/****************************************************************************/
{
  WINLIST      *wl,*wp;
  WINSTRUCT    *ws,*oldtop = NULL;
  RLIST	       *rl = 0L;
  REDRAWSTRUCT m;
  WORD         owner;
  WORD         i;
  WORD         wastopped = 0;

  ws = find_wind_description (msg->id);
  
  if(ws) {
    if(!(ws->status & WIN_OPEN)) {
      wl = (WINLIST *)Mxalloc(sizeof(WINLIST),GLOBALMEM);
      
      wl->win = ws;
      
      DB_printf ("srv.c: srv_wind_open: call setwinsize");
      setwinsize (wl->win, &msg->size);
      DB_printf ("srv.c: srv_wind_open: returned from setwinsize");
      
      if(win_vis) {
	if(win_vis->win->status & WIN_DIALOG) {
	  wl->next = win_vis->next;
	  win_vis->next = wl;
	  wl->win->status &= ~WIN_TOPPED;
	} else {					
	  oldtop = win_vis->win;
	  wl->next = win_vis;
	  win_vis = wl;
	  
	  if(!(wl->win->status & WIN_MENU)) {
	    wl->win->status |= WIN_TOPPED;
	    oldtop->status &= ~WIN_TOPPED;
	    wastopped = 1;
	  };
	};
      } else {	
	wl->next = 0L;
	win_vis = wl;
	wl->win->status |= WIN_TOPPED;
      };
      
      wp = wl->next;
      
      while(wp != 0L) {
	RLIST	*rd = 0L;
	
	Rlist_rectinter(&rl,&wl->win->totsize,&wp->win->rlist);
	
	Rlist_insert(&rd,&wp->win->rlist);
	
	wp->win->rlist = rd;
	wp = wp->next;
      }
      
      wl->win->rlist = 0L;
      Rlist_insert(&wl->win->rlist,&rl);
      
      wl->win->status |= WIN_OPEN;
      
      /*
      for(i = 0; i <= W_SMALLER; i++) {
	set_widget_colour(ws,i,&ws->untop_colour[i],&ws->top_colour[i]);
      };
      */
      
      if(!(wl->win->status & (WIN_DIALOG | WIN_MENU))){
	C_APPL_WRITE c_appl_write;
        R_APPL_WRITE r_appl_write;

	m.type = WM_REDRAW;
	m.length = 0;
        
	/*
          if(globals.realmove) {
        m.sid = -1; */
        /*x and y are relative to the position of the window*/
        /*	  m.area.x = 0;
                  m.area.y = 0;
          } else {
        */
        m.sid = 0;
        m.area.x = wl->win->totsize.x;
        m.area.y = wl->win->totsize.y;
        /*	}*/
	
	m.area.width = wl->win->totsize.width;
	m.area.height = wl->win->totsize.height;
	
	m.wid = wl->win->id;
	
	owner = wl->win->owner;
	
        /*
	draw_wind_elements(wl->win,&wl->win->totsize,0);
        */

	c_appl_write.addressee = owner;
	c_appl_write.length = MSG_LENGTH;
        c_appl_write.is_reference = TRUE;
	c_appl_write.msg.ref = &m;
	srv_appl_write (&c_appl_write, &r_appl_write);
      }
      
      /*
      if(wastopped) {
	WORD           i;
	C_APPL_CONTROL c_appl_control;
	
	for(i = 0; i <= W_SMALLER; i++) {
	  set_widget_colour(oldtop,i,&oldtop->untop_colour[i],
			    &oldtop->top_colour[i]);
	}
	
	draw_wind_elements (oldtop,&oldtop->totsize,0);
	
	c_appl_control.apid = wl->win->owner;
	c_appl_control.mode = APC_TOP;
	srv_appl_control(&c_appl_control);
      }
      */
    }
    else {
      ret->common.retval = 0;
    }
  }
  else {
    ret->common.retval = 0;
  }
  
  ret->common.retval = 1;
}


/*
** Description
** Server part of wind_update
**
** 1998-10-04 CG
*/
void
srv_wind_update (C_WIND_UPDATE * msg,
                 R_WIND_UPDATE * ret)
{
  static WORD update_lock = 0,update_cnt = 0;
  static WORD mouse_lock,mouse_cnt = 0;

  /*  WORD clnt_pid = Pgetpid();*/
  /*  long timeout = (mode & 0x100) ? 0L : -1L;*/ /* test for check-and-set mode */
  
  switch(msg->mode) {
  case BEG_UPDATE:	/* Grab the update lock */
  case BEG_UPDATE | 0x100:
    if (update_cnt > 0) {
      if (update_lock == msg->common.apid) {
        update_cnt++ ;
      } else {
        /*        DB_printf ("srv.c: srv_wind_update: Real locking not implemented yet");*/
      }
    } else {
      update_lock = msg->common.apid;
      update_cnt = 1;
    }
    break;
  
  case END_UPDATE:
    if (update_lock == msg->common.apid) {
      update_cnt--;
    } else {
      /*      DB_printf ("srv.c: srv_wind_update: Called END_UPDATE by other application");*/
    }
    break;
  
  case BEG_MCTRL:		/* Grab the mouse lock */
  case BEG_MCTRL | 0x100:
    if (mouse_lock == msg->common.apid) {
      mouse_cnt++ ;
      break ;
    }
     
    /*    if (Psemaphore(2,MOUSE_LOCK,timeout) == -EACCESS) {
      return 0;	 mouse locked by different process 
    }*/
    
    mouse_lock=msg->common.apid;
    mouse_cnt=1 ;
    break;
    
  case END_MCTRL:
    if ((mouse_lock == msg->common.apid) && (--mouse_cnt==0)) {
      mouse_lock=FALSE;
      /*      Psemaphore(3,MOUSE_LOCK,0);*/
    }
  }

  ret->common.retval = 1;
}


/*wind_set 0x0069*/
WORD srv_wind_set(WORD apid, C_WIND_SET *msg) {
  WINSTRUCT	*win;
  
  win = find_wind_description(msg->handle);
  
  if(win) {
    switch(msg->mode) {	
    case	WF_NAME	:	/*0x0002*/
    case	WF_INFO	:	/*0x0003*/
      {
	WORD object = (msg->mode == WF_NAME) ? WMOVER : WINFO;
	BYTE *str = (BYTE *)INTS2LONG(msg->parm1,msg->parm2);
	
	if(win->tree) {
	  Mfree(win->tree[object].ob_spec.tedinfo->te_ptext);
	  
	  win->tree[object].ob_spec.tedinfo->te_ptext =
	    (BYTE *)Mxalloc(strlen(str) + 1,GLOBALMEM);
	  
	  strcpy(win->tree[object].ob_spec.tedinfo->te_ptext,str);
	  
	  draw_wind_elements(win,&win->totsize,object);
	  
	  return 1;
	}
	else {
	  return 0;
	};
      };
      
    case	WF_CURRXYWH	:	/*0x0005*/
      return changewinsize(win,(RECT *)&msg->parm1,svid,0);
            
    case	WF_HSLIDE: /*0x08*/
      return changeslider(win,1,HSLIDE,msg->parm1,-1);
      
    case	WF_VSLIDE: /*0x09*/
      return changeslider(win,1,VSLIDE,msg->parm1,-1);
      
    case	WF_TOP	:	/*0x000a*/
      return top_window(msg->handle);
      
    case WF_NEWDESK: /*0x000e*/
      {
	OBJECT *tree = (OBJECT *)INTS2LONG(msg->parm1, msg->parm2);
	
	if(tree) {	
	  if(tree->ob_y + tree->ob_height < globals.screen.height) {
	    tree->ob_y =
	      globals.screen.height - tree->ob_height;
	  };
	};
	
	if(set_deskbg(apid,tree) == 0) {
	  return 1;
	}
	else {
	  return 0;
	};
      };
      
    case	WF_HSLSIZE: /*0x0f*/
      return changeslider(win,1,HSLIDE,-1,msg->parm1);
      
    case	WF_VSLSIZE: /*0x10*/
      return changeslider(win,1,VSLIDE,-1,msg->parm1);
      
    case WF_COLOR:  /*0x12*/
      DB_printf("srv_wind_set WF_COLOR not implemented");
      return 0;
      
    case WF_DCOLOR: /*0x13*/
      top_colour[msg->parm1] = *(OBJC_COLORWORD *)&msg->parm2;
      untop_colour[msg->parm1] = *(OBJC_COLORWORD *)&msg->parm3;
      
      return 0;
      
    case WF_BEVENT: /*0x18*/
      if(msg->parm1 & 1) {
	win->status |= WIN_UNTOPPABLE;
      }
      else {
	win->status &= ~WIN_UNTOPPABLE;
      };
      
      return 1;
      
    case WF_BOTTOM: /*0x0019*/
      return bottom_window(msg->handle);
      
    case WF_ICONIFY: /*0x1a*/
      win->origsize = win->totsize;
      set_win_elem(win->tree,IMOVER);
      win->status |= WIN_ICONIFIED;
      calcworksize(IMOVER,&win->totsize,&win->worksize,WC_WORK);
      return changewinsize(win,(RECT *)&msg->parm1,svid,1);

    case WF_UNICONIFY: /*0x1b*/
      set_win_elem(win->tree,win->elements);
      win->status &= ~WIN_ICONIFIED;
      calcworksize(win->elements,&win->totsize,&win->worksize,WC_WORK);
      return changewinsize(win,(RECT *)&msg->parm1,svid,1);
      
    default:
      DB_printf("%s: Line %d: srv_wind_set:\r\n"
		"Unknown mode %d",__FILE__,__LINE__,msg->mode);
      return 0;
    };
  }
  else {
    return 0;	
  };
}

/****************************************************************************
 * srv_wind_new                                                             *
 *  Implementation of wind_new().                                           *
 ****************************************************************************/
static WORD    /* 0 if error or 1 if ok.                                    */
srv_wind_new(  /*                                                           */
WORD apid)     /* Application whose windows should be erased.               */
/****************************************************************************/
{
  WINLIST *     wl;

  wl = win_vis;
  
  while(wl) {
    if((wl->win->owner == apid) && !(wl->win->status & WIN_DESKTOP)) {
      C_WIND_DELETE c_wind_delete;
      R_WIND_DELETE r_wind_delete;

      c_wind_delete.id = wl->win->id;
      srv_wind_delete(&c_wind_delete, &r_wind_delete);
      
      wl = win_vis;
    }
    else {
      wl = wl->next;
    };
  };
  
  wl = win_list;
  
  while(wl) {
    if(wl->win->owner == apid) {
      C_WIND_DELETE c_wind_delete;
      R_WIND_DELETE r_wind_delete;
      
      c_wind_delete.id = wl->win->id;
      srv_wind_delete(&c_wind_delete, &r_wind_delete);
      
      wl = win_list;
    }
    else {	
      wl = wl->next;
    };
  };
  
  return 1;
}

/****************************************************************************
 * srv_put_event                                                            *
 *  Put event message in event pipe                                         *
 ****************************************************************************/
WORD              /*  0 if ok or -1                                         */
srv_put_event(    /*                                                        */
WORD    apid,     /* Id of application that is to receive a message.        */
C_PUT_EVENT *msg)
/****************************************************************************/
{
	AP_INFO *ai;
	WORD    retval = 0;

	ai = internal_appl_info(apid);			

	if(ai) {
		LONG fd = Fopen(ai->eventname,O_WRONLY);

		if(fd >= 0) {
			Fwrite((WORD)fd, msg->length, msg->er);
			Fclose((WORD)fd);
		}			
		else {
			DB_printf("%s: Line %d:\r\n"
				"Couldn't open event pipe %s. apid=%d error code=%ld\r\n",
				__FILE__,__LINE__,ai->eventname,ai->id,fd);

			retval = -1;
		};
	}
	else
	{
		DB_printf(
			"%s: Line %d: Couldn't find description of application %d\r\n",
			__FILE__,__LINE__,apid);

		retval = -1;
	};

	return retval;
}


/*
** Description
** This is the server itself
**
** 1998-09-26 CG
*/
static
WORD
server (LONG arg) {
  WORD          work_in[] = {1,7,1,1,1,1,1,1,1,1,2};
  WORD          work_out[57];
  RECT          r;
  C_WIND_CREATE c_wind_create;
  R_WIND_CREATE r_wind_create;
  C_WIND_GET    c_wind_get;
  R_WIND_GET    r_wind_get;
  C_WIND_OPEN   c_wind_open;
  R_WIND_OPEN   r_wind_open;

  /* These variables are passed from clients */
  WORD          client_pid;
  WORD          apid;
  WORD          call;
  C_SRV         par;
  R_SRV         ret;
  void *        handle;

  WORD          code;

  WINSTRUCT *   ws;
  WINLIST *     wl;
  
  /* Stop warnings from compiler about unused parameters */
  NOT_USED(arg);
  
  /* Initialize message handling */
  DB_printf ("srv.c: Initializing message handling");
  Srv_open ();
  DB_printf ("srv.c: Initialized message handling");

  svid = globals.vid;
  DB_printf ("srv.c: Opening vdi workstation");
  Vdi_v_opnwk(work_in, &svid, work_out);
  DB_printf ("srv.c: Opened vdi workstation: svdi = %d", svid);

  /* Initialize desktop background */
  ws = winalloc();
  ws->status = WIN_OPEN | WIN_DESKTOP | WIN_UNTOPPABLE;
  ws->owner = 0;
  
  
  ws->tree = NULL;
  
  ws->worksize.x = globals.screen.x;
  ws->worksize.y = globals.screen.y + globals.clheight + 3;
  ws->worksize.width = globals.screen.width;
  ws->worksize.height = globals.screen.height - globals.clheight - 3;
  
  ws->maxsize = ws->totsize = ws->worksize;
  
  wl = (WINLIST *)Mxalloc(sizeof(WINLIST),GLOBALMEM);
  
  wl->win = ws;
  
  wl->next = win_vis;
  win_vis = wl;

  wl->win->rlist = (RLIST *)Mxalloc(sizeof(RLIST),GLOBALMEM);
  wl->win->rlist->r.x = 0 /*globals.screen.x*/;
  wl->win->rlist->r.y = 0 /*globals.screen.y*/;
  wl->win->rlist->r.width = 1024 /*globals.screen.width*/;
  wl->win->rlist->r.height = 768 /*globals.screen.height*/;
  wl->win->rlist->next = NULL;

  c_wind_get.handle = 0;
  c_wind_get.mode = WF_FULLXYWH;
  srv_wind_get (&c_wind_get, &r_wind_get);
  r = *((RECT *)&r_wind_get.parm1);

  r.height = r.y;
  r.y = 0;
  
  c_wind_create.common.apid = 0;
  c_wind_create.elements = 0;
  c_wind_create.maxsize = r;
  c_wind_create.status = WIN_MENU;

  srv_wind_create (&c_wind_create, &r_wind_create);
  mglob.winbar = r_wind_create.common.retval;

  c_wind_open.id = mglob.winbar;
  c_wind_open.size = r;
  srv_wind_open (&c_wind_open,
                 &r_wind_open);
  
  while(1) {
    
#ifdef SRV_DEBUG
    DB_printf("///");
#endif

    /* Wait for another call from a client */
    /*
    DB_printf ("srv.c: Waiting for message from client");
    */
    handle = Srv_get (&par, sizeof (C_SRV));
    /*
    DB_printf ("srv.c: Got message from client");
    */
#ifdef SRV_DEBUG
    DB_printf("Call: %d pid: %d",msg.cr.call,msg.pid);
#endif		
    
    switch (par.common.call) {
    case SRV_SHAKE:
      DB_printf ("I'm fine application %d (process %d)!", apid, client_pid);
      DB_printf ("How are you doing yourself?");
      Srv_reply (handle, &par, 0);
      break;
      
    case SRV_SHUTDOWN:
      DB_printf("OK! Nice chatting with you! Bye!");
      
      Srv_reply (handle, &par, 0);
      Vdi_v_clswk(svid);
      return 0;
      
    case SRV_APPL_CONTROL:
      code = srv_appl_control (&par.appl_control);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_APPL_EXIT:
      srv_appl_exit (&par.appl_exit, &ret.appl_exit);
      
      Srv_reply (handle, &ret, sizeof (R_APPL_EXIT));
      break;
      
    case SRV_APPL_FIND:
      code = srv_appl_find (&par.appl_find);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_APPL_INIT:
      srv_appl_init (&par.appl_init, &ret.appl_init);
      
      Srv_reply (handle, &ret, sizeof (R_APPL_INIT));
      break;
      
    case SRV_APPL_SEARCH:
      code = srv_appl_search (apid, &par.appl_search);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_APPL_WRITE:
      srv_appl_write (&par.appl_write, &ret.appl_write);
      
      Srv_reply (handle, &ret, sizeof (R_APPL_WRITE));
      break;
      
    case SRV_CLICK_OWNER:
      code = 
	srv_click_owner();
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_EVNT_MULTI:
      srv_evnt_multi (&par.evnt_multi, &ret.evnt_multi);
      
      Srv_reply (handle, &ret, sizeof (R_EVNT_MULTI));
      break;
      
    case SRV_GET_APPL_INFO:
      code = 
	srv_get_appl_info (apid, &par.get_appl_info);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_GET_TOP_MENU:
      code = 
	srv_get_top_menu (&par.get_top_menu);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_GET_VDI_ID :
      srv_get_vdi_id (&par.appl_exit, &ret.get_vdi_id);
      
      Srv_reply (handle, &ret, sizeof (R_GET_VDI_ID));
      break;

    case SRV_GET_WM_INFO:
      code = 
	srv_get_wm_info (&par.get_wm_info);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_MENU_BAR:
      code = 
	srv_menu_bar (&par.menu_bar);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_MENU_REGISTER:
      code = 
	srv_menu_register (apid, &par.menu_register);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_PUT_EVENT:
      code = 
	srv_put_event (apid, &par.put_event);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_SHEL_ENVRN:
      code = 
	srv_shel_envrn (&par.shel_envrn);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_SHEL_WRITE:
      code = srv_shel_write(apid, &par.shel_write);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_CHANGE:
      code = 
	srv_wind_change(&par.wind_change);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_CLOSE:
      code = 
	srv_wind_close(&par.wind_close);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_CREATE:
      srv_wind_create (&par.wind_create, &ret.wind_create);
      
      Srv_reply (handle, &ret, sizeof (R_WIND_CREATE));
      break;
      
    case SRV_WIND_DELETE:
      srv_wind_delete (&par.wind_delete,
                       &ret.wind_delete);
      
      Srv_reply (handle, &ret, sizeof (R_WIND_DELETE));
      break;
      
    case SRV_WIND_DRAW:
      code = 
	srv_wind_draw (&par.wind_draw);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_FIND:
      code = 
	srv_wind_find(&par.wind_find);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_GET:
      srv_wind_get (&par.wind_get,
                    &ret.wind_get);
      
      Srv_reply (handle, &ret, sizeof (R_WIND_GET));
      break;
      
    case SRV_WIND_NEW:
      code = 
	srv_wind_new(&par.wind_new);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_OPEN:
      DB_printf ("srv.c: server: srv_wind_open will be called");
      srv_wind_open (&par.wind_open, &ret.wind_open);
      
      Srv_reply (handle, &ret, sizeof (R_WIND_OPEN));
      DB_printf ("srv.c: server: srv_wind_open reply sent");
      break;
      
    case SRV_WIND_SET:
      code = 
	srv_wind_set(apid, &par.wind_set);
      
      Srv_reply (handle, &par, code);
      break;
      
    case SRV_WIND_UPDATE:
      srv_wind_update (&par.wind_update, &ret.wind_update);
      
      Srv_reply (handle, &ret, sizeof (R_WIND_UPDATE));
      break;
      
    default:
      DB_printf("%s: Line %d:\r\n"
		"Unknown call %d to server!",
		__FILE__, __LINE__, call);
      Srv_reply (handle, &par, -1);
    };
  };
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

/****************************************************************************
 * Server_init_module                                                       *
 *  Initialize server module.                                               *
 ****************************************************************************/
void                   /*                                                   */
Srv_init_module(void)  /*                                                   */
/****************************************************************************/
{
  WORD i;
  
  WINLIST	*wl;
  
  WINSTRUCT	*ws;
  
  for(i = 0; i < MAX_NUM_APPS; i++) {
    apps[i].id = -1;
    apps[i].eventpipe = -1;
    apps[i].msgpipe = -1;
    apps[i].rshdr = NULL;
    apps[i].deskbg = NULL;
    apps[i].menu = NULL;
    apps[i].deskmenu = -1;
  };
  
  if(globals.num_pens < 16) {
    for(i = 0; i <= W_SMALLER; i++) {
      untop_colour[i].pattern = 0;
      untop_colour[i].fillc = WHITE;
      top_colour[i].pattern = 0;
      top_colour[i].fillc = WHITE;
    }
  }
  
  globals.srvpid = (WORD)Misc_fork(server,0,"oAESsrv");
  
  /*
    Srv_shake();
    */
}

/****************************************************************************
 * Srv_exit_module                                                          *
 *  Shutdown server module.                                                 *
 ****************************************************************************/
void                   /*                                                   */
Srv_exit_module(void)  /*                                                   */
/****************************************************************************/
{
  DB_printf("Killing off alive processes");
  
  /* Kill all AES processes */
  while(ap_pri) {
    C_APPL_CONTROL msg;

    msg.apid = ap_pri->ai->id;
    msg.mode = APC_KILL;

    srv_appl_control(&msg);
  };
  
  DB_printf("Killed all processes\r\n");

  DB_printf("Server we have to quit now! Bye!");
  
  /*  Srv_put (0, SRV_SHUTDOWN, NULL);*/
}