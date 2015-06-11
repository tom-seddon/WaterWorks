/*

	Water Works
	===========

*/
#include <process.h>
#include <windows.h>
#include <stdio.h>
#include <ddraw.h>
#include <crtdbg.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "dx.h"
#include "debug.h"
#include "resource.h"
#include "strings.h"

// DEBUG_SCROLLING: information during WM_[VH]SCROLL processing
//#define DEBUG_SCROLLING
// DEBUG_PAINT_WINDOW: Print information if blt from back surface to window interior fails
#define DEBUG_PAINT_WINDOW

// Debug window_size() function (informative messages when WM_SIZING)
//#define DEBUG_WINDOW_SIZE

// Debug do_window_stuff()
//#define DEBUG_DO_WINDOW_STUFF

#define NUM_DROPLETS (100000)
//#define NUM_DROPLETS (15129)

/* For now, this prevents all sorts of problems. */
#define MIN_AREA_WIDTH (640)
#define MIN_AREA_HEIGHT (400)

/* if #defined the message loop will eat all messages before doing anything else. otherwise, it will
   eat them one at a time. Without GREEDY_MESSAGE_LOOP seems to work a little better. */
#define GREEDY_MESSAGE_LOOP

#define CLASS_NAME "wclass_water"

/* Adds table. See main loop for details. */
typedef struct {
	int area_width;						/* width of "play" area */
	int area_height;					/* heighto f "play" area */
	int bucket_size;					/* bucket width and height */
	int bucket_neck_size;				/* bucket's neck size */
	int paused;							/* whether water is paused or not */
	int asm;							/* whether asm lop should be used or not */
	unsigned update_diff;				/* time (in 1000ths of a second) between updates */

	/* DirectDraw specifics */
	int ddraw_valid;					/* whether current ddraw settings valid or not */
	int ddraw_bad;						/* whether current valid ddraw settings are bad */
	IDirectDrawSurface2 *primary;		/* surface -- primary (desktop) surface */
	IDirectDrawSurface2 *land;			/* surface -- landscape */
	int land_changed;
	IDirectDrawSurface2 *back;			/* surface -- back (offscreen) surface */
	IDirectDrawClipper *clipper;
	DDPIXELFORMAT pf;					/* pixel format for primary surface */
	int dd_bpp;							/* display bytes per pixel */
	COLORREF *land_backup;

	/* Window configuration */
	int window_valid;					/* window size valid or not */
	int view_x;
	int view_y;							/* X+Y position of top left of view */
	int view_width;
	int view_height;					/* W+H of window's client area, excluding decals and scroll bars*/
	int w_mul,h_mul;					/* width and height zoom factors */
	int popup_menu;
	int include_bucket;					/* include bucket in view or not */
	int use_wm_paint;					/* Don't even ask */
	int hscroll,vscroll;				/* Whether window sports horizontal and/or vertical scrollbar(s) */
	int stretch_image;
	HMENU menu;

	/* These are for or generally accessed by the window procedure */
	int held;							/* LMB held */
	int outside;						/* mouse outside (not used) */
	DWORD lastpoint;					/* last point, in WM_MOUSEMOVE coordinates */
	int brush_size;						/* brush size in pixels. Erm, sorry!! Logical device units. */
	int brush_col;						/* brush colour. index into brush_Cols[] etc. above. */
	int new_num_drops;					/* new number of droplets, to take effect ASAP. If 0, no request for new
										   droplets has been made. */
	/* Informative messages */
	char *msg;							/* the text to display */
	DWORD msg_time;						/* the time at which it should disappear */

	/* Timing */
	int no_catchup;						/* If true, don't attempt to catch up if it looks as if
										   updates are lagging behind. */

	/* Droplet data */
	unsigned pitch;					/* pitch (distance between successive lines) of droplet data */
	unsigned num_drops;				/* number of droplets*/
	unsigned *drops;				/* droplet data, 2 unsigneds per droplet. */
	int droplets_bpp;				/* format of droplet data: 1 (8bpp), 2 (16bpp), 4 (32bpp) */
}stuff_t;

/*
	Global variables
*/

/* brush_cols -- map brush type to colour as displayed on screen */
static COLORREF brush_cols[3]={RGB(200,200,0),RGB(0,255,0),RGB(0,0,0)};
/* Brush_col_names -- map brush type to entry in string table giving natural language name */
static unsigned brush_col_names[3]={IDS_BROWN_NAME,IDS_GREEN_NAME,IDS_BLACK_NAME};
/* brush_accels -- map brush type to accelerator used to select it */
static unsigned brush_accels[3]={IDA_BRUSHYELLOW,IDA_BRUSHGREEN,IDA_BRUSHBLACK};

/* brush types */
enum {
	YELLOW_BRUSH_COLOUR=0,GREEN_BRUSH_COLOUR=1,BLACK_BRUSH_COLOUR=2,
};

/* Random table. Saves calling rand() */
/* Size of indices, in bits */
#define RND_TBL_BITS (13)
/* Size of random table, in entries */
#define RND_TBL_SIZE (1<<RND_TBL_BITS)
/* Mask random table index with this value to clamp to valid range (with wrap) */
#define RND_TBL_IDX_MASK ((1<<RND_TBL_BITS)-1)
/* Random table */
static unsigned dir_tbl[RND_TBL_SIZE];

/* Map droplet type to droplet colour (as in: value to be written to screen memory) */
static DWORD droplet_colours[2];
/* Map droplet type to direction (specified as offset in bytes) when on green surface */
static int droplet_dirs[2];

/*
	Functions
*/

/* Draw and update droplets, 2 bytes/pixel */
static void draw_all_droplets16(int mask,void *vstuff,DDSURFACEDESC *ds);
static void update_all_droplets16(int no_era,void *vstuff,DDSURFACEDESC *ds);
/* Draw and update droplets, 4 bytes/pixel */
static void draw_all_droplets32(int mask,void *vstuff,DDSURFACEDESC *ds);
static void update_all_droplets32(int no_era,void *vstuff,DDSURFACEDESC *ds);

typedef struct funcs_t {
	unsigned bpp;
	void (*draw_all_droplets)(int,void *,DDSURFACEDESC *);
	void (*update_all_droplets)(int,void *,DDSURFACEDESC *);
	void (*draw_all_droplets_asm)(int,void *,DDSURFACEDESC *);
	void (*update_all_droplets_asm)(int,void *,DDSURFACEDESC *);
}funcs_t;

static funcs_t funcsarr[]={
	{16,draw_all_droplets16,update_all_droplets16,0,0},
	{32,draw_all_droplets32,update_all_droplets32,0,0},
	{0}
};

/* Drawing and update routines for current colour depth, chosen from the above selection */
static void (*draw_all_droplets)(int,void *,DDSURFACEDESC *)=0;
static void (*update_all_droplets)(int,void *,DDSURFACEDESC *)=0;

/* Save and restore landscape */
static void save_land(stuff_t *);
static int restore_land(stuff_t *);

/* Set number of droplets */
static void set_drops(stuff_t *stuff,unsigned num_drops);
/* Draw landscape border on landscape surface */
static void do_land_border(stuff_t *stuff);
/* Do WM_PAINT stuff */
static void paint_window(HWND h_wnd,stuff_t *stuff);

/* See do_window_stuff for more details. */
#define RW_NOSETWINDOWPOS (1)
#define RW_NOMESSAGES (2)
static void reset_window(stuff_t *stuff,HWND h_wnd,unsigned flags);
static void sizing_window(stuff_t *stuff,HWND h_wnd,RECT *rect,int side);

#ifdef _DEBUG
static void log_droplets(stuff_t *p,char *file,char *mode) {
	FILE *h;

	h=fopen(file,mode);
	if(h) {
		unsigned i;

		fprintf(h,"There are %u droplets.\n",p->num_drops);
		fprintf(h,"Pitch is %u.\n",p->pitch);
		for(i=0;i<p->num_drops;i++) {
			fprintf(h,"#%u: dw1=0x%08X dw2=0x%08X (X=%u, Y=%u)\n",
				i,p->drops[i*2],p->drops[i*2+1],p->drops[i*2]%p->pitch,p->drops[i*2]/p->pitch);
		}
		fclose(h);
	}
}
#endif

/*
	set_message

	Sets the message to be displayed in the window. The message will
	disappear after one second. The format string is specified by resource
	id, and any parameters it requires are passed afterwards.

	p -> stuff pointer
	msgid -> id of string in string table
	... -> parameters for format string
*/

static void set_message(stuff_t *p,unsigned msgid,...) {
	static char buf[100];
	va_list v;
	char *fmt;

	if(p->msg) {
		free(p->msg);
		p->msg=0;
	}
	fmt=get_string(msgid);
#ifdef _DEBUG
	if(!fmt) {
		p->msg=malloc(15);
		_snprintf(p->msg,15,"<<%u?>>",msgid);
	} else
#endif
	{
		va_start(v,msgid);
		_vsnprintf(buf,sizeof(buf),fmt,v);
		va_end(v);
		p->msg=_strdup(buf);
	}
	p->msg_time=GetTickCount()+1000;			/* Displayed for 1 second */
}

/*
set_brushcolour

  Sets the brush colour and displays suitable message.

  ncolour -> new colour of brush (MAKECOL)
*/

static void set_brushcolour(stuff_t *stuff,int ncolour) {
	int i;

	for(i=0;i<3;i++) {
		CheckMenuItem(stuff->menu,brush_accels[i],MF_BYCOMMAND|MF_UNCHECKED);
	}
	if(ncolour>=0&&ncolour<3) {
		CheckMenuItem(stuff->menu,brush_accels[ncolour],MF_BYCOMMAND|MF_CHECKED);
		stuff->brush_col=ncolour;
		set_message(stuff,IDS_BRUSH_COL_MSG,get_string(brush_col_names[ncolour]));
	}
}

/*
set_zoom

  Sets zoom factor, displays suitable message, checks appropriate menu item.

  nxz -> new X zoom
  nyz -> new Y zoom
  */
static void set_zoom(stuff_t *stuff,int nxz,int nyz) {
	static unsigned ids[3]={IDA_ZOOM1,IDA_ZOOM2,IDA_ZOOM3};
	int i;

	stuff->w_mul=nxz;
	stuff->h_mul=nyz;
	stuff->window_valid=0;
	set_message(stuff,IDS_ZOOM_MSG,stuff->w_mul,stuff->h_mul);
	for(i=0;i<3;i++) {
		CheckMenuItem(stuff->menu,ids[i],MF_BYCOMMAND|MF_UNCHECKED);
	}
	if(nxz==nyz&&nxz>=1&&nxz<=3) {
		CheckMenuItem(stuff->menu,ids[nxz-1],MF_BYCOMMAND|MF_CHECKED);
	}
}

/*
set_brushsize

  Sets brush size, displays tuiable message, checks appropriate menu item.

  nsz -> new brush size (pixels)
*/
static void set_brushsize(stuff_t *stuff,int nsz) {
	static unsigned ids[5]={IDA_BRUSH1,IDA_BRUSH2,IDA_BRUSH3,IDA_BRUSH4,IDA_BRUSH5};
	int i;

	stuff->brush_size=nsz;
	set_message(stuff,IDS_BRUSH_SIZE_MSG,stuff->brush_size);
	for(i=0;i<5;i++) {
		CheckMenuItem(stuff->menu,ids[i],MF_BYCOMMAND|MF_UNCHECKED);
	}
	if(nsz>=1&&nsz<=5) {
		CheckMenuItem(stuff->menu,ids[nsz-1],MF_BYCOMMAND|MF_CHECKED);
	}

}

/* 
mouse_trans

  Translates client coordinates into landscape coordinates.

  h -> handle of window clicked in
  x -> pointer to client X coordinates
  y -> pointer to client Y coordinate

  Return: *x and *y are updated.
*/
static void mouse_trans(stuff_t *stuff,HWND h,int *x,int *y) {
	RECT r;
	float xamt,yamt;

	GetClientRect(h,&r);
	xamt=(*x/(float)r.right);
	yamt=(*y/(float)r.bottom);
	*x=stuff->view_x+(int)(xamt*stuff->view_width/stuff->w_mul);
	*y=(stuff->view_y-stuff->bucket_size)+(int)(yamt*stuff->view_height/stuff->h_mul);
}

/*
	Crap debug thing on debugger.
*/
#ifdef _DEBUG
static void debug_status(stuff_t *p) {
	struct tm *newtime; time_t clk;

	time(&clk); newtime=localtime(&clk);
	dprintf("Debug status at %s:\n\n",asctime(newtime));
	dprintf("Area:\tsize=%d x %d\n",p->area_width,p->area_height);
	dprintf("view:\tsize=%d x %d at (%d,%d), ",p->view_width,p->view_height,p->view_x,p->view_y);
//	dprintf("view:\tat (%d,%d), ",p->view_x,p->view_y);
	dprintf("zoom factor=%d x %d\n",p->w_mul,p->h_mul);
	dprintf("Flags:\tusing WM_PAINT=%s; ",p->use_wm_paint?"yes":"no");
	dprintf("no_catchup=%s; ",p->no_catchup?"yes":"no");
	dprintf("paused=%s\n",p->paused?"yes":"no");
	dprintf("\nThat's everything for %s\n",asctime(newtime));
}
#endif

/*
cbox_add_item

  Adds a string (referenced by string table resource id) to a combo box.

  cbox -> handle of combo bxo window
  text_id -> string table id of item

  Return: 0-based index of item in combo box, or -1 if it all went horribly
  wrong.
*/
static int cbox_add_item(HWND cbox,unsigned text_id) {
	LRESULT r;

	r=SendMessage(cbox,CB_ADDSTRING,0,(LPARAM)get_string(text_id));
	if(r==CB_ERR||r==CB_ERRSPACE) {
		return -1;
	}
	/* Item data is string resource id */
	SendMessage(cbox,CB_SETITEMDATA,r,text_id);
	return r;
}

/*
unsigned_val_of

  Gets the unsigned value of a string.

  str -> points to string
  result -> points to variable to receive the result


  Return: if str is a valid number, return non-0. Else, return 0.
	If str is valid, and result isn't NULL, *result is set to
	value of str; if str isn't valid, *result is unchanged.

  result may be NULL, if you only care whether str is valid or not.
*/
static int unsigned_val_of(const char *str,unsigned *result) {
	unsigned long r;
	char *ep;

	r=strtoul(str,&ep,0);
	if(*ep&&!isspace(*ep)) {
		/* This is an error. */
		return 0;
	}
	if(result) {
		*result=r;
	}
	return 1;
}

static BOOL CALLBACK resize_dlgproc(HWND h,UINT msg,WPARAM w,LPARAM l) {
	static stuff_t *stuff;

	switch(msg) {
	case WM_INITDIALOG:
		stuff=(stuff_t *)l;
		{
			/* Initialise dialog appearance */
			char numtmp[20]; HWND cb; int selidx;

			/* Width text box */
			_snprintf(numtmp,sizeof(numtmp),"%d",stuff->area_width);
			SendMessage(GetDlgItem(h,IDC_NEW_WIDTH),WM_SETTEXT,0,(LPARAM)numtmp);
			/* Height text box */
			_snprintf(numtmp,sizeof(numtmp),"%d",stuff->area_height);
			SendMessage(GetDlgItem(h,IDC_NEW_HEIGHT),WM_SETTEXT,0,(LPARAM)numtmp);
			/* Add items to list box */
			cb=GetDlgItem(h,IDC_CURRENT_CONTENTS);
			selidx=cbox_add_item(cb,IDS_CONTENTS_DISCARD);
			cbox_add_item(cb,IDS_CONTENTS_PRESERVE);
			cbox_add_item(cb,IDS_CONTENTS_CENTRE);
			cbox_add_item(cb,IDS_CONTENTS_RESIZE);
			SendMessage(cb,CB_SETCURSEL,selidx,0);
		}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(w)) {
		case IDOK:
			{
				char w_text[20],h_text[20];
				unsigned new_width,new_height;
				int wf,hf,r;

				SendMessage(GetDlgItem(h,IDC_NEW_WIDTH),WM_GETTEXT,sizeof(w_text),(LPARAM)w_text);
				SendMessage(GetDlgItem(h,IDC_NEW_HEIGHT),WM_GETTEXT,sizeof(h_text),(LPARAM)h_text);
				wf=unsigned_val_of(w_text,&new_width);
				hf=unsigned_val_of(h_text,&new_height);
				if(!wf||!hf) {
					r=MessageBox(h,get_string(IDS_RESIZE_INVALID),get_string(IDS_RESIZE_INVALID_TITLE),MB_RETRYCANCEL|MB_ICONINFORMATION);
					if(r==IDCANCEL) {
						EndDialog(h,0);		/* equivalent to Cancel */
					}
				} else if(new_width<MIN_AREA_WIDTH||new_height<MIN_AREA_HEIGHT) {
					char txt[250];

					_snprintf(txt,sizeof(txt),get_string(IDS_RESIZE_TOOSMALL),MIN_AREA_WIDTH,MIN_AREA_HEIGHT);
					r=MessageBox(h,txt,get_string(IDS_RESIZE_INVALID_TITLE),MB_RETRYCANCEL|MB_ICONINFORMATION);
					if(r==IDCANCEL) {
						EndDialog(h,0);		/* Cancel */
					}
				} else {
					dprintf("resize dlg: OK: new_width=%u; new_height=%u\n",new_width,new_height);
					stuff->area_width=(signed)new_width;
					stuff->area_height=(signed)new_height;
					EndDialog(h,1);		/* non-0 is OK'ed */
				}
			}
			return TRUE;
		case IDCANCEL:
			EndDialog(h,0);		/* 0 indicates Cancel'ed */
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static LRESULT CALLBACK main_wndproc(HWND h,UINT msg,WPARAM w,LPARAM l) {
	/* This is the whole point of stuff_t. */
	stuff_t *p=(stuff_t *)GetWindowLong(h,GWL_USERDATA);

	switch(msg) {
	case WM_SIZING:
		sizing_window(p,h,(RECT *)l,w);
		p->use_wm_paint=1;		/* may get set to 0 thanks to SB_ENDSCROLL messages */
		return TRUE;
	case WM_ENTERMENULOOP:
//		dprintf("WM_ENTERMENULOOP:\n");
		p->use_wm_paint=1;
		p->no_catchup=1;
		return 0;
	case WM_ENTERSIZEMOVE:
		p->use_wm_paint=1;
		p->no_catchup=1;
		return 0;
	case WM_EXITSIZEMOVE:
		p->use_wm_paint=0;
		return 0;
	case WM_VSCROLL:
	case WM_HSCROLL:
		{
			char *name;
			int pos,old_pos;
			int which=(msg==WM_VSCROLL)?SB_VERT:SB_HORZ;
			SCROLLINFO si;

#ifdef DEBUG_SCROLLING
			dprintf("%s: ",msg==WM_VSCROLL?"WM_VSCROLL":"WM_HSCROLL");
#endif
			si.cbSize=sizeof(si);
			si.fMask=SIF_POS|SIF_PAGE;
			GetScrollInfo(h,which,&si);
			old_pos=si.nPos;
#ifdef DEBUG_SCROLLING
			dprintf("before: pos=%d, pagesize=%d; ",si.nPos,si.nPage);
#endif
			if(LOWORD(w)==SB_ENDSCROLL) {
				p->use_wm_paint=0;
				p->no_catchup=1;
				si.nPos=old_pos;
				name="SB_ENDSCROLL";
			} else {
				p->use_wm_paint=1;
				switch(LOWORD(w)) {
				case SB_THUMBTRACK:
					name="SB_THUMBTRACK";
					si.nPos=(short int)HIWORD(w);
					break;
				case SB_THUMBPOSITION:
					si.nPos=(short int)HIWORD(w);
					name="SB_THUMBPOSITION";
					break;
				case SB_LINEDOWN:
					name="SB_LINEDOWN";
					si.nPos++;
					break;
				case SB_LINEUP:
					name="SB_LINEUP";
					si.nPos--;
					break;
				case SB_PAGEDOWN:
					name="SB_PAGEDOWN";
					si.nPos+=si.nPage;
					break;
				case SB_PAGEUP:
					name="SB_PAGEUP";
					si.nPos-=si.nPage;
					break;
				}
			}
#ifdef DEBUG_SCROLLING
			dprintf("(type was %s) setting to %d; ",name,si.nPos);
#endif
			si.fMask=SIF_POS;
			SetScrollInfo(h,which,&si,TRUE);
			/* Must Get... again to see what SetScrollInfo did with out of range values. */
			GetScrollInfo(h,which,&si);
			pos=si.nPos;
			if(pos!=old_pos) {
				InvalidateRect(h,0,FALSE);
			}
#ifdef DEBUG_SCROLLING
			dprintf("\tafter: new position is %d\n",si.nPos);
#endif
			if(msg==WM_HSCROLL) {
				p->view_x=pos;
#ifdef DEBUG_SCROLLING
				dprintf("WM_HSCROLL: view_x=%d\n",p->view_x);
#endif
			} else if(msg==WM_VSCROLL) {
				p->view_y=pos;
#ifdef DEBUG_SCROLLING
				dprintf("WM_VSCROLL: view_y=%d\n",p->view_y);
#endif
			}
		}
		return 0;
	case WM_PAINT:
		if(p->ddraw_valid&&p->ddraw_bad) {
			PAINTSTRUCT ps;
			RECT r;
			HDC dc;
			char *msg;
			TEXTMETRIC tm;

			GetClientRect(h,&r);
			dc=BeginPaint(h,&ps);

			/* Fill background in black */
			SelectObject(dc,GetStockObject(BLACK_PEN));
			SelectObject(dc,GetStockObject(BLACK_BRUSH));
			Rectangle(dc,r.left,r.top,r.right,r.bottom);

			/* Text */
			SelectObject(dc,GetStockObject(WHITE_PEN));
			SelectObject(dc,GetStockObject(SYSTEM_FONT));
			GetTextMetrics(dc,&tm);

			/* First line */
			msg=get_string(IDS_BAD_DDRAW_1);
			SetTextAlign(dc,TA_TOP|TA_CENTER);
			TextOut(dc,r.right/2,r.bottom/2-(int)(tm.tmHeight*1.25),msg,strlen(msg));

			/* Second line */
			msg=get_string(IDS_BAD_DDRAW_2);
			SetTextAlign(dc,TA_BOTTOM|TA_CENTER);
			TextOut(dc,r.right/2,r.bottom/2+(int)(tm.tmHeight*1.25),msg,strlen(msg));

			EndPaint(h,&ps);
			return 0;
		} else if(p->use_wm_paint&&p->ddraw_valid&&!p->ddraw_bad) {
			PAINTSTRUCT ps;
			HDC dc;
			dc=BeginPaint(h,&ps);
			paint_window(h,p);
			EndPaint(h,&ps);
			return 0;
		}
		break;
	case WM_DISPLAYCHANGE:
		if(p->ddraw_valid&&!p->ddraw_bad) {
			HCURSOR oc;

			//dx_with_lock(p->land,0,0,copy_bits_from);
			oc=SetCursor(LoadCursor(0,IDC_WAIT));
			save_land(p);
			SetCursor(oc);
		}
		p->ddraw_valid=0;
		return 0;
	case WM_CREATE:
		p=((CREATESTRUCT *)l)->lpCreateParams;
		SetWindowLong(h,GWL_USERDATA,(LONG)p);
		p->held=0;
		p->outside=0;
		p->msg=0;
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_RBUTTONDOWN:
		if(p->popup_menu) {
			POINT pt;

			pt.x=LOWORD(l);
			pt.y=HIWORD(l);
			ClientToScreen(h,&pt);
			TrackPopupMenu(p->menu,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,pt.x,pt.y,0,h,0);
			return 0;
		}
		break;
	case WM_LBUTTONDOWN:
		p->held=1;
		p->lastpoint=l;
		return 0;
	case WM_LBUTTONUP:
		SendMessage(h,WM_MOUSEMOVE,w,l);
		p->held=0;
		return 0;
	case WM_MOUSEMOVE:
		if(p->held&&!(w&MK_LBUTTON)) {
			/* left button released outside the window */
			p->held=0;
		} else if(p->held) {
			int oldx,oldy,thisx,thisy;
			HPEN pen;
			HRGN rgn;
			HDC hdc;

			/* Fetch unadulterated coordinates */
			oldx=LOWORD(p->lastpoint);
			oldy=HIWORD(p->lastpoint);
			if(w&MK_CONTROL) {		/* Lock Y axis */
				thisy=oldy;
			} else {
				thisy=HIWORD(l);
			}
			if(w&MK_SHIFT) {		/* Lock X axis */
				thisx=oldx;
			} else {
				thisx=LOWORD(l);
			}
			/* Make last point */
			p->lastpoint=MAKELONG(thisx,thisy);
			/* Modify coordinates, taking into account zoom factor and origin */
			mouse_trans(p,h,&oldx,&oldy);
			mouse_trans(p,h,&thisx,&thisy);
			if((oldx!=thisx||oldy!=thisy)&&SUCCEEDED(IDirectDrawSurface2_GetDC(p->land,&hdc))) {
				rgn=CreateRectRgn(1,1,p->area_width-1,p->area_height-1);
				pen=CreatePen(PS_SOLID,p->brush_size,brush_cols[p->brush_col]);
				SelectObject(hdc,pen);
				SelectClipRgn(hdc,rgn);
				DeleteObject(rgn);
				MoveToEx(hdc,oldx,oldy,0);
				LineTo(hdc,thisx,thisy);
				SelectObject(hdc,GetStockObject(WHITE_PEN));
				IDirectDrawSurface2_ReleaseDC(p->land,hdc);
				DeleteObject(pen);
				p->land_changed=1;
			}
		}
		return 0;
	case WM_COMMAND:
		if(!l) {
			switch(LOWORD(w)) {
/* Why Developer Studio wouldn't accept ID_FILE_NEW, I'm not entirely
   sure. But it wouldn't. Fortunately insulting it to its face did
   the trick. */
//			case ID_FILE_NEW:
			case ID_F__KING_DEVSTUDIO:
				{
					int i;

					p->no_catchup=1;
					i=DialogBoxParam(GetModuleHandle(0),MAKEINTRESOURCE(IDD_RESIZE),h,resize_dlgproc,(LPARAM)p);
					if(i) {
						/* -> area_width and area_height already done. */
						p->ddraw_valid=0;
						p->window_valid=0;
						/* A bit of a kludge, this ensures droplets are reset when the land size changes */
						SendMessage(h,WM_COMMAND,MAKELONG(ID_FILE_RESET,0),0);
					}
				}
				return 0;
			case ID_FILE_CLEAR:
				if(p->land) {
					dx_clear_surface(p->land);
					do_land_border(p);
					p->land_changed=1;
					p->no_catchup=1;
				}
				return 0;
			case ID_FILE_EXIT:
				DestroyWindow(h);
				return 0;
			case ID_FILE_RESET:
				p->new_num_drops=p->num_drops;
				p->no_catchup=1;
				return 0;
			case ID_TOOLS_POPUPMENU:
				p->popup_menu=!p->popup_menu;
				p->window_valid=0;
				return 0;
			case ID_HELP_ABOUT:
				p->use_wm_paint=1;
				MessageBox(h,get_string(IDS_ABOUT_INFO),get_string(IDS_ABOUT_INFO_TITLE),MB_OK|MB_ICONINFORMATION);
				p->use_wm_paint=0;
				p->no_catchup=1;
				return 0;
#ifdef _DEBUG
			case ID_TOOLS_SAVEDROPLETDATA:
				log_droplets(p,get_string(IDS_DROPLETDATAFILE),"wt");
				return 0;
#endif
			case ID_TOOLS_RUN:
				p->paused=0;
				return 0;
			case ID_TOOLS_PAUSE:
				p->paused=1;
				return 0;
			case IDA_ZOOM1:
				set_zoom(p,1,1);
				return 0;
			case IDA_ZOOM2:
				set_zoom(p,2,2);
				return 0;
			case IDA_ZOOM3:
				set_zoom(p,3,3);
				return 0;
			case IDA_BRUSH1:
				set_brushsize(p,1);
				return 0;
			case IDA_BRUSH2:
				set_brushsize(p,2);
				return 0;
			case IDA_BRUSH3:
				set_brushsize(p,3);
				return 0;
			case IDA_BRUSH4:
				set_brushsize(p,4);
				return 0;
			case IDA_BRUSH5:
				set_brushsize(p,5);
				return 0;
			case IDA_TOGGLEBUCKET:
				p->include_bucket=!p->include_bucket;
				p->window_valid=0;
				set_message(p,p->include_bucket?IDS_BUCKET_INCLUDED:IDS_BUCKET_EXCLUDED);
				return 0;
			case IDA_BRUSHBLACK:
				set_brushcolour(p,BLACK_BRUSH_COLOUR);
				return 0;
			case IDA_BRUSHYELLOW:
				set_brushcolour(p,YELLOW_BRUSH_COLOUR);
				return 0;
			case IDA_BRUSHGREEN:
				set_brushcolour(p,GREEN_BRUSH_COLOUR);
				return 0;
#ifdef _DEBUG
			case IDA_DEBUG_STATUS:
				debug_status(p);
				return 0;
#endif
			case ID_OPTIONS_STRETCHIMAGE:
				p->stretch_image=!p->stretch_image;
				p->window_valid=0;
				set_message(p,p->stretch_image?IDS_YESSTRETCH:IDS_NOSTRETCH);
				return 0;
			case ID_TOOLS_FILL:
				{
					HDC hdc;
					LOGBRUSH lb;
					HRGN rgn;
					HGDIOBJ oldpen,oldbrush;

					if(SUCCEEDED(IDirectDrawSurface2_GetDC(p->land,&hdc))) {
						lb.lbStyle=BS_SOLID;
						lb.lbColor=brush_cols[0];
						oldpen=SelectObject(hdc,CreatePen(PS_SOLID,0,brush_cols[0]));		/* yellow */
						oldbrush=SelectObject(hdc,CreateBrushIndirect(&lb));
						rgn=CreateRectRgn(1,1,p->area_width-1,p->area_height-1);
						Rectangle(hdc,1,1,p->area_width-1,p->area_height-1);
						SelectClipRgn(hdc,rgn);
						DeleteObject(rgn);
						DeleteObject(SelectObject(hdc,oldpen));
						DeleteObject(SelectObject(hdc,oldbrush));
						IDirectDrawSurface2_ReleaseDC(p->land,hdc);
						p->land_changed=1;
					}
				}
				return 0;
			}
			break;
		}
		break;
	}
	return DefWindowProc(h,msg,w,l);
}

/*
	wclass

  Registers the window class.
*/
static void wclass(void) {
	WNDCLASS w;

	w.style=CS_VREDRAW|CS_HREDRAW;
	w.lpfnWndProc=main_wndproc;
	w.cbClsExtra=w.cbWndExtra=0;
	w.hInstance=GetModuleHandle(0);
	w.hIcon=LoadIcon(GetModuleHandle(0),MAKEINTRESOURCE(PROGICON));
	w.hCursor=LoadCursor(0,IDC_CROSS);//IDC_ARROW);
	w.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);
	w.lpszMenuName=0;
	w.lpszClassName=CLASS_NAME;
	RegisterClass(&w);
}

/*
kill_stuff

  Releases DirectDraw objects referenced by the stuff object.
*/
static void kill_stuff(stuff_t *p) {
	if(p->clipper) {
		IDirectDrawClipper_Release(p->clipper);
		p->clipper=0;
	}
	if(p->land) {
		IDirectDrawSurface2_Release(p->land);
		p->land=0;
	}
	if(p->back) {
		IDirectDrawSurface2_Release(p->back);
		p->back=0;
	}
	if(p->primary) {
		IDirectDrawSurface2_Release(p->primary);
		p->primary=0;
	}
	dx_ddraw_exit();
}

/*
set_drops

  Sets the number of droplets. Existing droplets are removed and a fresh
  set is created.

  num_drops -> number of droplets
*/
static void set_drops(stuff_t *stuff,unsigned num_drops) {
	free(stuff->drops);
	stuff->droplets_bpp=1;
	if(!num_drops) {
		stuff->num_drops=0;
		stuff->drops=0;
	} else {
		unsigned idx;
		int i,j;

		stuff->drops=malloc(num_drops*sizeof(unsigned)*2);
		memset(stuff->drops,0,num_drops*sizeof(unsigned)*2);
		stuff->num_drops=num_drops;
		stuff->pitch=stuff->area_width*stuff->droplets_bpp;		/* will do for the moment */
		/* Bucket must be big enuogh to contain all droplets */
		stuff->bucket_size=(int)sqrt(stuff->num_drops)+10;
		/* Generate positions */
		idx=0;
		for(i=1;idx<=stuff->num_drops&&i<stuff->bucket_size;i++) {
			for(j=1;idx<stuff->num_drops&&j<i*2;j++) {
				BYTE *p;

				stuff->drops[idx*2]=((stuff->area_width/2-i)+j)*stuff->droplets_bpp;			/* X position */
				stuff->drops[idx*2]+=(stuff->bucket_size-i)*stuff->pitch;				/* Y position */
				p=(BYTE *)&stuff->drops[idx*2+1];
				*p=rand()>=RAND_MAX/2;			/* droplet type */
				idx++;
			}
		}
#ifdef _DEBUG
		log_droplets(stuff,get_string(IDS_DROPLETDATAFILE),"wt");
#endif
	}
}

/*
pixel

  Draws a pixel on a locked DirectDraw surface.

  ds -> points to DDSURFACEDESC as filled in by IDirectDrawSurface2::Lock()
  x -> X coordinate
  y -> Y coordinate
  val -> value to write (format appropriately, as per DDSURFACEDESC)
  mul -> "pixel multiplier" -- number of bytes per pixel.

*/
static void pixel(DDSURFACEDESC *ds,int x,int y,DWORD val,int mul) {
	BYTE *p;

	p=ds->lpSurface;
	p+=y*ds->lPitch;
	p+=x*mul;
	switch(mul) {
	case 1:
		*p=(BYTE)val;
		break;
	case 2:
		*((WORD *)p)=(WORD)val;
		break;
	case 3:
		p[0]=(BYTE)(val&0xFF);		/* R */
		p[1]=(BYTE)(val>>8);		/* G */
		p[2]=(BYTE)(val>>16);		/* B */
		break;
	case 4:
		*((DWORD *)p)=val;
		break;
	}
}

static void do_land_border(stuff_t *stuff) {
	DWORD colour=stuff->pf.dwRBitMask|stuff->pf.dwBBitMask|stuff->pf.dwGBitMask;
	int cx=stuff->area_width/2;

	dx_fill_area(stuff->land,colour,0,0,stuff->area_width-1,0);
	dx_fill_area(stuff->land,colour,0,stuff->area_height-1,stuff->area_width-1,stuff->area_height-1);
	dx_fill_area(stuff->land,colour,0,0,0,stuff->area_height-1);
	dx_fill_area(stuff->land,colour,stuff->area_width-1,0,stuff->area_width-1,stuff->area_height-1);
	dx_fill_area(stuff->land,0,cx-(stuff->bucket_neck_size-1),0,cx+(stuff->bucket_neck_size-1),0);
	dx_fill_area(stuff->land,0,cx,stuff->area_height-1,cx,stuff->area_height-1);
}


/* Draw bucket and frame */
static void do_bucket(stuff_t *stuff) {
	int i,cx,cy;
	DWORD white=stuff->pf.dwRBitMask|stuff->pf.dwBBitMask|stuff->pf.dwGBitMask;
	cx=stuff->area_width/2;
	for(i=1;i<=stuff->bucket_size;i++) {
		int dx;

		cy=stuff->bucket_size-i;
		dx=max(i,stuff->bucket_neck_size);
		dx_fill_area(stuff->back,white,0,cy,cx-dx,cy);
		dx_fill_area(stuff->back,white,cx+dx,cy,stuff->area_width-1,cy);
	}
}

static void fix_droplet_data(stuff_t *stuff,unsigned this_pitch) {
	if(stuff->pitch!=this_pitch||stuff->droplets_bpp!=stuff->dd_bpp) {
		int n_l=0,n_r=0;
		unsigned *p=stuff->drops,x,y,i;

		dprintf("fix_droplet_data: before: pitch=%u bpp=%u after: pitch=%u bpp=%d\n",
			stuff->pitch,stuff->droplets_bpp,this_pitch,stuff->dd_bpp);
		for(i=0;i<stuff->num_drops;i++,p+=2) {
			x=(*p%stuff->pitch)/stuff->droplets_bpp;
			y=*p/stuff->pitch;
			*p=x*stuff->dd_bpp+y*this_pitch;
		}
		stuff->pitch=this_pitch;
		stuff->droplets_bpp=stuff->dd_bpp;
		/* RND table as well */
		for(i=0;i<RND_TBL_SIZE;i++) {
			dir_tbl[i]=((float)rand()/RAND_MAX)>0.5?-stuff->dd_bpp:+stuff->dd_bpp;
			if((signed)dir_tbl[i]<0) {
				n_l++;
			} else {
				n_r++;
			}
		}
		dprintf("dir_tbl: %d elements: %d left, %d right\n",RND_TBL_SIZE,n_l,n_r);
	}
}

static void cons(stuff_t *stuff) {
	stuff->window_valid=0;
	stuff->ddraw_valid=0;
	stuff->ddraw_bad=0;
	stuff->primary=0;
	stuff->back=0;
	stuff->land=0;
	stuff->clipper=0;
	stuff->paused=1;
	stuff->new_num_drops=0;
	stuff->no_catchup=0;
	stuff->update_diff=10;

	stuff->view_x=0;
	stuff->view_y=0;
	stuff->include_bucket=0;
	stuff->land_backup=0;

	stuff->msg=0;
}

static void defaults(stuff_t *stuff) { 
	stuff->asm=0;
	stuff->num_drops=0;
	stuff->drops=0;
	stuff->pitch=0;
	stuff->area_width=MIN_AREA_WIDTH;
	stuff->area_height=400;
	stuff->view_width=MIN_AREA_WIDTH;
	stuff->view_height=400;
	stuff->stretch_image=0;

	stuff->popup_menu=0;
	stuff->use_wm_paint=0;

	set_zoom(stuff,1,1);
	set_brushsize(stuff,1);
	set_brushcolour(stuff,0);
	stuff->bucket_neck_size=5;
}

// C4127: conditional expression is constant
#define CHK do{if(FAILED(hr)) {kill_stuff(stuff); return describe_dx_error(hr);}}__pragma(warning(push)) __pragma(warning(disable:4127)) while(0) __pragma(warning(pop))

/* reset DirectDraw: kill all objects as necessary, then recreate them. */
static char *reset_ddraw(stuff_t *stuff,HWND h_wnd) {
	HRESULT hr;

	kill_stuff(stuff);
	stuff->ddraw_valid=stuff->ddraw_bad=1;
	hr=IDirectDraw2_SetCooperativeLevel(dx_ddraw(),0,DDSCL_NORMAL);
	if(FAILED(hr)) {
		return describe_dx_error(hr);
	}
	stuff->primary=dx_create_surface(DDSCAPS_PRIMARYSURFACE,-1,-1);
	stuff->back=dx_create_surface(DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY,stuff->area_width,stuff->area_height+stuff->bucket_size);
	stuff->land=dx_create_surface(DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY,stuff->area_width,stuff->area_height);
	if(!stuff->primary||!stuff->back||!stuff->land) {
		kill_stuff(stuff);
		return get_string(IDS_NO_SURFACES_MSG);
	}
	stuff->pf.dwSize=sizeof(stuff->pf);
	IDirectDrawSurface2_GetPixelFormat(stuff->primary,&stuff->pf);
	/* Set up back surface */
	dx_clear_surface(stuff->back);
	do_bucket(stuff);
	/* Initialise functions for this bit depth */
	{
		funcs_t *p;

		for(p=funcsarr;p->bpp&&p->bpp!=stuff->pf.dwRGBBitCount;p++) {
		}
		if(p->bpp) {
			int asm_ok=p->update_all_droplets_asm&&p->draw_all_droplets_asm;

			EnableMenuItem(GetMenu(h_wnd),ID_OPTIONS_ASSEMBLERVERSION,asm_ok?MF_ENABLED:MF_DISABLED);
			CheckMenuItem(GetMenu(h_wnd),ID_OPTIONS_ASSEMBLERVERSION,(asm_ok&&stuff->asm)?MF_CHECKED:MF_UNCHECKED);
			if(stuff->asm&&asm_ok) {
				update_all_droplets=p->update_all_droplets_asm;
				draw_all_droplets=p->draw_all_droplets_asm;
			} else {
				update_all_droplets=p->update_all_droplets;
				draw_all_droplets=p->draw_all_droplets;
			}
			stuff->dd_bpp=p->bpp/8;
		} else {
			/* Unsupported */
			kill_stuff(stuff);
			return get_string(IDS_BADBITDEPTH);
		}
	}
	/* Restore landscape or just clear the surface */
	if(!restore_land(stuff)) {
		dx_clear_surface(stuff->land);
		do_land_border(stuff);
	}
	stuff->land_changed=1;
	hr=IDirectDraw2_CreateClipper(dx_ddraw(),0,&stuff->clipper,0);
	CHK;
	hr=IDirectDrawClipper_SetHWnd(stuff->clipper,0,h_wnd);
	CHK;
	hr=IDirectDrawSurface2_SetClipper(stuff->primary,stuff->clipper);
	CHK;
	stuff->ddraw_bad=0;
	return 0;
}

static void get_decals_size(stuff_t *stuff,int *width,int *height) {
	if(width) {
		*width=GetSystemMetrics(SM_CXSIZEFRAME)*2;
		*width+=GetSystemMetrics(SM_CXVSCROLL);
	}
	if(height) {
		*height=GetSystemMetrics(SM_CYSIZEFRAME)*2+GetSystemMetrics(SM_CYCAPTION);
		if(!stuff->popup_menu) {
			*height+=GetSystemMetrics(SM_CYMENU);
		}
		*height+=GetSystemMetrics(SM_CYHSCROLL);
	}
}

/*
window_size

  Given a rectangle constrains it appropriately given which side it is being resized
  from.

  stuff -> stuff ptr
  h_wnd -> handle of window
  in_rect -> points to current rectangle
  side -> side being dragged from (WMSZ_ constants as per WM_SIZING)

  Return: in_rect is fixed up with the new drag rectangle.
*/
static void window_size(stuff_t *stuff,HWND h_wnd,RECT *in_rect,int side) {
	int dw,dh,cw,ch,diff,height;
	RECT r;

	(void)h_wnd;

#ifdef DEBUG_WINDOW_SIZE
	dprintf("window_size: input rect: left=%d top=%d right=%d bottom=%d\n",in_rect->left,in_rect->top,in_rect->right,in_rect->bottom);
#endif
	r=*in_rect;
	get_decals_size(stuff,&dw,&dh);
	/* Horizontal aspect */
	cw=(r.right-r.left)-dw;			/* Client area */
	diff=cw-stuff->area_width*stuff->w_mul;
	if(!diff||(diff>0&&!stuff->stretch_image)) {
		switch(side) {
		case WMSZ_BOTTOMLEFT:
		case WMSZ_LEFT:
		case WMSZ_TOPLEFT:
			r.left+=diff;
			break;
		case WMSZ_BOTTOMRIGHT:
		case WMSZ_RIGHT:
		case WMSZ_TOPRIGHT:
		default:
			r.right-=diff;
			break;
		}
		stuff->hscroll=0;
		stuff->view_width=cw-diff;//stuff->area_width;
	} else if(diff<0) {		/* Scroll bars */
		stuff->hscroll=1;
		stuff->view_width=cw;
	}
	/* Vertical aspect */
	ch=(r.bottom-r.top)-dh;
	height=stuff->area_height;
	if(stuff->include_bucket) {
		height+=stuff->bucket_size;
	}
	height*=stuff->h_mul;
	diff=ch-height;
	if(!diff||(diff>0&&!stuff->stretch_image)) {
		switch(side) {
		case WMSZ_BOTTOM:
		case WMSZ_BOTTOMLEFT:
		case WMSZ_BOTTOMRIGHT:
		default:
			r.bottom-=diff;
			break;
		case WMSZ_TOP:
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
			r.top+=diff;
			break;
		}
		stuff->vscroll=0;
		stuff->view_height=ch-diff;//stuff->area_height;
	} else if(diff<0) {		/* Scroll bars */
		stuff->vscroll=1;
		stuff->view_height=ch;

	}
#ifdef DEBUG_WINDOW_SIZE
	dprintf("window_size: output rect: left=%d top=%d right=%d bottom=%d ",r.left,r.top,r.right,r.bottom);
	dprintf("(client: width=%d height=%d)\n",(r.right-r.left)-dw,(r.bottom-r.top)-dh);
	dprintf("window_size: view_width=%d view_height=%d hscroll=%s vscroll=%s\n",
		stuff->view_width,stuff->view_height,stuff->hscroll?"yes":"no",stuff->vscroll?"yes":"no");
#endif
	*in_rect=r;
}

#define DWS_WM_SIZING_MODE (1<<30)

/*
do_window_stuff

  Does window "stuff". Resizing, setting scrollbars, etc.

  stuff -> stuff ptr
  h_wnd -> handle of window
  flags -> A combination of:

	DWS_WM_SIZING_MODE		Run in WM_SIZING handler mode (examine rect+side, update rect,
							a la WM_SIZING message rqeuirements)
	RW_NOSETWINDOWPOS		Don't set new window position and size
	RW_NOMESSAGES			Don't print messages

  rect -> pointer to current window sizing rectangle
  side -> side window is being sized from

  If flags doesn't include DWS_WM_SIZING_MODE, side and rect are ignored.

  DWS_WM_SIZING_MODE implies RW_NOSETWINDOWPOS.

  RW_NOMESSAGES may be irrelevant; messages aren't always printed, and they only appear
  on the debugging output anyway.

  Return: rect updated, if flags&DWS_WM_SIZING_MODE.
*/
static void do_window_stuff(stuff_t *stuff,HWND h_wnd,unsigned flags,RECT *rect,int side) {
	RECT win_rect;
	SCROLLINFO si;

	si.cbSize=sizeof(si);
	si.fMask=SIF_ALL|SIF_DISABLENOSCROLL;
	if(!(flags&DWS_WM_SIZING_MODE)) {
		GetWindowRect(h_wnd,&win_rect);
		rect=&win_rect;
	}
	window_size(stuff,h_wnd,rect,(flags&DWS_WM_SIZING_MODE)?side:WMSZ_BOTTOMRIGHT);
//	get_decals_size(stuff,&decals_w,&decals_h);
	SetMenu(h_wnd,stuff->popup_menu?0:stuff->menu);
	ShowScrollBar(h_wnd,SB_VERT,TRUE);
	EnableScrollBar(h_wnd,SB_VERT,stuff->vscroll?ESB_ENABLE_BOTH:ESB_DISABLE_BOTH);
	if(!stuff->vscroll) {
//		ShowScrollBar(h_wnd,SB_VERT,FALSE);
		stuff->view_y=stuff->include_bucket?0:stuff->bucket_size;
	} else {
		si.nMin=stuff->include_bucket?0:stuff->bucket_size;
		si.nMax=stuff->area_height+stuff->bucket_size-1;
		si.nPage=stuff->view_height/stuff->h_mul;
		si.nPos=stuff->view_y;
		SetScrollInfo(h_wnd,SB_VERT,&si,FALSE);
		SendMessage(h_wnd,WM_VSCROLL,MAKELONG(SB_THUMBPOSITION,si.nPos),0);
		SendMessage(h_wnd,WM_VSCROLL,MAKELONG(SB_ENDSCROLL,0),0);
	}
	ShowScrollBar(h_wnd,SB_HORZ,TRUE);
	EnableScrollBar(h_wnd,SB_HORZ,stuff->hscroll?ESB_ENABLE_BOTH:ESB_DISABLE_BOTH);
	if(!stuff->hscroll) {
//		ShowScrollBar(h_wnd,SB_HORZ,FALSE);
		stuff->view_x=0;
	} else {
		si.nMin=0;
		si.nMax=stuff->area_width-1;
		si.nPage=stuff->view_width/stuff->w_mul;
		si.nPos=stuff->view_x;
		SetScrollInfo(h_wnd,SB_HORZ,&si,FALSE);
		SendMessage(h_wnd,WM_HSCROLL,MAKELONG(SB_THUMBPOSITION,si.nPos),0);
		SendMessage(h_wnd,WM_HSCROLL,MAKELONG(SB_ENDSCROLL,0),0);
	}
	if(!(flags&(RW_NOSETWINDOWPOS|DWS_WM_SIZING_MODE))) {
		MoveWindow(h_wnd,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,TRUE);
	}
	if(!(flags&DWS_WM_SIZING_MODE)) {
		CheckMenuItem(stuff->menu,ID_OPTIONS_STRETCHIMAGE,stuff->stretch_image?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(stuff->menu,IDA_TOGGLEBUCKET,stuff->include_bucket?MF_CHECKED:MF_UNCHECKED);
	}
#ifdef DEBUG_DO_WINDOW_STUFF
	if(!(flags&RW_NOMESSAGES)) {
		RECT client;

	/* Informative message */
		if(!(flags&DWS_WM_SIZING_MODE)) {
			GetWindowRect(h_wnd,&win_rect);
		}
		GetClientRect(h_wnd,&client);
		dprintf("reset_window: client size is %d x %d\n",client.right,client.bottom);
		dprintf("reset_window: window size is %d x %d (at %d,%d)\n",rect->right-rect->left,rect->bottom-rect->top,rect->left,rect->top);
	}
#endif
}

static void reset_window(stuff_t *stuff,HWND h_wnd,unsigned flags) {
	do_window_stuff(stuff,h_wnd,flags,0,0);
}

static void sizing_window(stuff_t *stuff,HWND h_wnd,RECT *rect,int side) {
	do_window_stuff(stuff,h_wnd,DWS_WM_SIZING_MODE|RW_NOSETWINDOWPOS,rect,side);
}

static void paint_window(HWND h_wnd,stuff_t *stuff) {
	if(stuff->ddraw_valid&&!stuff->ddraw_bad&&stuff->window_valid) {
		POINT tlpos;
		RECT rect,src_rect,dest_rect;
		HRESULT br;

		if(FAILED(IDirectDrawSurface2_IsLost(stuff->primary))) {
			IDirectDrawSurface2_Restore(stuff->primary);
		}
		if(FAILED(IDirectDrawSurface2_IsLost(stuff->back))) {
			IDirectDrawSurface2_Restore(stuff->back);
		}
		tlpos.x=tlpos.y=0;
		if(!ClientToScreen(h_wnd,&tlpos)||!GetClientRect(h_wnd,&rect)) {
			return;
		}
		/* Dest rectangle */
		dest_rect.left=0;
		dest_rect.right=rect.right;
		dest_rect.top=0;
		dest_rect.bottom=rect.bottom;
		/* Source rectangle */
		src_rect.left=stuff->view_x;
		src_rect.top=stuff->view_y;
		src_rect.right=src_rect.left+(int)(stuff->view_width/(float)stuff->w_mul);
		src_rect.bottom=src_rect.top+(int)(stuff->view_height/(float)stuff->h_mul);
		/* It seems that small rounding errors may occur in the above operations, and generate
		   invalid rectangle errors when diong the Blt. */
		if(src_rect.right>stuff->area_width) {
			src_rect.right=stuff->area_width;
		}
		if(src_rect.bottom>stuff->area_height+stuff->bucket_size) {
			src_rect.bottom=stuff->area_height+stuff->bucket_size;
		}
		/* Fix dest rect coordinates (from client coords -> screen coords) */
		OffsetRect(&dest_rect,tlpos.x,tlpos.y);
		if(dest_rect.right>dest_rect.left&&dest_rect.bottom>dest_rect.top) {
			br=IDirectDrawSurface2_Blt(stuff->primary,&dest_rect,stuff->back,&src_rect,DDBLT_WAIT,0);
			if(FAILED(br)) {
#ifdef DEBUG_PAINT_WINDOW
				dprintf("paint_window: back->primary: Blt failed: %s\n",describe_dx_error(br));
				dprintf("\tsrc_rect:\tleft=%d\ttop=%d\tright=%d\tbottom=%d\n",src_rect.left,src_rect.top,
					src_rect.right,src_rect.bottom);
				dprintf("\tdest_rect:\tleft=%d\ttop=%d\tright=%d\tbottom=%d\n",dest_rect.left,dest_rect.top,
					dest_rect.right,dest_rect.bottom);
				dprintf("\tbucket_size=%d\n",stuff->bucket_size);
#endif
			}
		}
		/* This ends up a bit flickery! */
		if(stuff->msg) {
			HDC dc;
			RECT r;

			dc=GetWindowDC(h_wnd);
			GetClientRect(h_wnd,&r);
			SelectObject(dc,GetStockObject(WHITE_PEN));
			SelectObject(dc,GetStockObject(SYSTEM_FONT));
			SetTextAlign(dc,TA_BASELINE|TA_RIGHT);
			//SetBkColor(dc,RGB(0,0,0));
			//SetBkMode(dc,TRANSPARENT);
			TextOut(dc,r.right-GetSystemMetrics(SM_CXFIXEDFRAME)*2,
				r.bottom-GetSystemMetrics(SM_CYFIXEDFRAME)*2,stuff->msg,strlen(stuff->msg));
			ReleaseDC(h_wnd,dc);
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nShowCmd) {
	MSG msg;
	int done=0;
	HWND h_wnd=0;
	stuff_t stuff;
	DWORD l_upd,diff,tick;
	HACCEL accelerator=0;
	STARTUPINFO sif;

	(void)hInstance,(void)hPrevInstance,(void)lpCmdLine,(void)nShowCmd;

	GetStartupInfo(&sif);
#ifndef _DEBUG
	srand(GetTickCount());
#endif
	stuff.menu=LoadMenu(GetModuleHandle(0),MAKEINTRESOURCE(ID_MAINMENU));
	cons(&stuff);
	defaults(&stuff);
	set_drops(&stuff,NUM_DROPLETS);
	wclass();
	accelerator=LoadAccelerators(GetModuleHandle(0),MAKEINTRESOURCE(IDR_ACCELERATOR1));
	h_wnd=CreateWindow(CLASS_NAME,get_string(IDS_WINDOW_TITLE),WS_BORDER|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_HSCROLL|WS_VSCROLL|WS_THICKFRAME,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
		0,0,GetModuleHandle(0),&stuff);
	stuff.view_x=0;
	stuff.view_y=stuff.bucket_size;
	reset_window(&stuff,h_wnd,0);
	ShowWindow(h_wnd,(sif.dwFlags&STARTF_USESHOWWINDOW)?sif.wShowWindow:SW_SHOWDEFAULT);
//	UpdateWindow(h_wnd);
	l_upd=GetTickCount();
	while(!done) {
		if(!stuff.window_valid) {		/* Reset window */
			reset_window(&stuff,h_wnd,0);
			stuff.window_valid=1;
		}
		if(!stuff.ddraw_valid&&stuff.window_valid) {
			char *msg;
			HCURSOR oc;

			oc=SetCursor(LoadCursor(0,IDC_WAIT));
			msg=reset_ddraw(&stuff,h_wnd);
			SetCursor(oc);
			if(msg) {
				char buf[1000];

				InvalidateRect(h_wnd,0,TRUE);
				strcpy(buf,get_string(IDS_DDRAW_ERROR_MSGBOX1));
				strcat(buf,msg);
				strcat(buf,get_string(IDS_DDRAW_ERROR_MSGBOX2));
				MessageBox(h_wnd,buf,get_string(IDS_ERROR_TITLE),MB_OK|MB_ICONEXCLAMATION);
				dprintf("reset_draw says \"%s\"\n",msg);
				stuff.paused=1;
			} else {
				droplet_colours[0]=stuff.pf.dwRBitMask;
				droplet_colours[1]=stuff.pf.dwBBitMask;
				droplet_dirs[0]=-stuff.dd_bpp;
				droplet_dirs[1]=stuff.dd_bpp;
				dprintf("reset_draw: returned OK.\n");
				dprintf("\tdroplet_colors[0]=0x%08lX, droplet_colours[1]=0x%08lX\n",droplet_colours[0],
					droplet_colours[1]);
				dprintf("\tdroplet_dirs[0]=%d, droplet_dirs[1]=%d\n",droplet_dirs[0],droplet_dirs[1]);
				dprintf("\tcolour depth: %dbpp\n",stuff.dd_bpp*8);
			}
		}
#ifdef GREEDY_MESSAGE_LOOP
		while(!done&&PeekMessage(&msg,0,0,0,PM_NOREMOVE))
#else
		if(PeekMessage(&msg,0,0,0,PM_NOREMOVE))
#endif
		{
			if(!GetMessage(&msg,0,0,0)) {
				done=1;
				/* #ifndef GREEDY_MESSAGE_LOOP this breaks out of while(!done) above; otherwise, breaks out
				   of while(!done&&PeekMEssage(blah blah blah)) above. */
				break;
			}
			if(!TranslateAccelerator(h_wnd,accelerator,&msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
#ifdef GREEDY_MESSAGE_LOOP
		/* break above (with GREEDY_MESSAGE_LOOP) gets here */
		if(done) {
			continue;
		}
#endif
		tick=GetTickCount();
		/* Message decay */
		if(tick>stuff.msg_time) {
			free(stuff.msg);
			stuff.msg=0;
		}
		if(stuff.paused||stuff.no_catchup) {
			l_upd=tick;
			diff=0;
		} else {
			diff=tick-l_upd;
		}
		if(diff>stuff.update_diff||stuff.paused||stuff.no_catchup) {
			DDSURFACEDESC ds;

			if(!stuff.new_num_drops) {
				stuff.no_catchup=0;
			}
			if(stuff.ddraw_valid&&!stuff.ddraw_bad) {
				HRESULT hr;
				int no_era=0;
				unsigned i;

				diff/=stuff.update_diff;
				memset(&ds,0,sizeof(ds));
				ds.dwSize=sizeof(ds);
				/*dprintf("processing %u frames\n",diff);*/
				if(stuff.land_changed) {
					RECT dest;

					dest.left=0;
					dest.top=stuff.bucket_size;
					dest.right=stuff.area_width;
					dest.bottom=stuff.area_height+stuff.bucket_size;
					hr=IDirectDrawSurface2_Blt(stuff.back,&dest,stuff.land,0,DDBLT_WAIT,0);
					if(SUCCEEDED(hr)) {
						stuff.land_changed=0;
						no_era=1;
					}
				}
				if(stuff.new_num_drops) {
					//draw_all_droplets(&stuff,0);
					/* Don't erase if no_era! */
					if(!no_era) {
						dx_with_lock(stuff.back,0,&stuff,draw_all_droplets);
					}
					set_drops(&stuff,stuff.new_num_drops);
					stuff.new_num_drops=0;
					no_era=1;
				}
				if(stuff.paused) {
					/* If no_era is true, the droplets have been erased already and must
					   be redrawn. */
					if(no_era) {
						//draw_all_droplets(&stuff,-1);
						dx_with_lock(stuff.back,-1,&stuff,draw_all_droplets);
						no_era=0;
					}
				} else {
					diff=1;//HACK
					for(i=0;i<diff;i++) {
						dx_with_lock(stuff.back,no_era,&stuff,update_all_droplets);
						no_era=0;
					}
				}
				/* frames */
				l_upd+=diff*stuff.update_diff;	/* yes, to prevent timer going wrong */
			}
			/* draw the update(s) */
			if(stuff.use_wm_paint) {
				InvalidateRect(h_wnd,0,FALSE);
			} else {
				paint_window(h_wnd,&stuff);
			}
		}
	}
	set_drops(&stuff,0);
	DestroyMenu(stuff.menu);
	free(stuff.msg);
	_cexit();
	_CrtCheckMemory();
	_CrtDumpMemoryLeaks();
	ExitProcess(0);
}

/* This is a dx_with_lock callback function. */
static void draw_all_droplets16(int mask,void *vstuff,DDSURFACEDESC *ds) {
	stuff_t *stuff=vstuff;
	unsigned *p,j;
	BYTE *surface;

	surface=ds->lpSurface;
	fix_droplet_data(stuff,ds->lPitch);
	p=stuff->drops;
	for(j=0;j<stuff->num_drops;j++,p+=2) {
		*((WORD *)(surface+*p))=(WORD)(droplet_colours[*((BYTE *)(p+1))]&((unsigned)mask));
	}
}

/* This is a dx_with_lock callback function. */
void update_all_droplets16(int no_era,void *vstuff,DDSURFACEDESC *ds) {
	static unsigned r_idx=0;
	stuff_t *stuff=vstuff;
	WORD value,lval,rval;
	unsigned max,t_p,type,*p,j;
	BYTE *tptr,*surface;
	int pitch;

	value=(WORD)stuff->pf.dwGBitMask;
	fix_droplet_data(stuff,ds->lPitch);
	/* -1 -- droplets go back to top upon falling into the hole rather than falling
	   below it. */
	max=(stuff->area_height+stuff->bucket_size-1)*ds->lPitch;
	/* If the landscape was erased, the old droplets are no longer in place.
	   This is unfortunate because they must be there. This redraws them. */
	surface=ds->lpSurface;
	pitch=ds->lPitch;
	if(no_era) {
		p=stuff->drops;
		for(j=0;j<stuff->num_drops;j++,p+=2) {
			*((WORD *)(surface+*p))=(WORD)droplet_colours[*((BYTE *)(p+1))];;
		}
		no_era=0;
	}
	p=stuff->drops;
	//for(j=stuff->num_drops;j;j--,p+=2) {
	for(j=0;j<stuff->num_drops;j++,p+=2) {
		t_p=*p;
		type=*((BYTE *)(p+1));
		tptr=surface+t_p;
		*((WORD *)tptr)=0;
		/* where now */
		if(!*((WORD *)(tptr+pitch))) {
			t_p+=pitch;
		} else {
			lval=*((WORD *)(tptr-2));
			rval=*((WORD *)(tptr+2));
			if(!lval) {				/* can move left */
				if(!rval) {			/* can move left or right */
					if(*((WORD *)(tptr+pitch))==value) {
						t_p+=droplet_dirs[type];
					} else {
						t_p+=dir_tbl[r_idx++];
						r_idx&=RND_TBL_IDX_MASK;
					}
				} else {
					t_p-=2;			/* can move left only */
				}
			} else {				/* cannot move left */
				if(!rval) {			/* can move right only */
					t_p+=2;
				} else {			/* can move up only */
					if(t_p>=(unsigned)pitch&&!*((WORD *)(tptr-pitch))) {
						t_p-=pitch;
					}
				}
			}
		}
		/* if(t_p>=max) {t_p-=max;} */
		t_p%=max;
		/* draw */
		*((WORD *)(surface+t_p))=(WORD)droplet_colours[type];
		*p=t_p;
	}
}

/* This is a dx_with_lock callback function. */
static void draw_all_droplets32(int mask,void *vstuff,DDSURFACEDESC *ds) {
	stuff_t *stuff=vstuff;
	unsigned *p,j;
	BYTE *surface;

	surface=ds->lpSurface;
	fix_droplet_data(stuff,ds->lPitch);
	p=stuff->drops;
	for(j=0;j<stuff->num_drops;j++,p+=2) {
		*((DWORD *)(surface+*p))=droplet_colours[*((BYTE *)(p+1))]&((unsigned)mask);
	}
}

/* This is a dx_with_lock callback function. */
void update_all_droplets32(int no_era,void *vstuff,DDSURFACEDESC *ds) {
	static unsigned r_idx=0;
	stuff_t *stuff=vstuff;
	DWORD value,lval,rval;
	unsigned max,t_p,type,*p,j;
	BYTE *tptr,*surface;
	int pitch;

	value=stuff->pf.dwGBitMask;
	fix_droplet_data(stuff,ds->lPitch);
	/* -1 -- droplets go back to top upon falling into the hole rather than falling
	   below it. */
	max=(stuff->area_height+stuff->bucket_size-1)*ds->lPitch;
	/* If the landscape was erased, the old droplets are no longer in place.
	   This is unfortunate because they must be there. This redraws them. */
	surface=ds->lpSurface;
	pitch=ds->lPitch;
	if(no_era) {
		p=stuff->drops;
		for(j=0;j<stuff->num_drops;j++,p+=2) {
			*((DWORD *)(surface+*p))=droplet_colours[*((BYTE *)(p+1))];
		}
		no_era=0;
	}
	p=stuff->drops;
	//for(j=stuff->num_drops;j;j--,p+=2) {
	for(j=0;j<stuff->num_drops;j++,p+=2) {
		DWORD below;

		t_p=*p;
		type=*((BYTE *)(p+1));
		tptr=surface+t_p;
		*((DWORD *)tptr)=0;
		/* where now */
		below=*(DWORD *)(tptr+pitch);
		if(below==0) {
			t_p+=pitch;
		} else {
			lval=*((DWORD *)(tptr-4));
			rval=*((DWORD *)(tptr+4));
			if(!lval) {				/* can move left */
				if(!rval) {			/* can move left or right */
					if(below==value) {
						t_p+=droplet_dirs[type];
					} else {
						t_p+=dir_tbl[r_idx++];
						r_idx&=RND_TBL_IDX_MASK;
					}
				} else {
					t_p-=4;			/* can move left only */
				}
			} else {				/* cannot move left */
				if(!rval) {			/* can move right only */
					t_p+=4;
				} else {			/* can move up only */
					if(t_p>=(unsigned)pitch&&!*((DWORD *)(tptr-pitch))) {
						t_p-=pitch;
					}
				}
			}
		}
		/* if(t_p>=max) {t_p-=max;} */
		//t_p%=max;
		if(t_p>=max) {
			t_p-=max;
		}
		/* draw */
		*((DWORD *)(surface+t_p))=droplet_colours[type];
		*p=t_p;
	}
}

static int restore_land(stuff_t *stuff) {
	COLORREF *src;
	HDC dc;
	int x,y;

	if(!stuff->land_backup) {
		return 0;
	}
	if(SUCCEEDED(IDirectDrawSurface2_GetDC(stuff->land,&dc))) {
		src=stuff->land_backup;
		for(y=0;y<stuff->area_height;y++) {
			for(x=0;x<stuff->area_width;x++) {
				SetPixel(dc,x,y,*src++);
			}
		}
		IDirectDrawSurface2_ReleaseDC(stuff->land,dc);
	}
	free(stuff->land_backup);
	stuff->land_backup=0;
	return 1;
}

static void save_land(stuff_t *stuff) {
	COLORREF *dest;
	HDC dc;
	int x,y;

	if(stuff->land_backup) {
		free(stuff->land_backup);
	}
	dest=stuff->land_backup=malloc(stuff->area_width*stuff->area_height*sizeof(COLORREF));
	if(SUCCEEDED(IDirectDrawSurface2_GetDC(stuff->land,&dc))) {
		for(y=0;y<stuff->area_height;y++) {
			for(x=0;x<stuff->area_width;x++) {
				*dest++=GetPixel(dc,x,y);
			}
		}
		IDirectDrawSurface2_ReleaseDC(stuff->land,dc);
	} else {
		free(stuff->land_backup);
		stuff->land_backup=0;
	}
}

#if 0
/* This is a dx_with_lock callback function. */
static void draw_all_droplets16_asm(int mask,void *vstuff,DDSURFACEDESC *ds) {
	stuff_t *stuff=vstuff;
	unsigned *p,j;
	BYTE *surface;

	surface=ds->lpSurface;
	fix_droplet_data(stuff,ds->lPitch);
	p=stuff->drops;
	for(j=0;j<stuff->num_drops;j++,p+=2) {
		*((WORD *)(surface+*p))=(WORD)(droplet_colours[*((BYTE *)(p+1))]&((unsigned)mask));
	}
}

/* This is a dx_with_lock callback function. */
void update_all_droplets16_asm(int no_era,void *vstuff,DDSURFACEDESC *ddsd) {
	DDSURFACEDESC ddsurfacedesc;	// seems to be necessary for assembler syntax
	unsigned max;
	/*
		eax -> 
		ebx -> counter
		ecx -> max
		edx -> pitch
		esi -> ptr to surface
		edi
		ebp
	*/
	/* -1 -- droplets go back to top upon falling into the hole rather than falling
	   below it. */
	{
		stuff_t *stuff=vstuff;
		fix_droplet_data(stuff,ddsd->lPitch);
		max=(stuff->area_height+stuff->bucket_size-1)*ddsd->lPitch;
	}
	__asm {
		mov edi,vstuff;
		cmp no_era,0;
		jne do_droplets;
redraw_droplets:
		mov edi,vstuff;
		mov ecx,[edi].num_drops;
		dec ecx;
		mov esi,[edi].drops;
		mov edi,ddsd;
		mov edi,[edi]ddsurfacedesc.lpSurface;
		xor ebx,ebx;
redraw_droplets_lp:
		// ptr to cur droplet; counter; ptr to droplet_colours; ptr to surface;
		mov bl,[esi+ecx*8+4];		// droplet type
		mov eax,[esi+ecx*8];		// droplet offset
		mov edx,[ebx*4+droplet_colours];// droplet value in EDX
		mov [edi+eax],dx;			// write value
		dec ecx;
		jns redraw_droplets_lp;
do_droplets:
		mov edi,vstuff;
		mov ecx,[edi].num_drops;
		mov esi,[edi].drops;
		dec ecx;
		mov edi,ddsd;
		mov ebx,[edi]ddsurfacedesc.lPitch;
		mov edi,[edi]ddsurfacedesc.lpSurface;
do_droplets_lp:
		xor edx,edx;				// erase EDX for OR below
		mov eax,[esi+ecx*8];		// droplet offset
		mov word ptr [edi+eax],0;	// erase old droplet
		add eax,edi;				// eax=address of droplet in surface memory
		or dx,[eax];				// fetch value, set flags
		jnz not_move_downwards;
move_downwards:
		add eax,ebx;				// move downwards
		jmp next_droplet;

not_move_downwards:
		mov dx,[eax-2];				// left droplet
		
next_droplet:
	}
}
#endif
