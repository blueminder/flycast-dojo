#include "spg.h"
#include "pvr_regs.h"
#include "Renderer_if.h"
#include "rend/TexCache.h"

void libPvr_Reset(bool hard)
{
	KillTex = true;
	Regs_Reset(hard);
	spg_Reset(hard);
}

s32 libPvr_Init()
{
	if (!spg_Init())
	{
		//failed
		return -1;
	}

	return 0;
}

void libPvr_Term()
{
	spg_Term();
}
