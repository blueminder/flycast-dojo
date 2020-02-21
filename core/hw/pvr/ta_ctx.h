#pragma once
#include "ta.h"
#include "pvr_regs.h"

#define TA_DATA_SIZE (8 * 1024 * 1024)

//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 spc[4];

	float u,v;

	// Two volumes format
	u8 col1[4];
	u8 spc1[4];

	float u1,v1;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	u64 texid;

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
	TSP tsp1;
	TCW tcw1;
	u64 texid1;
};

struct ModifierVolumeParam
{
	u32 first;
	u32 count;
	ISP_Modvol isp;
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

struct tad_context
{
	tad_context()
	{
		thd_data = thd_root = thd_old_data = (u8*)aligned_malloc(TA_DATA_SIZE, 32);
		render_pass_count = 0;
	}
	~tad_context()
	{
		free(thd_root);
	}
	// No copy
	tad_context(const tad_context&) = delete;
	tad_context& operator=(const tad_context &) = delete;

	u8* thd_data;
	u8* thd_root;
	u8* thd_old_data;
	u8 *render_passes[10];
	u32 render_pass_count;

	void Clear()
	{
		thd_old_data = thd_data = thd_root;
		render_pass_count = 0;
	}

	void ClearPartial()
	{
		thd_old_data = thd_data;
		thd_data = thd_root;
	}

	void Continue()
	{
		render_passes[render_pass_count] = End();
		if (render_pass_count < sizeof(render_passes) / sizeof(u8*) - 1)
			render_pass_count++;
	}
	
	u8* End()
	{
		return thd_data == thd_root ? thd_old_data : thd_data;
	}
};

struct RenderPass {
	bool autosort;
	bool z_clear;
	u32 op_count;
	u32 mvo_count;
	u32 pt_count;
	u32 tr_count;
	u32 mvo_tr_count;
};

struct rend_context
{
	u8* proc_start = nullptr;
	u8* proc_end = nullptr;

	f32 fZ_min = 0.f;
	f32 fZ_max = 0.f;

	bool isRTT = false;
	bool isRenderFramebuffer = false;
	
	FB_X_CLIP_type    fb_X_CLIP;
	FB_Y_CLIP_type    fb_Y_CLIP;
	
	u32 fog_clamp_min = 0;
	u32 fog_clamp_max = 0;

	std::vector<Vertex>      verts;
	std::vector<u32>         idx;
	std::vector<ModTriangle> modtrig;
	std::vector<ModifierVolumeParam>  global_param_mvo;
	std::vector<ModifierVolumeParam>  global_param_mvo_tr;

	std::vector<PolyParam>   global_param_op;
	std::vector<PolyParam>   global_param_pt;
	std::vector<PolyParam>   global_param_tr;
	std::vector<RenderPass>  render_passes;

	rend_context()
	{
		verts.reserve(32768);
		idx.reserve(32768);
		global_param_op.reserve(4096);
		global_param_pt.reserve(2048);
		global_param_tr.reserve(4096);
		global_param_mvo.reserve(2048);
		global_param_mvo_tr.reserve(2048);
		modtrig.reserve(4096);
	}

	void Clear()
	{
		verts.clear();
		idx.clear();
		global_param_op.clear();
		global_param_pt.clear();
		global_param_tr.clear();
		modtrig.clear();
		global_param_mvo.clear();
		global_param_mvo_tr.clear();
		render_passes.clear();

		fZ_min= 1000000.0f;
		fZ_max= 1.0f;
		isRenderFramebuffer = false;
	}
};

//vertex lists
struct TA_context
{
	u32 Address;
	u32 LastUsed;

	cMutex rend_inuse;

	tad_context tad;
	rend_context rend;
	
	PolyParam background;
	Vertex bgnd_vtx[4];

	/*
		Dreamcast games use up to 20k vtx, 30k idx, 1k (in total) parameters.
		at 30 fps, thats 600kvtx (900 stripped)
		at 20 fps thats 1.2M vtx (~ 1.8M stripped)

		allocations allow much more than that !

		some stats:
			recv:   idx: 33528, vtx: 23451, op: 128, pt: 4, tr: 133, mvo: 14, modt: 342
			sc:     idx: 26150, vtx: 17417, op: 162, pt: 12, tr: 244, mvo: 6, modt: 2044
			doa2le: idx: 47178, vtx: 34046, op: 868, pt: 0, tr: 354, mvo: 92, modt: 976 (overruns)
			ika:    idx: 46748, vtx: 33818, op: 984, pt: 9, tr: 234, mvo: 10, modt: 16, ov: 0  
			ct:     idx: 30920, vtx: 21468, op: 752, pt: 0, tr: 360, mvo: 101, modt: 732, ov: 0
			sa2:    idx: 36094, vtx: 24520, op: 1330, pt: 10, tr: 177, mvo: 39, modt: 360, ov: 0
	*/

	void MarkRend(u32 render_pass)
	{
		verify(render_pass <= tad.render_pass_count);

		rend.proc_start = render_pass == 0 ? tad.thd_root : tad.render_passes[render_pass - 1];
		rend.proc_end = render_pass == tad.render_pass_count ? tad.End() : tad.render_passes[render_pass];
	}

	TA_context()
	{
		Reset();
	}

	void Reset()
	{
		rend_inuse.Lock();
		verify(tad.End() - tad.thd_root < TA_DATA_SIZE);
		tad.Clear();
		rend.Clear();
		rend.proc_end = rend.proc_start = tad.thd_root;
		rend_inuse.Unlock();
	}

	~TA_context()
	{
		verify(tad.End() - tad.thd_root < TA_DATA_SIZE);
	}
};


extern TA_context* ta_ctx;
extern tad_context* ta_tad;

extern rend_context* vd_rc;

TA_context* tactx_Find(u32 addr, bool allocnew=false);
TA_context* tactx_Pop(u32 addr);

TA_context* tactx_Alloc();
void tactx_Recycle(TA_context* poped_ctx);
void tactx_Term();
/*
	Ta Context

	Rend Context
*/

#define TACTX_NONE (0xFFFFFFFF)

void SetCurrentTARC(u32 addr);
bool QueueRender(TA_context* ctx);
TA_context* DequeueRender();
void FinishRender(TA_context* ctx);

//must be moved to proper header
void FillBGP(TA_context* ctx);
bool UsingAutoSort(int pass_number);
bool rend_framePending();
