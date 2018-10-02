#include <math.h>
#include "glcache.h"
#include "rend/TexCache.h"
#include "cfg/cfg.h"

#ifdef TARGET_PANDORA
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#ifndef FBIO_WAITFORVSYNC
	#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif
int fbdev = -1;
#endif

#if HOST_OS != OS_DARWIN
#include <GL3/gl3w.c>
#pragma comment(lib,"Opengl32.lib")
#endif

/*
GL|ES 2
Slower, smaller subset of gl2

*Optimisation notes*
Keep stuff in packed ints
Keep data as small as possible
Keep vertex programs as small as possible
The drivers more or less suck. Don't depend on dynamic allocation, or any 'complex' feature
as it is likely to be problematic/slow
Do we really want to enable striping joins?

*Design notes*
Follow same architecture as the d3d renderer for now
Render to texture, keep track of textures in GL memory
Direct flip to screen (no vlbank/fb emulation)
Do we really need a combining shader? it is needlessly expensive for openGL | ES
Render contexts
Free over time? we actually care about ram usage here?
Limit max resource size? for psp 48k verts worked just fine

FB:
Pixel clip, mapping

SPG/VO:
mapping

TA:
Tile clip

*/

#include "oslib/oslib.h"
#include "rend/rend.h"
#include "hw/pvr/Renderer_if.h"

float fb_scale_x,fb_scale_y;
float scale_x, scale_y;

//Fragment and vertex shaders code

const char* VertexShaderSource =
"\
#version 140 \n\
#define pp_Gouraud %d \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp float     extra_depth_scale; \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
in lowp vec4     in_base1; \n\
in lowp vec4     in_offs1; \n\
in mediump vec2  in_uv1; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
			  out mediump vec2 vtx_uv; \n\
INTERPOLATION out lowp vec4 vtx_base1; \n\
INTERPOLATION out lowp vec4 vtx_offs1; \n\
			  out mediump vec2 vtx_uv1; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	vtx_base1 = in_base1; \n\
	vtx_offs1 = in_offs1; \n\
	vtx_uv1 = in_uv1; \n\
	vec4 vpos=in_pos; \n\
	if (isinf(vpos.z)) \n\
		vpos.w = 1.18e-38; \n\
	else \n\
		vpos.w = extra_depth_scale / vpos.z; \n\
	if (vpos.w < 0.0) { \n\
		gl_Position = vec4(0.0, 0.0, 0.0, vpos.w); \n\
		return; \n\
	} \n\
	vpos.z = vpos.w; \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* PixelPipelineShader = SHADER_HEADER
"\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_TwoVolumes %d \n\
#define pp_DepthFunc %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define PASS %d \n\
#define PI 3.1415926 \n\
 \n\
#if PASS <= 1 \n\
out vec4 FragColor; \n\
#endif \n\
 \n\
#if pp_TwoVolumes == 1 \n\
#define IF(x) if (x) \n\
#else \n\
#define IF(x) \n\
#endif \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform highp float shade_scale_factor; \n\
uniform sampler2D tex0, tex1; \n\
layout(binding = 5) uniform sampler2D fog_table; \n\
uniform int pp_Number; \n\
uniform usampler2D shadow_stencil; \n\
uniform sampler2D DepthTex; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
 \n\
uniform ivec2 blend_mode[2]; \n\
#if pp_TwoVolumes == 1 \n\
uniform bool use_alpha[2]; \n\
uniform bool ignore_tex_alpha[2]; \n\
uniform int shading_instr[2]; \n\
uniform int fog_control[2]; \n\
#endif \n\
 \n\
uniform highp float extra_depth_scale; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
INTERPOLATION in lowp vec4 vtx_base1; \n\
INTERPOLATION in lowp vec4 vtx_offs1; \n\
			  in mediump vec2 vtx_uv1; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
	highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
	highp float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	float idx = floor(m) + exp * 16.0 + 0.5; \n\
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.r; \n\
} \n\
 \n\
highp vec4 fog_clamp(highp vec4 col) \n\
{ \n\
#if FogClamping == 1 \n\
	return clamp(col, fog_clamp_min, fog_clamp_max); \n\
#else \n\
	return col; \n\
#endif \n\
} \n\
 \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
	#if PASS == 3 \n\
		// Manual depth testing \n\
		highp float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r; \n\
		#if pp_DepthFunc == 0		// Never \n\
			discard; \n\
		#elif pp_DepthFunc == 1		// Greater \n\
			if (gl_FragDepth <= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 2		// Equal \n\
			if (gl_FragDepth != frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 3		// Greater or equal \n\
			if (gl_FragDepth < frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 4		// Less \n\
			if (gl_FragDepth >= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 5		// Not equal \n\
			if (gl_FragDepth == frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 6		// Less or equal \n\
			if (gl_FragDepth > frontDepth) \n\
				discard; \n\
		#endif \n\
	#endif \n\
	\n\
	// Clip outside the box \n\
	#if pp_ClipTestMode==1 \n\
		if (gl_FragCoord.x < pp_ClipTest.x || gl_FragCoord.x > pp_ClipTest.z \n\
				|| gl_FragCoord.y < pp_ClipTest.y || gl_FragCoord.y > pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	// Clip inside the box \n\
	#if pp_ClipTestMode==-1 \n\
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z \n\
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	\n\
	highp vec4 color = vtx_base; \n\
	lowp vec4 offset = vtx_offs; \n\
	mediump vec2 uv = vtx_uv; \n\
	bool area1 = false; \n\
	ivec2 cur_blend_mode = blend_mode[0]; \n\
	\n\
	#if pp_TwoVolumes == 1 \n\
		bool cur_use_alpha = use_alpha[0]; \n\
		bool cur_ignore_tex_alpha = ignore_tex_alpha[0]; \n\
		int cur_shading_instr = shading_instr[0]; \n\
		int cur_fog_control = fog_control[0]; \n\
		#if PASS == 1 \n\
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
			if (stencil.r == 0x81u) { \n\
				color = vtx_base1; \n\
				offset = vtx_offs1; \n\
				uv = vtx_uv1; \n\
				area1 = true; \n\
				cur_blend_mode = blend_mode[1]; \n\
				cur_use_alpha = use_alpha[1]; \n\
				cur_ignore_tex_alpha = ignore_tex_alpha[1]; \n\
				cur_shading_instr = shading_instr[1]; \n\
				cur_fog_control = fog_control[1]; \n\
			} \n\
		#endif\n\
	#endif\n\
	\n\
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1 \n\
		IF(!cur_use_alpha) \n\
			color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2 \n\
		IF(cur_fog_control == 3) \n\
			color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		highp vec4 texcol; \n\
		if (area1) \n\
			texcol = texture(tex1, uv); \n\
		else \n\
			texcol = texture(tex0, uv); \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1 \n\
				IF(cur_ignore_tex_alpha) \n\
					texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif\n\
		#if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL \n\
		IF(cur_shading_instr == 0) \n\
		{ \n\
			color=texcol; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE \n\
		IF(cur_shading_instr == 1) \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA \n\
		IF(cur_shading_instr == 2) \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA \n\
		IF(cur_shading_instr == 3) \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb += offset.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	#if PASS == 1 && pp_TwoVolumes == 0 \n\
		uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
		if (stencil.r == 0x81u) \n\
			color.rgb *= shade_scale_factor; \n\
	#endif \n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT \n\
		IF(cur_fog_control == 0) \n\
		{ \n\
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
		} \n\
	#endif\n\
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex \n\
		IF(cur_fog_control == 1) \n\
		{ \n\
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a); \n\
		} \n\
	#endif\n\
	 \n\
	color *= trilinear_alpha; \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	\n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0); \n\
	\n\
	#if PASS == 1  \n\
		FragColor = color; \n\
	#elif PASS > 1 \n\
		// Discard as many pixels as possible \n\
		switch (cur_blend_mode.y) // DST \n\
		{ \n\
		case ONE: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
					discard; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
				case INVERSE_SRC_ALPHA: \n\
					if (color.a == 1.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		case OTHER_COLOR: \n\
			if (cur_blend_mode.x == ZERO && color == vec4(1.0)) \n\
				discard; \n\
			break; \n\
		case INVERSE_OTHER_COLOR: \n\
			if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0)) \n\
				discard; \n\
			break; \n\
		case SRC_ALPHA: \n\
			if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0) \n\
				discard; \n\
			break; \n\
		case INVERSE_SRC_ALPHA: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0) \n\
						discard; \n\
					break; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		} \n\
		\n\
		ivec2 coords = ivec2(gl_FragCoord.xy); \n\
		uint idx =  getNextPixelIndex(); \n\
		 \n\
		Pixel pixel; \n\
		pixel.color = color; \n\
		pixel.depth = gl_FragDepth; \n\
		pixel.seq_num = uint(pp_Number); \n\
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx); \n\
		pixels[idx] = pixel; \n\
		\n\
		discard; \n\
		\n\
	#endif \n\
}";

const char* ModifierVolumeShader = SHADER_HEADER
" \
/* Vertex input*/ \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
}";

const char* OSD_Shader =
" \
#version 140 \n\
out vec4 FragColor; \n\
 \n\
smooth in lowp vec4 vtx_base; \n\
       in mediump vec2 vtx_uv; \n\
/* Vertex input*/ \n\
uniform sampler2D tex; \n\
void main() \n\
{ \n\
	mediump vec2 uv = vtx_uv; \n\
	uv.y = 1.0 - uv.y; \n\
	FragColor = vtx_base * texture(tex, uv.st); \n\n\
}";

GLCache glcache;
gl_ctx gl;

int screen_width;
int screen_height;
GLuint fogTextureId;

#if (HOST_OS != OS_DARWIN) && !defined(TARGET_NACL32)

#if HOST_OS == OS_WINDOWS
	#define WGL_DRAW_TO_WINDOW_ARB         0x2001
	#define WGL_ACCELERATION_ARB           0x2003
	#define WGL_SWAP_METHOD_ARB            0x2007
	#define WGL_SUPPORT_OPENGL_ARB         0x2010
	#define WGL_DOUBLE_BUFFER_ARB          0x2011
	#define WGL_PIXEL_TYPE_ARB             0x2013
	#define WGL_COLOR_BITS_ARB             0x2014
	#define WGL_DEPTH_BITS_ARB             0x2022
	#define WGL_STENCIL_BITS_ARB           0x2023
	#define WGL_FULL_ACCELERATION_ARB      0x2027
	#define WGL_SWAP_EXCHANGE_ARB          0x2028
	#define WGL_TYPE_RGBA_ARB              0x202B
	#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
	#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
	#define WGL_CONTEXT_FLAGS_ARB              0x2094

	#define		WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
	#define 	WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
	#define 	WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
	#define 	WGL_CONTEXT_LAYER_PLANE_ARB   0x2093
	#define 	WGL_CONTEXT_FLAGS_ARB   0x2094
	#define 	WGL_CONTEXT_DEBUG_BIT_ARB   0x0001
	#define 	WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB   0x0002
	#define 	ERROR_INVALID_VERSION_ARB   0x2095
	#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

	typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats,
															int *piFormats, UINT *nNumFormats);
	typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
	typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;


	HDC ourWindowHandleToDeviceContext;
	bool gl_init(void* hwnd, void* hdc)
	{
		PIXELFORMATDESCRIPTOR pfd =
		{
				sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
				PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
				32,                        //Colordepth of the framebuffer.
				0, 0, 0, 0, 0, 0,
				0,
				0,
				0,
				0, 0, 0, 0,
				24,                        //Number of bits for the depthbuffer
				8,                        //Number of bits for the stencilbuffer
				0,                        //Number of Aux buffers in the framebuffer.
				PFD_MAIN_PLANE,
				0,
				0, 0, 0
		};

		/*HDC*/ ourWindowHandleToDeviceContext = (HDC)hdc;//GetDC((HWND)hwnd);

		int  letWindowsChooseThisPixelFormat;
		letWindowsChooseThisPixelFormat = ChoosePixelFormat(ourWindowHandleToDeviceContext, &pfd);
		SetPixelFormat(ourWindowHandleToDeviceContext,letWindowsChooseThisPixelFormat, &pfd);

		HGLRC ourOpenGLRenderingContext = wglCreateContext(ourWindowHandleToDeviceContext);
		wglMakeCurrent (ourWindowHandleToDeviceContext, ourOpenGLRenderingContext);

		bool rv = true;

		if (rv) {

			wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
			if(!wglChoosePixelFormatARB)
			{
				return false;
			}

			wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			if(!wglCreateContextAttribsARB)
			{
				return false;
			}

			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
			if(!wglSwapIntervalEXT)
			{
				return false;
			}

			int attribs[] =
		   {
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 1,
				WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				0
		   };

			HGLRC m_hrc = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext,0, attribs);

			if (m_hrc)
				wglMakeCurrent(ourWindowHandleToDeviceContext,m_hrc);
			else
				rv = false;

			wglDeleteContext(ourOpenGLRenderingContext);
		}

		if (rv) {
			rv = gl3wInit() != -1 && gl3wIsSupported(3, 1);
		}

		RECT r;
		GetClientRect((HWND)hwnd, &r);
		screen_width = r.right - r.left;
		screen_height = r.bottom - r.top;

		return rv;
	}
	#include <Wingdi.h>
	void gl_swap()
	{
		wglSwapLayerBuffers(ourWindowHandleToDeviceContext,WGL_SWAP_MAIN_PLANE);
		//SwapBuffers(ourWindowHandleToDeviceContext);
	}
#else
	#if defined(SUPPORT_X11)
		//! windows && X11
		//let's assume glx for now

		#include <X11/X.h>
		#include <X11/Xlib.h>
		#include <GL/gl.h>
		#include <GL/glx.h>


		bool gl_init(void* wind, void* disp)
		{
			extern void* x11_glc;

			glXMakeCurrent((Display*)libPvr_GetRenderSurface(),
				(GLXDrawable)libPvr_GetRenderTarget(),
				(GLXContext)x11_glc);

			screen_width = 640;
			screen_height = 480;
			return gl3wInit() != -1 && gl3wIsSupported(3, 1);
		}

		void gl_swap()
		{
			glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());

			Window win;
			int temp;
			unsigned int tempu, new_w, new_h;
			XGetGeometry((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget(),
						&win, &temp, &temp, &new_w, &new_h,&tempu,&tempu);

			//if resized, clear up the draw buffers, to avoid out-of-draw-area junk data
			if (new_w != screen_width || new_h != screen_height) {
				screen_width = new_w;
				screen_height = new_h;
			}

			#if 0
				//handy to debug really stupid render-not-working issues ...

				glcache.ClearColor( 0, 0.5, 1, 1 );
				glClear( GL_COLOR_BUFFER_BIT );
				glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());


				glcache.ClearColor ( 1, 0.5, 0, 1 );
				glClear ( GL_COLOR_BUFFER_BIT );
				glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());
			#endif
		}
	#endif
#endif

#endif

struct ShaderUniforms_t ShaderUniforms;

GLuint gl_CompileShader(const char* shader,GLuint type)
{
	GLint result;
	GLint compile_log_len;
	GLuint rv=glCreateShader(type);
	glShaderSource(rv, 1,&shader, NULL);
	glCompileShader(rv);

	//lets see if it compiled ...
	glGetShaderiv(rv, GL_COMPILE_STATUS, &result);
	glGetShaderiv(rv, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		if (compile_log_len==0)
			compile_log_len=1;
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetShaderInfoLog(rv, compile_log_len, &compile_log_len, compile_log);
		printf("Shader: %s \n%s\n",result?"compiled!":"failed to compile",compile_log);

		free(compile_log);
	}

	return rv;
}

GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader)
{
	//create shaders
	GLuint vs=gl_CompileShader(VertexShader, GL_VERTEX_SHADER);
	GLuint ps=gl_CompileShader(FragmentShader, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, ps);

	//bind vertex attribute to vbo inputs
	glBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
	glBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
	glBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
	glBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");
	glBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
	glBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
	glBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");

	glBindFragDataLocation(program, 0, "FragColor");

	glLinkProgram(program);

	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);


	GLint compile_log_len;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		if (compile_log_len==0)
			compile_log_len=1;
		compile_log_len+= 1024;
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetProgramInfoLog(program, compile_log_len, &compile_log_len, compile_log);
		printf("Shader linking: %s \n (%d bytes), - %s -\n",result?"linked":"failed to link", compile_log_len,compile_log);

		free(compile_log);

		// Dump the shaders source for troubleshooting
		printf("// VERTEX SHADER\n%s\n// END\n", VertexShader);
		printf("// FRAGMENT SHADER\n%s\n// END\n", FragmentShader);
		die("shader compile fail\n");
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	verify(glIsProgram(program));

	return program;
}

int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_TwoVolumes, u32 pp_DepthFunc, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, int pass)
{
	u32 rv=0;

	rv|=pp_ClipTestMode;
	rv<<=1; rv|=cp_AlphaTest;
	rv<<=1; rv|=pp_Texture;
	rv<<=1; rv|=pp_UseAlpha;
	rv<<=1; rv|=pp_IgnoreTexA;
	rv<<=2; rv|=pp_ShadInstr;
	rv<<=1; rv|=pp_Offset;
	rv<<=2; rv|=pp_FogCtrl;
	rv <<= 1; rv |= (int)pp_TwoVolumes;
	rv <<= 3; rv |= pp_DepthFunc;
	rv <<= 1; rv |= (int)pp_Gouraud;
	rv <<= 1; rv |= pp_BumpMap;
	rv <<= 1; rv |= fog_clamping;
	rv <<= 2; rv |= pass;

	return rv;
}

bool CompilePipelineShader(	PipelineShader* s, const char *source /* = PixelPipelineShader */)
{
	char vshader[16384];

	sprintf(vshader, VertexShaderSource, s->pp_Gouraud);

	char pshader[16384];

	sprintf(pshader, source,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_TwoVolumes, s->pp_DepthFunc, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, s->pass);

	s->program = gl_CompileAndLink(vshader, pshader);

	//setup texture 0 as the input for the shader
	GLint gu = glGetUniformLocation(s->program, "tex0");
	if (s->pp_Texture == 1 && gu != -1)
		glUniform1i(gu, 0);
	// Setup texture 1 as the input for area 1 in two volume mode
	gu = glGetUniformLocation(s->program, "tex1");
	if (s->pp_Texture == 1 && gu != -1)
		glUniform1i(gu, 1);

	//get the uniform locations
	s->scale	            = glGetUniformLocation(s->program, "scale");
	s->extra_depth_scale = glGetUniformLocation(s->program, "extra_depth_scale");

	s->pp_ClipTest      = glGetUniformLocation(s->program, "pp_ClipTest");

	s->sp_FOG_DENSITY   = glGetUniformLocation(s->program, "sp_FOG_DENSITY");

	s->cp_AlphaTestValue= glGetUniformLocation(s->program, "cp_AlphaTestValue");

	//FOG_COL_RAM,FOG_COL_VERT,FOG_DENSITY;
	if (s->pp_FogCtrl==1 && s->pp_Texture==1)
		s->sp_FOG_COL_VERT=glGetUniformLocation(s->program, "sp_FOG_COL_VERT");
	else
		s->sp_FOG_COL_VERT=-1;
	if (s->pp_FogCtrl==0 || s->pp_FogCtrl==3)
	{
		s->sp_FOG_COL_RAM=glGetUniformLocation(s->program, "sp_FOG_COL_RAM");
	}
	else
	{
		s->sp_FOG_COL_RAM=-1;
	}
	s->shade_scale_factor = glGetUniformLocation(s->program, "shade_scale_factor");

	// Use texture 1 for depth texture
	gu = glGetUniformLocation(s->program, "DepthTex");
	if (gu != -1)
		glUniform1i(gu, 2);		// GL_TEXTURE2

	s->trilinear_alpha = glGetUniformLocation(s->program, "trilinear_alpha");
	
	if (s->fog_clamping)
	{
		s->fog_clamp_min = glGetUniformLocation(s->program, "fog_clamp_min");
		s->fog_clamp_max = glGetUniformLocation(s->program, "fog_clamp_max");
	}
	else
	{
		s->fog_clamp_min = -1;
		s->fog_clamp_max = -1;
	}

	// Shadow stencil for OP/PT rendering pass
	gu = glGetUniformLocation(s->program, "shadow_stencil");
	if (gu != -1)
		glUniform1i(gu, 3);		// GL_TEXTURE3

	s->pp_Number = glGetUniformLocation(s->program, "pp_Number");

	s->blend_mode = glGetUniformLocation(s->program, "blend_mode");
	s->use_alpha = glGetUniformLocation(s->program, "use_alpha");
	s->ignore_tex_alpha = glGetUniformLocation(s->program, "ignore_tex_alpha");
	s->shading_instr = glGetUniformLocation(s->program, "shading_instr");
	s->fog_control = glGetUniformLocation(s->program, "fog_control");

	return glIsProgram(s->program)==GL_TRUE;
}

GLuint osd_tex;
GLuint osd_font;

bool gl_create_resources()
{

	//create vao
	//This is really not "proper", vaos are supposed to be defined once
	//i keep updating the same one to make the es2 code work in 3.1 context
	glGenVertexArrays(1, &gl.vbo.vao);

	//create vbos
	glGenBuffers(1, &gl.vbo.geometry);
	glGenBuffers(1, &gl.vbo.modvols);
	glGenBuffers(1, &gl.vbo.idxs);
	glGenBuffers(1, &gl.vbo.idxs2);

	char vshader[16384];
	sprintf(vshader, VertexShaderSource, 1);

	gl.modvol_shader.program=gl_CompileAndLink(vshader, ModifierVolumeShader);
	gl.modvol_shader.scale          = glGetUniformLocation(gl.modvol_shader.program, "scale");
	gl.modvol_shader.extra_depth_scale = glGetUniformLocation(gl.modvol_shader.program, "extra_depth_scale");


	gl.OSD_SHADER.program=gl_CompileAndLink(vshader, OSD_Shader);
	gl.OSD_SHADER.scale=glGetUniformLocation(gl.OSD_SHADER.program, "scale");
	gl.OSD_SHADER.extra_depth_scale = glGetUniformLocation(gl.OSD_SHADER.program, "extra_depth_scale");
	glUniform1i(glGetUniformLocation(gl.OSD_SHADER.program, "tex"),0);		//bind osd texture to slot 0

	//#define PRECOMPILE_SHADERS
	#ifdef PRECOMPILE_SHADERS
	for (u32 i=0;i<sizeof(gl.pogram_table)/sizeof(gl.pogram_table[0]);i++)
	{
		if (!CompilePipelineShader(	&gl.pogram_table[i] ))
			return false;
	}
	#endif

	int w, h;
	osd_tex=loadPNG(get_readonly_data_path("/data/buttons.png"),w,h);
#ifdef TARGET_PANDORA
	osd_font=loadPNG(get_readonly_data_path("/font2.png"),w,h);
#else
	osd_font = loadPNG(get_readonly_data_path("/pixmaps/font.png"), w, h);
	if (osd_font == 0)
		osd_font = loadPNG(get_readonly_data_path("/font.png"), w, h);
#endif

	// Create the buffer for Translucent poly params
	glGenBuffers(1, &gl.vbo.tr_poly_params);
	// Bind it
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl.vbo.tr_poly_params);
	// Declare storage
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gl.vbo.tr_poly_params);
	glCheck();

	return true;
}

bool gl_init(void* wind, void* disp);

//swap buffers
void gl_swap();

GLuint gl_CompileShader(const char* shader,GLuint type);

bool gl_create_resources();

//setup
extern void initABuffer();

void gl_DebugOutput(GLenum source,
                            GLenum type,
                            GLuint id,
                            GLenum severity,
                            GLsizei length,
                            const GLchar *message,
                            void *userParam)
{
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
    if (id == 131186)
    	// Warning when fetching the atomic_uint pixel count
    	return;

    printf("OpenGL Debug message (%d): %s\n", id, message);

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             printf("Source: API"); break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   printf("Source: Window System"); break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: printf("Source: Shader Compiler"); break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     printf("Source: Third Party"); break;
        case GL_DEBUG_SOURCE_APPLICATION:     printf("Source: Application"); break;
        case GL_DEBUG_SOURCE_OTHER:           printf("Source: Other"); break;
    }
    printf(" ");

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               printf("Type: Error"); break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: printf("Type: Deprecated Behaviour"); break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  printf("Type: Undefined Behaviour"); break;
        case GL_DEBUG_TYPE_PORTABILITY:         printf("Type: Portability"); break;
        case GL_DEBUG_TYPE_PERFORMANCE:         printf("Type: Performance"); break;
        case GL_DEBUG_TYPE_MARKER:              printf("Type: Marker"); break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          printf("Type: Push Group"); break;
        case GL_DEBUG_TYPE_POP_GROUP:           printf("Type: Pop Group"); break;
        case GL_DEBUG_TYPE_OTHER:               printf("Type: Other"); break;
    }
    printf(" ");

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         printf("Severity: high"); break;
        case GL_DEBUG_SEVERITY_MEDIUM:       printf("Severity: medium"); break;
        case GL_DEBUG_SEVERITY_LOW:          printf("Severity: low"); break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: printf("Severity: notification"); break;
    };
    printf("\n");
}

bool gles_init()
{

	if (!gl_init((void*)libPvr_GetRenderTarget(),
		         (void*)libPvr_GetRenderSurface()))
			return false;

	if (!gl_create_resources())
		return false;

//    glEnable(GL_DEBUG_OUTPUT);
//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//    glDebugMessageCallback(gl_DebugOutput, NULL);
//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);


	//clean up the buffer
	glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	gl_swap();

	initABuffer();

	if (settings.rend.TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}

	return true;
}


void UpdateFogTexture(u8 *fog_table)
{
	glActiveTexture(GL_TEXTURE5);
	if (fogTextureId == 0)
	{
		fogTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);

	u8 temp_tex_buffer[256];
	for (int i = 0; i < 128; i++)
	{
		temp_tex_buffer[i] = fog_table[i * 4];
		temp_tex_buffer[i + 128] = fog_table[i * 4 + 1];
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 128, 2, 0, GL_RED, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}


extern u16 kcode[4];
extern u8 rt[4],lt[4];

#define key_CONT_C           (1 << 0)
#define key_CONT_B           (1 << 1)
#define key_CONT_A           (1 << 2)
#define key_CONT_START       (1 << 3)
#define key_CONT_DPAD_UP     (1 << 4)
#define key_CONT_DPAD_DOWN   (1 << 5)
#define key_CONT_DPAD_LEFT   (1 << 6)
#define key_CONT_DPAD_RIGHT  (1 << 7)
#define key_CONT_Z           (1 << 8)
#define key_CONT_Y           (1 << 9)
#define key_CONT_X           (1 << 10)
#define key_CONT_D           (1 << 11)
#define key_CONT_DPAD2_UP    (1 << 12)
#define key_CONT_DPAD2_DOWN  (1 << 13)
#define key_CONT_DPAD2_LEFT  (1 << 14)
#define key_CONT_DPAD2_RIGHT (1 << 15)

u32 osd_base;
u32 osd_count;


#if defined(_ANDROID)
extern float vjoy_pos[14][8];
#else

float vjoy_pos[14][8]=
{
	{24+0,24+64,64,64},     //LEFT
	{24+64,24+0,64,64},     //UP
	{24+128,24+64,64,64},   //RIGHT
	{24+64,24+128,64,64},   //DOWN

	{440+0,280+64,64,64},   //X
	{440+64,280+0,64,64},   //Y
	{440+128,280+64,64,64}, //B
	{440+64,280+128,64,64}, //A

	{320-32,360+32,64,64},  //Start

	{440,200,90,64},        //RT
	{542,200,90,64},        //LT

	{-24,128+224,128,128},  //ANALOG_RING
	{96,320,64,64},         //ANALOG_POINT
	{1}
};
#endif // !_ANDROID

float vjoy_sz[2][14] = {
	{ 64,64,64,64, 64,64,64,64, 64, 90,90, 128, 64 },
	{ 64,64,64,64, 64,64,64,64, 64, 64,64, 128, 64 },
};

static void DrawButton(float* xy, u32 state)
{
	Vertex vtx;

	vtx.z=1;

	float x=xy[0];
	float y=xy[1];
	float w=xy[2];
	float h=xy[3];

	vtx.col[0]=vtx.col[1]=vtx.col[2]=(0x7F-0x40*state/255)*vjoy_pos[13][0];

	vtx.col[3]=0xA0*vjoy_pos[13][4];

	vjoy_pos[13][4]+=(vjoy_pos[13][0]-vjoy_pos[13][4])/2;



	vtx.x=x; vtx.y=y;
	vtx.u=xy[4]; vtx.v=xy[5];
	*pvrrc.verts.Append()=vtx;

	vtx.x=x+w; vtx.y=y;
	vtx.u=xy[6]; vtx.v=xy[5];
	*pvrrc.verts.Append()=vtx;

	vtx.x=x; vtx.y=y+h;
	vtx.u=xy[4]; vtx.v=xy[7];
	*pvrrc.verts.Append()=vtx;

	vtx.x=x+w; vtx.y=y+h;
	vtx.u=xy[6]; vtx.v=xy[7];
	*pvrrc.verts.Append()=vtx;

	osd_count+=4;
}

void DrawButton2(float* xy, bool state) { DrawButton(xy,state?0:255); }

static void DrawCenteredText(float yy, float scale, int transparency, const char* text)
// Draw a centered text. Font is loaded from font2.png file. Each char is 16*16 size, so scale it down so it's not too big
// Transparency 255=opaque, 0=not visible
{
  Vertex vtx;

  vtx.z=1;

  float w=float(strlen(text)*14)*scale;

  float x=320-w/2.0f;
  float y=yy;
  float h=16.0f*scale;
  w=14.0f*scale;
  float step=32.0f/512.0f;
  float step2=4.0f/512.0f;

  if (transparency<0) transparency=0;
  if (transparency>255) transparency=255;

  for (int i=0; i<strlen(text); i++) {
    int c=text[i];
    float u=float(c%16);
    float v=float(c/16);

    vtx.col[0]=vtx.col[1]=vtx.col[2]=255;
    vtx.col[3]=transparency;

    vtx.x=x; vtx.y=y;
    vtx.u=u*step+step2; vtx.v=v*step+step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x+w; vtx.y=y;
    vtx.u=u*step+step-step2; vtx.v=v*step+step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x; vtx.y=y+h;
    vtx.u=u*step+step2; vtx.v=v*step+step-step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x+w; vtx.y=y+h;
    vtx.u=u*step+step-step2; vtx.v=v*step+step-step2;
    *pvrrc.verts.Append()=vtx;

    x+=w;

    osd_count+=4;
  }
}
static void DrawRightedText(float yy, float scale, int transparency, const char* text)
// Draw a text right justified. Font is loaded from font.png file. Each char is 16*16 size, so scale it down so it's not too big
// Transparency 255=opaque, 0=not visible
{
  Vertex vtx;

  vtx.z=1;

  float w=float(strlen(text)*14)*scale;

  float x = (ShaderUniforms.scale_coefs[2] + 1) / ShaderUniforms.scale_coefs[0] - w;
  float y=yy;
  float h=16.0f*scale;
  w=14.0f*scale;
  float step=32.0f/512.0f;
  float step2=4.0f/512.0f;

  if (transparency<0) transparency=0;
  if (transparency>255) transparency=255;

  for (int i=0; i<strlen(text); i++) {
    int c=text[i];
    float u=float(c%16);
    float v=float(c/16);

    vtx.col[0]=vtx.col[1]=vtx.col[2]=255;
    vtx.col[3]=transparency;

    vtx.x=x; vtx.y=y;
    vtx.u=u*step+step2; vtx.v=v*step+step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x+w; vtx.y=y;
    vtx.u=u*step+step-step2; vtx.v=v*step+step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x; vtx.y=y+h;
    vtx.u=u*step+step2; vtx.v=v*step+step-step2;
    *pvrrc.verts.Append()=vtx;

    vtx.x=x+w; vtx.y=y+h;
    vtx.u=u*step+step-step2; vtx.v=v*step+step-step2;
    *pvrrc.verts.Append()=vtx;

    x+=w;

    osd_count+=4;
  }
}

#ifdef TARGET_PANDORA
char OSD_Info[128];
int  OSD_Delay=0;
char OSD_Counters[256];
int  OSD_Counter=0;
#endif

static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static void OSD_HOOK()
{
	osd_base=pvrrc.verts.used();
	osd_count=0;

	#ifndef TARGET_PANDORA
	if (osd_tex)
	{
		DrawButton2(vjoy_pos[0],kcode[0]&key_CONT_DPAD_LEFT);
		DrawButton2(vjoy_pos[1],kcode[0]&key_CONT_DPAD_UP);
		DrawButton2(vjoy_pos[2],kcode[0]&key_CONT_DPAD_RIGHT);
		DrawButton2(vjoy_pos[3],kcode[0]&key_CONT_DPAD_DOWN);

		DrawButton2(vjoy_pos[4],kcode[0]&key_CONT_X);
		DrawButton2(vjoy_pos[5],kcode[0]&key_CONT_Y);
		DrawButton2(vjoy_pos[6],kcode[0]&key_CONT_B);
		DrawButton2(vjoy_pos[7],kcode[0]&key_CONT_A);

		DrawButton2(vjoy_pos[8],kcode[0]&key_CONT_START);

		DrawButton(vjoy_pos[9],lt[0]);

		DrawButton(vjoy_pos[10],rt[0]);

		DrawButton2(vjoy_pos[11],1);
		DrawButton2(vjoy_pos[12],0);
	}
	#endif
	#ifdef TARGET_PANDORA
	  if (OSD_Delay) {
		DrawCenteredText(400, 1.0f, (OSD_Delay<255)?OSD_Delay:255, OSD_Info);
		OSD_Delay--;    //*TODO* Delay should be in ms, not in ticks...
	  }
	  if (OSD_Counter) {
		DrawRightedText(20, 1.0f, 255, OSD_Counters);
	  }
	#endif

	if (settings.rend.ShowFPS) {
		if (os_GetSeconds() - LastFPSTime > 0.5) {
			fps = (FrameCount - lastFrameCount) / (os_GetSeconds() - LastFPSTime);
			LastFPSTime = os_GetSeconds();
			lastFrameCount = FrameCount;
		}
		if (fps >= 0) {
			char text[32];
			sprintf(text, "F:%.1f", fps);
			DrawRightedText(460, 1.f, 196, text);
		}
	}
}

#define OSD_TEX_W 512
#define OSD_TEX_H 256

void OSD_DRAW()
{
	#ifndef TARGET_PANDORA
	if (osd_tex)
	{
		float u=0;
		float v=0;

		for (int i=0;i<13;i++)
		{
			//umin,vmin,umax,vmax
			vjoy_pos[i][4]=(u+1)/OSD_TEX_W;
			vjoy_pos[i][5]=(v+1)/OSD_TEX_H;

			vjoy_pos[i][6]=((u+vjoy_sz[0][i]-1))/OSD_TEX_W;
			vjoy_pos[i][7]=((v+vjoy_sz[1][i]-1))/OSD_TEX_H;

			u+=vjoy_sz[0][i];
			if (u>=OSD_TEX_W)
			{
				u-=OSD_TEX_W;
				v+=vjoy_sz[1][i];
			}
			//v+=vjoy_pos[i][3];
		}

		verify(glIsProgram(gl.OSD_SHADER.program));

		glcache.BindTexture(GL_TEXTURE_2D, osd_tex);
		glcache.UseProgram(gl.OSD_SHADER.program);

		//reset rendering scale
/*
		float dc_width=640;
		float dc_height=480;

		float dc2s_scale_h=screen_height/480.0f;
		float ds2s_offs_x=(screen_width-dc2s_scale_h*640)/2;

		//-1 -> too much to left
		ShaderUniforms.scale_coefs[0]=2.0f/(screen_width/dc2s_scale_h);
		ShaderUniforms.scale_coefs[1]=-2/dc_height;
		ShaderUniforms.scale_coefs[2]=1-2*ds2s_offs_x/(screen_width);
		ShaderUniforms.scale_coefs[3]=-1;

		glUniform4fv( gl.OSD_SHADER.scale, 1, ShaderUniforms.scale_coefs);
*/

		glcache.Enable(GL_BLEND);
		glcache.Disable(GL_DEPTH_TEST);
		glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glcache.DepthMask(false);
		glcache.DepthFunc(GL_ALWAYS);

		glcache.Disable(GL_CULL_FACE);
		glcache.Disable(GL_SCISSOR_TEST);

		int dfa=osd_count/4;

		for (int i=0;i<dfa;i++)
			glDrawArrays(GL_TRIANGLE_STRIP,osd_base+i*4,4);
	}
#endif
  if (osd_font)
  {
    verify(glIsProgram(gl.OSD_SHADER.program));

    glcache.BindTexture(GL_TEXTURE_2D,osd_font);
    glcache.UseProgram(gl.OSD_SHADER.program);

    glcache.Enable(GL_BLEND);
    glcache.Disable(GL_DEPTH_TEST);
    glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glcache.DepthMask(false);
    glcache.DepthFunc(GL_ALWAYS);

    glcache.Disable(GL_CULL_FACE);
    glcache.Disable(GL_SCISSOR_TEST);

    int dfa=osd_count/4;

   	for (int i=0;i<dfa;i++)
		glDrawArrays(GL_TRIANGLE_STRIP,osd_base+i*4,4);
 }
}

bool ProcessFrame(TA_context* ctx)
{
	ctx->rend_inuse.Lock();

	if (KillTex)
	{
		void killtex();
		killtex();
		printf("Texture cache cleared\n");
	}

	if (ctx->rend.isRenderFramebuffer)
	{
		RenderFramebuffer();
		ctx->rend_inuse.Unlock();
	}
	else
	{
		if (!ta_parse_vdrc(ctx))
			return false;
	}
	CollectCleanup();

	if (ctx->rend.Overrun)
		printf("ERROR: TA context overrun\n");

	return !ctx->rend.Overrun;
}

bool RenderFrame()
{
	static int old_screen_width, old_screen_height;
	if (screen_width != old_screen_width || screen_height != old_screen_height) {
		rend_resize(screen_width, screen_height);
		old_screen_width = screen_width;
		old_screen_height = screen_height;
	}
	DoCleanup();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

	//these should be adjusted based on the current PVR scaling etc params
	float dc_width=640;
	float dc_height=480;

	if (!is_rtt)
	{
		gcflip=0;
	}
	else
	{
		gcflip=1;

		//For some reason this produces wrong results
		//so for now its hacked based like on the d3d code
		/*
		u32 pvr_stride=(FB_W_LINESTRIDE.stride)*8;
		*/
		dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
	}

	scale_x = 1;
	scale_y = 1;

	float scissoring_scale_x = 1;

	if (!is_rtt && !pvrrc.isRenderFramebuffer)
	{
		scale_x=fb_scale_x;
		scale_y=fb_scale_y;

		//work out scaling parameters !
		//Pixel doubling is on VO, so it does not affect any pixel operations
		//A second scaling is used here for scissoring
		if (VO_CONTROL.pixel_double)
		{
			scissoring_scale_x = 0.5f;
			scale_x *= 0.5f;
		}

		if (SCALER_CTL.hscale)
		{
			scissoring_scale_x /= 2;
			scale_x*=2;
		}
	}

	dc_width  *= scale_x;
	dc_height *= scale_y;

	/*
		Handle Dc to screen scaling
	*/
	float dc2s_scale_h = is_rtt ? (screen_width / dc_width) : (screen_height / 480.0);
	float ds2s_offs_x =  is_rtt ? 0 : ((screen_width - dc2s_scale_h * 640.0) / 2);

	//-1 -> too much to left
	ShaderUniforms.scale_coefs[0]=2.0f/(screen_width/dc2s_scale_h*scale_x);
	ShaderUniforms.scale_coefs[1]=(is_rtt ? 2 : -2) / dc_height;		// FIXME CT2 needs 480 here instead of dc_height=512
	ShaderUniforms.scale_coefs[2]=1-2*ds2s_offs_x/(screen_width);
	ShaderUniforms.scale_coefs[3]=(is_rtt?1:-1);

	ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

	//printf("scale: %f, %f, %f, %f\n",ShaderUniforms.scale_coefs[0],ShaderUniforms.scale_coefs[1],ShaderUniforms.scale_coefs[2],ShaderUniforms.scale_coefs[3]);

	if (!is_rtt)
		OSD_HOOK();

	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	ShaderUniforms.ps_FOG_COL_VERT[0]=fog_colvert_bgra[2]/255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[1]=fog_colvert_bgra[1]/255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[2]=fog_colvert_bgra[0]/255.0f;

	ShaderUniforms.ps_FOG_COL_RAM[0]=fog_colram_bgra [2]/255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[1]=fog_colram_bgra [1]/255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[2]=fog_colram_bgra [0]/255.0f;

	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[0];
	ShaderUniforms.fog_den_float=fog_den_mant*powf(2.0f,fog_den_exp);

	ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;
	
	if (fog_needs_update)
	{
		fog_needs_update = false;
		UpdateFogTexture((u8 *)FOG_TABLE);
	}

	glcache.UseProgram(gl.modvol_shader.program);

	glUniform4fv( gl.modvol_shader.scale, 1, ShaderUniforms.scale_coefs);

	glUniform1f(gl.modvol_shader.extra_depth_scale, ShaderUniforms.extra_depth_scale);

	GLfloat td[4]={0.5,0,0,0};

	glcache.UseProgram(gl.OSD_SHADER.program);
	glUniform4fv( gl.OSD_SHADER.scale, 1, ShaderUniforms.scale_coefs);
	glUniform1f(gl.OSD_SHADER.extra_depth_scale, 1.0f);

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	GLuint output_fbo;

	//setup render target first
	if (is_rtt)
	{
		GLuint channels,format;
		switch(FB_W_CTRL.fb_packmode)
		{
		case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 1: //0x1   565 RGB 16 bit
			channels=GL_RGB;
			format=GL_UNSIGNED_SHORT_5_6_5;
			break;

		case 2: //0x2   4444 ARGB 16 bit
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 4: //0x4   888 RGB 24 bit packed
		case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
		case 6: //0x6   8888 ARGB 32 bit
			fprintf(stderr, "Unsupported render to texture format: %d\n", FB_W_CTRL.fb_packmode);
			return false;

		case 7: //7     invalid
			die("7 is not valid");
			break;
		}
		//printf("RTT packmode=%d stride=%d - %d,%d -> %d,%d\n", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
		//		FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		output_fbo = BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels, format);
	}
	else
	{
#if HOST_OS != OS_DARWIN
        //Fix this in a proper way
		glBindFramebuffer(GL_FRAMEBUFFER,0);
#endif
		glViewport(0, 0, screen_width, screen_height);
		output_fbo = 0;
	}

	bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	//move vertex to gpu

	if (!pvrrc.isRenderFramebuffer)
	{
		//Main VBO
		glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();

		glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

		glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);

		//Modvol VBO
		if (pvrrc.modtrig.used())
		{
			glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols); glCheck();
			glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
		}

		// TR PolyParam data
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl.vbo.tr_poly_params);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(struct PolyParam) * pvrrc.global_param_tr.used(), pvrrc.global_param_tr.head(), GL_STATIC_DRAW);
		glCheck();

		int offs_x=ds2s_offs_x+0.5f;
		//this needs to be scaled

		//not all scaling affects pixel operations, scale to adjust for that
		scale_x *= scissoring_scale_x;

		#if 0
			//handy to debug really stupid render-not-working issues ...
			printf("SS: %dx%d\n", screen_width, screen_height);
			printf("SCI: %d, %f\n", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
			printf("SCI: %f, %f, %f, %f\n", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
		#endif

		if (!wide_screen_on)
		{
			float width = (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x;
			float height = (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y;
			float min_x = pvrrc.fb_X_CLIP.min / scale_x;
			float min_y = pvrrc.fb_Y_CLIP.min / scale_y;
			if (!is_rtt)
			{
				// Add x offset for aspect ratio > 4/3
				min_x = min_x * dc2s_scale_h + offs_x;
				// Invert y coordinates when rendering to screen
				min_y = screen_height - (min_y + height) * dc2s_scale_h;
				width *= dc2s_scale_h;
				height *= dc2s_scale_h;
			}
			else if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
			{
				min_x *= settings.rend.RenderToTextureUpscale;
				min_y *= settings.rend.RenderToTextureUpscale;
				width *= settings.rend.RenderToTextureUpscale;
				height *= settings.rend.RenderToTextureUpscale;
			}

			glScissor(min_x, min_y, width, height);
			glcache.Enable(GL_SCISSOR_TEST);
		}

		//restore scale_x
		scale_x /= scissoring_scale_x;
		DrawStrips(output_fbo);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
		DrawFramebuffer(dc_width, dc_height);
	}
	#if HOST_OS==OS_WINDOWS
		//Sleep(40); //to test MT stability
	#endif

	eglCheck();

	KillTex=false;

	if (is_rtt)
		ReadRTTBuffer();

	return !is_rtt;
}

#if !defined(_ANDROID) && !defined(TARGET_NACL32)
#if HOST_OS==OS_LINUX
#define SET_AFNT 1
#endif
#endif

extern u16 kcode[4];

void rend_set_fb_scale(float x,float y)
{
	fb_scale_x=x;
	fb_scale_y=y;
}

void reshapeABuffer(int w, int h);

struct glesrend : Renderer
{
	bool Init() { return gles_init(); }
	void Resize(int w, int h)
	{
		screen_width=w;
		screen_height=h;
		if (stencilTexId != 0)
		{
			glcache.DeleteTextures(1, &stencilTexId);
			stencilTexId = 0;
		}
		if (depthTexId != 0)
		{
			glcache.DeleteTextures(1, &depthTexId);
			depthTexId = 0;
		}
		if (opaqueTexId != 0)
		{
			glcache.DeleteTextures(1, &opaqueTexId);
			opaqueTexId = 0;
		}
		if (depthSaveTexId != 0)
		{
			glcache.DeleteTextures(1, &depthSaveTexId);
			depthSaveTexId = 0;
		}
		reshapeABuffer(w, h);
	}
	void Term() { }

	bool Process(TA_context* ctx) { return ProcessFrame(ctx); }
	bool Render() { return RenderFrame(); }

	void Present() { gl_swap(); }

	void DrawOSD() { OSD_DRAW(); }

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};


#include "deps/libpng/png.h"

FILE* pngfile;

void png_cstd_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	fread(data,1, length,pngfile);
}

GLuint loadPNG(const string& fname, int &width, int &height)
{
	const char* filename=fname.c_str();
	FILE* file = fopen(filename, "rb");
	pngfile=file;

	if (!file)
	{
		printf("Error opening %s\n", filename);
		return TEXTURE_LOAD_ERROR;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	fread(header,1,8,file);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png)
	{
		fclose(file);
		printf("Not a PNG file : %s", filename);
		return TEXTURE_LOAD_ERROR;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		NULL, NULL);
	if (!png_ptr)
	{
		fclose(file);
		printf("Unable to create PNG struct : %s", filename);
		return (TEXTURE_LOAD_ERROR);
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		printf("Unable to create PNG info : %s", filename);
		fclose(file);
		return (TEXTURE_LOAD_ERROR);
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		printf("Unable to create PNG end info : %s", filename);
		fclose(file);
		return (TEXTURE_LOAD_ERROR);
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fclose(file);
		printf("Error during setjmp : %s", filename);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return (TEXTURE_LOAD_ERROR);
	}

	//init png reading
	//png_init_io(png_ptr, fp);
	png_set_read_fn(png_ptr, NULL, png_cstd_read);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
		NULL, NULL, NULL);

	//update width and height based on png info
	width = twidth;
	height = theight;

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = new png_byte[rowbytes * height];
	if (!image_data)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		printf("Unable to allocate image_data while loading %s ", filename);
		fclose(file);
		return TEXTURE_LOAD_ERROR;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		printf("Unable to allocate row_pointer while loading %s ", filename);
		fclose(file);
		return TEXTURE_LOAD_ERROR;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - 1 - i] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	//Now generate the OpenGL texture object
	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, (GLvoid*) image_data);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//clean up memory and close stuff
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] image_data;
	delete[] row_pointers;
	fclose(file);

	return texture;
}


Renderer* rend_GLES2() { return new glesrend(); }
