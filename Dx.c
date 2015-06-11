/* DirectDraw "library" */
#define INITGUID
#include <windows.h>
#include <ddraw.h>
#include "debug.h"
#include "dx.h"

/* The DirectX SDK that comes with VC++5 doesn't have DDLOCK_NOSYSLOCK. */
/* In addition, refuse use of NO_SYSLOCK, even if available, by defining DONT_USE_NOSYSLOCK. Appears
   to be a problem with backwards compatiblity (DIRECTDRAW_VERSION=0x300) with DX7 headers -- DX3
   doesn't appear to support DDLOCK_NOSYSLOCK. This fixes NT4 compatibility. */
//#define DONT_USE_NOSYSLOCK
#if !defined(DONT_USE_NOSYSLOCK) && !defined(DDLOCK_NOSYSLOCK)
#define DDLOCK_NOSYSLOCK (0)
#endif

/***** DirectDraw *****/

static IDirectDraw2 *ddraw_object=0;

/* Call a DirectX function and, if it fails, print descriptive message to debugger
   and return false/0/NULL (delete as inapplicable).

   NB do {} while for ;s and local vars; should be optmiised out. */
#ifdef _DEBUG
#define DXCALL(K) \
	do {\
		HRESULT hr=(K);\
		if(FAILED(hr)) {\
			dprintf("%s:%d: %s failed: %s\n",__FILE__,__LINE__,#K,describe_dx_error(hr));\
			return 0;\
		}\
		} __pragma(warning(push)) __pragma(warning(disable:4127)) while(0) __pragma(warning(pop))
#else
#define DXCALL(K) do{if(FAILED(K)){return 0;}}__pragma(warning(push)) __pragma(warning(disable:4127)) while(0) __pragma(warning(pop))
#endif
/*
	IDirectDraw2 *dx_ddraw(void)
	
	What
		Returns a pointer to the DirectDraw object. Does not bump the
		object's reference count.

	Inputs
		void

	Output
		returns IDirectDraw2 *, pointer to the IDirectDraw2 object. 
	
	Notes
		First call calls DirectDrawCreate et al to create IDirectDraw2
		object; further invocations return this same object.
*/
IDirectDraw2 *dx_ddraw(void) {
	if(ddraw_object) {
		return ddraw_object;
	} else {
		IDirectDraw *tmp;
		HRESULT hr;
	
		DXCALL(DirectDrawCreate(0,&tmp,0));
		hr=IDirectDraw_QueryInterface(tmp,&IID_IDirectDraw2,&ddraw_object);
		if(FAILED(hr)) {
			ddraw_object=0;
		}
		IDirectDraw_Release(tmp);
		return ddraw_object;
	}
}

/*
	dx_create_surface

	What
		Creates a surface with the given width, height and capabilities.

	Inputs
		caps		Capabilities of the surface, as per DDSURFACEDESC::ddsCaps.dwCaps
		w, h		Width and height of the surface

	Output
		IDirectDrawSurface2 *, a pointer to the new surface.

	Notes
		If one or both of width and height is negative, the width and height are
		ignored; use for creating primary surface.

		dx_create_surface does not alter the reference count of the created surface.
*/
IDirectDrawSurface2 *dx_create_surface(unsigned caps,int w,int h) {
	DDSURFACEDESC ds;
	IDirectDrawSurface *surface=0;
	IDirectDrawSurface2 *result=0;
	HRESULT hr;
	memset(&ds,0,sizeof(ds));
	ds.dwSize=sizeof(ds);
	ds.dwFlags=DDSD_CAPS;
	ds.ddsCaps.dwCaps=caps;
	if(w>-1&&h>-1) {
		ds.dwFlags|=DDSD_WIDTH|DDSD_HEIGHT;
		ds.dwWidth=w;
		ds.dwHeight=h;
	}
	DXCALL(IDirectDraw_CreateSurface(dx_ddraw(),&ds,&surface,0));
	hr=IDirectDrawSurface_QueryInterface(surface,&IID_IDirectDrawSurface2,&result);
	IDirectDrawSurface_Release(surface);
	if(FAILED(hr)) {
		result=0;
	}
	return result;
}

/*
	dx_fill_area

	What
		Fills an area on a surface to a specified colour.

	Inputs
		surface				points to IDirectDrawSurface2 object describnig surface
		colour				value to write
		x1, y1				coordinates of top left-hand corner of rectangle
		x2, y2				coordinates of bottom left-hand corner of rectangle

	Output
		void

	Notes
		The area filled includes the point (x2,y2).
*/

void dx_fill_area(IDirectDrawSurface2 *surface,DWORD colour,int x1,int y1,int x2,int y2) {
	RECT r;

	if(x1<x2) {
		r.left=x1;
		r.right=x2+1;
	} else {
		r.left=x2;
		r.right=x1+1;
	}
	if(y1<y2) {
		r.top=y1;
		r.bottom=y2+1;
	} else {
		r.top=y2;
		r.bottom=y1+1;
	}
	dx_fill_rect(surface,colour,&r);
}

/*
	dx_fill_rect

	What
		Fills an area on a surface with a specified colour.

	Inputs
		surface				points to IDirectDrawSurface2 object describing surface
		colour				value to write
		r					points to RECT describing area to fill

	Outputs
		void

	Notes
		The RECT is passed unmodified, so the area filled doesn't include y==r->bottom or
		x==r->right.
*/
void dx_fill_rect(IDirectDrawSurface2 *surface,DWORD colour,RECT *r) {
	DDBLTFX fx;

	fx.dwSize=sizeof(fx);
	fx.dwFillColor=colour;
	IDirectDrawSurface2_Blt(surface,r,0,0,DDBLT_COLORFILL,&fx);
}

/*
	dx_clear_surface

	What
		Clears a surface to "black" (colour 0).

	Inputs
		surface				surface to clear
	
	Outputs
		void
*/
void dx_clear_surface(IDirectDrawSurface2 *surface) {
	dx_fill_rect(surface,0,0);
}

/*
	dx_ddraw_exit

	What
		Destroys the current DirectDraw object.

	Inputs
		void

	Outupts
		void

	Notes
		The next call to dx_ddraw will recreate the DirectDraw object.
*/
void dx_ddraw_exit(void) {
	if(ddraw_object) {
		IDirectDraw2_Release(ddraw_object);
		ddraw_object=0;
	}
}

/*
	dx_with_lock

	What
		Locks a surface, calls a specified function, then nulocks the surface.

	Inputs
		surface				surface to use
		iparam				int parameter to be passed to specified function
		pparam				void *parameter to be passed to specified function
		func				pointer to function to be called
	
	Outputs
		void

	Notes
		The signature for func is

			void (*func)(int iparam,void *pparam,DDSURFACEDESC *ds)

		iparam and pparam are as passed to dx_with_lock. ds points to DDSURFACEDESC as filled in
		by IDirectDrawSurface2::Lock
*/
void dx_with_lock(IDirectDrawSurface2 *surface,int iparam,void *pparam,void (*func)(int iparam,void *pparam,DDSURFACEDESC *ds)) {
	HRESULT hr;
	DDSURFACEDESC ds;

	ds.dwSize=sizeof(ds);
#ifdef DONT_USE_NOSYSLOCK
	hr=IDirectDrawSurface2_Lock(surface,0,&ds,DDLOCK_WAIT,0);
#else
	hr=IDirectDrawSurface2_Lock(surface,0,&ds,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,0);
#endif
	if(FAILED(hr)) {
		return;
	}
	(*func)(iparam,pparam,&ds);
	IDirectDrawSurface2_Unlock(surface,0);
}


/***** DirectInput *****/

/*
static IDirectInput *dinput_object=0;
static IDirectInputDevice *dinput_mouse=0;

IDirectInput *dx_dinput(void) {
	if(!dinput_object) {
		DXCALL(DirectInputCreate(GetModuleHandle(0),DIRECTINPUT_VERSION,&dinput_object,0));
	}
	return dinput_object;
}

IDirectInputDevice *dx_mousedevice(void) {
	if(dinput_mouse) {
		return dinput_mouse;
	} else {
		DXCALL(IDirectInput_CreateDevice(dx_dinput(),&GUID_SysMouse,0));
		IDirectInputDevice_SetDataFormat(&c_dfDIMouse);
		return dinput_mouse;
	}
}
*/