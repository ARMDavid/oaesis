#include <basepage.h>
#include <fcntl.h>
#include <ioctl.h>
#include <limits.h>
#include <mintbind.h>
#include <osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gemdefs.h"
#include "lxgemdos.h"
#include "misc.h"
#include "srv.h"
#include "types.h"

#define TOSAPP 0
#define GEMAPP 1

WORD Shel_do_read(BYTE *name,BYTE *tail) {
	BYTE pname[30];
	_DTA *olddta,newdta;
	WORD retval;
	
	olddta = Fgetdta();
	Fsetdta(&newdta);
	
	sprintf(pname,"u:\\proc\\*.%03d",Pgetpid());
	if(Fsfirst(pname,0) == 0) {
		LONG fd;
		
		sprintf(pname,"u:\\proc\\%s",newdta.dta_name);
		
		if((fd = Fopen(pname,O_RDONLY)) >= 0) {
			struct __ploadinfo li;
			
			li.fnamelen = 128;
			li.cmdlin = tail;
			li.fname = name;
			
			Fcntl((WORD)fd,&li,PLOADINFO);
			Fclose((WORD)fd);
			retval = 1;
		}
		else {
			retval = 0;
		};
		
	}
	else {
		retval = 0;
	};

	Fsetdta(olddta);
	
	return retval;
}

/*0x0078 shel_read*/

void	Shel_read(AES_PB *apb) {
	apb->int_out[0] = Shel_do_read((BYTE *)apb->addr_in[0],(BYTE *)apb->addr_in[1]);
}


/****************************************************************************
 * Shel_write                                                               *
 *  0x0079 shel_write().                                                    *
 ****************************************************************************/
void              /*                                                        */
Shel_write(       /*                                                        */
AES_PB *apb)      /* AES parameter block.                                   */
/****************************************************************************/
{
	apb->int_out[0] = 
	Srv_shel_write(apb->global->apid,apb->int_in[0], apb->int_in[1], apb->int_in[2],
		(BYTE *)apb->addr_in[0], (BYTE *)apb->addr_in[1]);
		
	if(apb->int_out[0] == 0) {
		DB_printf("shel_write(0x%04x,0x%04x,0x%04x,\r\n"
																"%s,\r\n"
																"%s)", 
		        apb->int_in[0],apb->int_in[1],apb->int_in[2],
		        (BYTE *)apb->addr_in[0],(BYTE *)apb->addr_in[1]+1);
	}
}

/****************************************************************************
 * Shel_do_find                                                             *
 *  Implementation of shel_find().                                          *
 ****************************************************************************/
WORD              /* 0 if the file was not found or 1.                      */
Shel_do_find(     /*                                                        */
BYTE *buf)        /* Buffer where the filename is given and full path       */
                  /* returned.                                              */
/****************************************************************************/
{
	BYTE name[128];
 	BYTE tail[128], *p, *q;
	XATTR xa;
	
	strcpy(name,buf);
	
	/* we start by checking if the file is in our path */
	
	if(Fxattr(0,buf,&xa) == 0)  {
	/* check if we were passed an absolute path (rather simplistic)
	 * if this was the case, then _don't_ concat name on the path */
	 	if (!((name[0] == '\\') ||
	 	     (name[0] && (name[ 1] == ':')))) {
			Dgetpath(buf,0);
			strcat(buf, "\\");
			strcat(buf, name);
		};
		
		return 1;
	}

	strlwr(buf);

	if(Fxattr(0,buf,&xa) == 0)  {
	/* check if we were passed an absolute path (rather simplistic)
	 * if this was the case, then _don't_ concat name on the path */
	 	if (!((name[0] == '\\') ||
	 	     (name[0] && (name[ 1] == ':')))) {
			Dgetpath(buf,0);
			strcat(buf, "\\");
			strcat(buf, name);
		};
		
		return 1;
	}

	Shel_do_read(buf, tail);
	p = strrchr( buf, '\\');
	if(p) {
		strcpy( p+1, name);
		if(Fxattr(0, buf, &xa) == 0) 
			return 1;
	
		q = strrchr( name, '\\');

		if(q) {
			strcpy(p, q);
			if(Fxattr(0, buf, &xa) == 0) {
				return 1;
			}
			else {
				(void)0; /* <--search the PATH enviroment */
			}
		}
	}
		
	return 0;
}

/****************************************************************************
 * Shel_find                                                                *
 *  0x007c shel_find().                                                     *
 ****************************************************************************/
void              /*                                                        */
Shel_find(        /*                                                        */
AES_PB *apb)      /* AES parameter block.                                   */
/****************************************************************************/
{
	apb->int_out[0] = Shel_do_find((BYTE *)apb->addr_in[0]);
}

/****************************************************************************
 * Shel_envrn                                                               *
 *  0x007d shel_envrn().                                                    *
 ****************************************************************************/
void              /*                                                        */
Shel_envrn(       /*                                                        */
AES_PB *apb)      /* AES parameter block.                                   */
/****************************************************************************/
{
	apb->int_out[0] = Srv_shel_envrn((BYTE **)apb->addr_in[0],
																		(BYTE *)apb->addr_in[1]);
}
