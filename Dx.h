#ifndef TOM_DX_H
#define TOM_DX_H

#include <ddraw.h>

IDirectDraw2 *dx_ddraw(void);
IDirectDrawSurface2 *dx_create_surface(unsigned caps,int w,int h);
void dx_fill_area(IDirectDrawSurface2 *surface,DWORD colour,int x1,int y1,int x2,int y2);
void dx_fill_rect(IDirectDrawSurface2 *surface,DWORD colour,RECT *r);
void dx_clear_surface(IDirectDrawSurface2 *surface);
void dx_ddraw_exit(void);
void dx_with_lock(IDirectDrawSurface2 *surface,int iparam,void *pparam,void (*func)(int iparam,void *pparam,DDSURFACEDESC *ds));

#endif