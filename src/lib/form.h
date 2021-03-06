#ifndef	__FORM__
#define	__FORM__

#include	"types.h"

/*
** Description
** Implementation of form_button.
**
** 1998-12-19 CG
*/
WORD
Form_do_button (WORD     apid,
                OBJECT * tree,
                WORD     obj,
                WORD     clicks,
                WORD *   newobj);

void
Form_do_center (WORD     apid,
                OBJECT * tree,
                RECT *   clip);

WORD	Form_do_dial(WORD apid,WORD mode,RECT *r1,RECT *r2);

/*
** Description
** Implementation of form_do.
**
** 1998-12-19 CG
*/
WORD
Form_do_do (WORD     apid,
            OBJECT * tree,
            WORD editobj);

/*
** Description
** Display pre-defined error alert box.
**
** 1998-12-19 CG
*/
WORD
Form_do_error (WORD   apid,
               WORD   error);

/****************************************************************************
 *  Form_do_keybd                                                           *
 *   Process key input to form.                                             *
 ****************************************************************************/
WORD              /* 0 if an exit object was selected, or 1.                */
Form_do_keybd(    /*                                                        */
WORD     apid,
OBJECT * tree,    /* Resource tree of form.                                 */
WORD     obj,     /* Object with edit focus (0 => none).                    */
WORD     kc,      /* Keypress to process.                                   */
WORD   * newobj,  /* New object with edit focus.                            */
WORD   * keyout); /* Keypress that couldn't be processed.                   */
/****************************************************************************/


void	Form_do(AES_PB *apb);		/*0x0032*/
void	Form_dial(AES_PB *apb);		/*0x0033*/
void	Form_alert(AES_PB *apb);	/*0x0034*/
void	Form_error(AES_PB *apb);	/*0x0035*/
void	Form_center(AES_PB *apb);	/*0x0036*/

/****************************************************************************
 *  Form_keybd                                                              *
 *   0x0037 form_keybd()                                                    *
 ****************************************************************************/
void              /*                                                        */
Form_keybd(       /*                                                        */
AES_PB *apb);     /* AES parameter block.                                   */
/****************************************************************************/

void	Form_button(AES_PB *apb);	/*0x0038*/

#endif
