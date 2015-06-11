#include <windows.h>
#include <mmreg.h>
#include <ddraw.h>
#include <d3d.h>
#include <dsound.h>
#include <stdio.h>
#include "debug.h"

#if defined(_DEBUG) || defined(ALLOW_DPRINTF)
void dprintf(char *fmt,...) {
	va_list v;
	static char text[2048];

	va_start(v,fmt);
	_vsnprintf(text,sizeof(text),fmt,v);
	va_end(v);
	OutputDebugString(text);
}
#endif

#if defined(_DEBUG) || defined(DESCRIBE_DX_ERRORS)
char *describe_dx_error(HRESULT err_code) {
	switch(err_code)
	{
	case	DD_OK:
		return("No error");
	case	DDERR_ALREADYINITIALIZED:
		return("Object already initialised");
	case	DDERR_CANNOTATTACHSURFACE:
		return("Cannot attack surface");
	case	DDERR_CANNOTDETACHSURFACE:
		return("Cannot detach surface");
	case	DDERR_CURRENTLYNOTAVAIL:
		return("Currently Not Available");
	case	DDERR_EXCEPTION:                         
		return("Exception has occured");
	case	DDERR_GENERIC:                           
		return("Generic failure");
	case	DDERR_HEIGHTALIGN:                       
		return("Height Alignment");
	case	DDERR_INCOMPATIBLEPRIMARY:               
		return("Incompatible Primary");
	case	DDERR_INVALIDCAPS:                       
		return("Invalid Caps");
	case	DDERR_INVALIDCLIPLIST:                   
		return("Invlaid Clip List");
	case	DDERR_INVALIDMODE:                       
		return("Invalid Mode");
	case	DDERR_INVALIDOBJECT:                     
		return("Invalid Object");
	case	DDERR_INVALIDPARAMS:                     
		return("Invalid Params");
	case	DDERR_INVALIDPIXELFORMAT:
		return("Invalid Pixel Format");
	case	DDERR_INVALIDRECT:                       
		return("Invalid Rectangle");
	case	DDERR_LOCKEDSURFACES:
		return("Locked Surfaces");
	case	DDERR_NO3D:                              
		return("No 3d support present");
	case	DDERR_NOALPHAHW:                         
		return("No Alpha Hardware");
	case	DDERR_NOCLIPLIST:
		return("No Clip List");
	case	DDERR_NOCOLORCONVHW:
		return("No Colour Conversion Hardware");
	case	DDERR_NOCOOPERATIVELEVELSET:
		return("No Cooperative Level Set");
	case	DDERR_NOCOLORKEY:
		return("Surface Has No Colour Key");
	case	DDERR_NOCOLORKEYHW:
		return("No Hardware Colour Key");
	case	DDERR_NODIRECTDRAWSUPPORT:
		return("No Direct Draw Support");
	case	DDERR_NOEXCLUSIVEMODE:
		return("No Exclusive Mode");
	case	DDERR_NOFLIPHW:
		return("No Flip Hardware");
	case	DDERR_NOGDI:
		return("GDI is not present");
	case	DDERR_NOMIRRORHW:
		return("No Mirror Hardware");
	case	DDERR_NOTFOUND:
		return("Requested Item Was No Found");
	case	DDERR_NOOVERLAYHW:
		return("No Overlay Hardware");
	case	DDERR_NORASTEROPHW:
		return("No Raster Op Hardware");
	case	DDERR_NOROTATIONHW:
		return("No Roatation Hardware");
	case	DDERR_NOSTRETCHHW:
		return("No Stretch Hardware");
	case	DDERR_NOT4BITCOLOR:
		return("Not 4 Bit Colour");
	case	DDERR_NOT4BITCOLORINDEX:
		return("Not 4 Bit Colour Index");
	case	DDERR_NOT8BITCOLOR:
		return("Not 8 Bit Colour");
	case	DDERR_NOTEXTUREHW:
		return("No Texture Hardware");
	case	DDERR_NOVSYNCHW:
		return("No VSync Hardware");
	case	DDERR_NOZBUFFERHW:
		return("No Z Buffer Hardware");
	case	DDERR_NOZOVERLAYHW:
		return("No Z Overlay Hardware");
	case	DDERR_OUTOFCAPS:
		return("Hardware Has Already Been Allocated");
	case	DDERR_OUTOFMEMORY:                     
		return("Out Of Memory");
	case	DDERR_OUTOFVIDEOMEMORY:
		return("Out Of Video Memory");
	case	DDERR_OVERLAYCANTCLIP:
		return("Hardware can't clip overlays");
	case	DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
		return("Can only have colour key active at one time for overlays");
	case	DDERR_PALETTEBUSY:
		return("Palette Locked by another thread");
	case	DDERR_COLORKEYNOTSET:
		return("Colour key is not set");
	case	DDERR_SURFACEALREADYATTACHED:
		return("Surface already attached");
	case	DDERR_SURFACEALREADYDEPENDENT:
		return("Surface Already Dependent");
	case	DDERR_SURFACEBUSY:
		return("Surface Is Busy");
	case	DDERR_SURFACEISOBSCURED:
		return("Surface Is Obscured");
	case	DDERR_SURFACELOST:
		return("Surface Lost");
	case	DDERR_SURFACENOTATTACHED:
		return("Surface Not Attached");
	case	DDERR_TOOBIGHEIGHT:
		return("Height Too Big");
	case	DDERR_TOOBIGSIZE:
		return("Size Too Big - Height and Width are individually OK");
	case	DDERR_TOOBIGWIDTH:
		return("Width Too Big");
	case	DDERR_UNSUPPORTED:                    
		return("Unsupported Operation");
	case	DDERR_UNSUPPORTEDFORMAT:
		return("Unsupported Format");
	case	DDERR_UNSUPPORTEDMASK:
		return("Unsupported Mask Format");
	case	DDERR_VERTICALBLANKINPROGRESS:
		return("Vertical Blank In Progress");
	case	DDERR_WASSTILLDRAWING:
		return("Still Drawing");
	case	DDERR_XALIGN:                
		return("X Align");
	case	DDERR_INVALIDDIRECTDRAWGUID:
		return("Invalid Direct Draw GUID");
	case	DDERR_DIRECTDRAWALREADYCREATED:
		return("Direct Draw Already Created");
	case	DDERR_NODIRECTDRAWHW:          
		return("No Direct Draw HardWare");
	case	DDERR_PRIMARYSURFACEALREADYEXISTS:
		return("Primary Surface Already Exists");
	case	DDERR_NOEMULATION:
		return("No Emulation");
	case	DDERR_REGIONTOOSMALL:
		return("Region Too Small");
	case	DDERR_CLIPPERISUSINGHWND:
		return("Clipper is using a HWND");
	case	DDERR_NOCLIPPERATTACHED:
		return("No Clipper Attached");
	case	DDERR_NOHWND:
		return("No HWND set");
	case	DDERR_HWNDSUBCLASSED:
		return("HWND Sub-Classed");
	case	DDERR_HWNDALREADYSET:
		return("HWND Already Set");
	case	DDERR_NOPALETTEATTACHED:
		return("No Palette Attrached");
	case	DDERR_NOPALETTEHW:
		return("No Palette Hardware");
	case	DDERR_BLTFASTCANTCLIP:
		return("Blit Fast Can't Clip");
	case	DDERR_NOBLTHW:                           
		return("No Blit Hardware");
	case	DDERR_NODDROPSHW:
		return("No Direct Draw Raster OP Hardware");
	case	DDERR_OVERLAYNOTVISIBLE:
		return("Overlay Not Visible");
	case	DDERR_NOOVERLAYDEST:
		return("No Overlay Destination");
	case	DDERR_INVALIDPOSITION:                   
		return("Invlaid Position");
	case	DDERR_NOTAOVERLAYSURFACE:
		return("Not An Overlay Surface");
	case	DDERR_EXCLUSIVEMODEALREADYSET:
		return("Exclusive Mode Already Set");
	case	DDERR_NOTFLIPPABLE:                      
		return("Not Flippable");
	case	DDERR_CANTDUPLICATE:                     
		return("Can't Duplicate");
	case	DDERR_NOTLOCKED:                         
		return("Not Locked");
	case	DDERR_CANTCREATEDC:                      
		return("Can't Create DC");
	case	DDERR_NODC:                              
		return("No DC has ever Existed");
	case	DDERR_WRONGMODE:
		return("Wrong Mode");
	case	DDERR_IMPLICITLYCREATED:
		return("Surface Cannot Be Restored");
	case	DDERR_NOTPALETTIZED:
		return("Not Palettized");
	case	DDERR_UNSUPPORTEDMODE:
		return("Unsupported Mode");
    case	D3DERR_BADMAJORVERSION:
        return "D3DERR_BADMAJORVERSION\0";
    case	D3DERR_BADMINORVERSION:
        return "D3DERR_BADMINORVERSION\0";
    case	D3DERR_EXECUTE_LOCKED:
        return "D3DERR_EXECUTE_LOCKED\0";
    case	D3DERR_EXECUTE_NOT_LOCKED:
        return "D3DERR_EXECUTE_NOT_LOCKED\0";
    case	D3DERR_EXECUTE_CREATE_FAILED:
        return "D3DERR_EXECUTE_CREATE_FAILED\0";
    case	D3DERR_EXECUTE_DESTROY_FAILED:
        return "D3DERR_EXECUTE_DESTROY_FAILED\0";
    case	D3DERR_EXECUTE_LOCK_FAILED:
        return "D3DERR_EXECUTE_LOCK_FAILED\0";
    case	D3DERR_EXECUTE_UNLOCK_FAILED:
        return "D3DERR_EXECUTE_UNLOCK_FAILED\0";
    case	D3DERR_EXECUTE_FAILED:
        return "D3DERR_EXECUTE_FAILED\0";
    case	D3DERR_EXECUTE_CLIPPED_FAILED:
        return "D3DERR_EXECUTE_CLIPPED_FAILED\0";
    case	D3DERR_TEXTURE_NO_SUPPORT:
        return "D3DERR_TEXTURE_NO_SUPPORT\0";
    case	D3DERR_TEXTURE_NOT_LOCKED:
        return "D3DERR_TEXTURE_NOT_LOCKED\0";
    case	D3DERR_TEXTURE_LOCKED:
        return "D3DERR_TEXTURELOCKED\0";
    case	D3DERR_TEXTURE_CREATE_FAILED:
        return "D3DERR_TEXTURE_CREATE_FAILED\0";
    case	D3DERR_TEXTURE_DESTROY_FAILED:
        return "D3DERR_TEXTURE_DESTROY_FAILED\0";
    case	D3DERR_TEXTURE_LOCK_FAILED:
        return "D3DERR_TEXTURE_LOCK_FAILED\0";
    case	D3DERR_TEXTURE_UNLOCK_FAILED:
        return "D3DERR_TEXTURE_UNLOCK_FAILED\0";
    case	D3DERR_TEXTURE_LOAD_FAILED:
        return "D3DERR_TEXTURE_LOAD_FAILED\0";
    case	D3DERR_MATRIX_CREATE_FAILED:
        return "D3DERR_MATRIX_CREATE_FAILED\0";
    case	D3DERR_MATRIX_DESTROY_FAILED:
        return "D3DERR_MATRIX_DESTROY_FAILED\0";
    case	D3DERR_MATRIX_SETDATA_FAILED:
        return "D3DERR_MATRIX_SETDATA_FAILED\0";
    case	D3DERR_SETVIEWPORTDATA_FAILED:
        return "D3DERR_SETVIEWPORTDATA_FAILED\0";
    case	D3DERR_MATERIAL_CREATE_FAILED:
        return "D3DERR_MATERIAL_CREATE_FAILED\0";
    case	D3DERR_MATERIAL_DESTROY_FAILED:
        return "D3DERR_MATERIAL_DESTROY_FAILED\0";
    case	D3DERR_MATERIAL_SETDATA_FAILED:
        return "D3DERR_MATERIAL_SETDATA_FAILED\0";
    case	D3DERR_LIGHT_SET_FAILED:
        return "D3DERR_LIGHT_SET_FAILED\0";
	case E_PENDING:
		return "Data is not yet available";
//	case DS_OK:
//		return "The request completed successfully.";
	case DSERR_ALLOCATED:
		return "The request failed because resources, such as a priority level, were already in use by another caller.";
	case DSERR_ALREADYINITIALIZED:
		return "The object is already initialized.";
	case DSERR_BADFORMAT:
		return "The specified wave format is not supported.";
	case DSERR_BUFFERLOST:
		return "The buffer memory has been lost and must be restored.";
	case DSERR_CONTROLUNAVAIL:
		return "The control (volume, pan, and so forth) requested by the caller is not available.";
//	case DSERR_GENERIC:
//		return "An undetermined error occurred inside the DirectSound subsystem.";
	case DSERR_INVALIDCALL:
		return "This function is not valid for the current state of this object.";
//	case DSERR_INVALIDPARAM:
//		return "An invalid parameter was passed to the returning function.";
//	case DSERR_NOAGGREGATION:  
//		return "The object does not support aggregation.";
	case DSERR_NODRIVER:
		return "No sound driver is available for use.";
	case DSERR_OTHERAPPHASPRIO:
		return "This value is obsolete and is not used.";
//	case DSERR_OUTOFMEMORY:
//		return "The DirectSound subsystem could not allocate sufficient memory to complete the caller's request.";
	case DSERR_PRIOLEVELNEEDED:
		return "The caller does not have the priority level required for the function to succeed.";
	case DSERR_UNINITIALIZED:
		return "The IDirectSound::Initialize method has not been called or has not been called successfully before other methods were called.";
//	case DSERR_UNSUPPORTED:
//		return "The function called is not supported at this time.";
	}
	return "**UNKNOWN**";
}
#endif