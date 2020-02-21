#include <mutex>
#include "ta.h"
#include "ta_ctx.h"

#include "hw/sh4/sh4_sched.h"
#include "oslib/oslib.h"

extern u32 fskip;
extern u32 FrameCount;

TA_context* ta_ctx;
tad_context* ta_tad;

rend_context* vd_rc;

void SetCurrentTARC(u32 addr)
{
	if (addr != TACTX_NONE)
	{
		if (ta_ctx)
			SetCurrentTARC(TACTX_NONE);

		verify(ta_ctx == nullptr);
		//set new context
		ta_ctx = tactx_Find(addr,true);

		// set current TA data context
		ta_tad = &ta_ctx->tad;
	}
	else
	{
		verify(ta_ctx != nullptr);
		
		//clear context
		ta_ctx = nullptr;
		ta_tad = nullptr;
	}
}

static cMutex mtx_rqueue;
TA_context* rqueue;
cResetEvent frame_finished;

static double last_frame1, last_frame2;
static u64 last_cycles1, last_cycles2;

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
 	//Try to limit speed to a "sane" level
 	//Speed is also limited via audio, but audio
 	//is sometimes not accurate enough (android, vista+)
	u32 cycle_span = (u32)(sh4_sched_now64() - last_cycles2);
	last_cycles2 = last_cycles1;
	last_cycles1 = sh4_sched_now64();
 	double time_span = os_GetSeconds() - last_frame2;
 	last_frame2 = last_frame1;
 	last_frame1 = os_GetSeconds();

 	bool too_fast = (cycle_span / time_span) > (SH4_MAIN_CLOCK * 1.2);
	
	if (rqueue && too_fast && settings.pvr.SynchronousRender) {
		//wait for a frame if
		//  we have another one queue'd and
		//  sh4 run at > 120% over the last two frames
		//  and SynchronousRender is enabled
		frame_finished.Wait();
	}

	if (rqueue)
	{
		// FIXME if the discarded render is a RTT we'll have a texture missing. But waiting for the current frame to finish kills performance...
		tactx_Recycle(ctx);
		fskip++;
		return false;
	}

	frame_finished.Reset();
	mtx_rqueue.Lock();
	TA_context* old = rqueue;
	rqueue=ctx;
	mtx_rqueue.Unlock();

	verify(!old);

	return true;
}

TA_context* DequeueRender()
{
	mtx_rqueue.Lock();
	TA_context* rv = rqueue;
	mtx_rqueue.Unlock();

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending() {
	mtx_rqueue.Lock();
	TA_context* rv = rqueue;
	mtx_rqueue.Unlock();

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
	if (ctx != NULL)
	{
		verify(rqueue == ctx);
		mtx_rqueue.Lock();
		rqueue = NULL;
		mtx_rqueue.Unlock();

		tactx_Recycle(ctx);
	}
	frame_finished.Set();
}

static cMutex mtx_pool;

static vector<TA_context*> ctx_pool;
static vector<TA_context*> ctx_list;

TA_context* tactx_Alloc()
{
	{
		std::lock_guard<cMutex> lock(mtx_pool);
		if (!ctx_pool.empty())
		{
			TA_context *rv = ctx_pool.back();
			ctx_pool.pop_back();
			return rv;
		}
	}
	
	return new TA_context();
}

void tactx_Recycle(TA_context* poped_ctx)
{
	std::lock_guard<cMutex> lock(mtx_pool);

	if (ctx_pool.size() > 2)
		delete poped_ctx;
	else
	{
		poped_ctx->Reset();
		ctx_pool.push_back(poped_ctx);
	}
}

TA_context* tactx_Find(u32 addr, bool allocnew)
{
	for (TA_context *context : ctx_list)
		if (context->Address == addr)
			return context;

	if (allocnew)
	{
		TA_context* rv = tactx_Alloc();
		rv->Address=addr;
		ctx_list.push_back(rv);

		return rv;
	}
	return nullptr;
}

TA_context* tactx_Pop(u32 addr)
{
	for (auto it = ctx_list.begin(); it != ctx_list.end(); it++)
	{
		if ((*it)->Address == addr)
		{
			TA_context* rv = *it;
			
			if (ta_ctx == rv)
				SetCurrentTARC(TACTX_NONE);

			ctx_list.erase(it);

			return rv;
		}
	}
	return nullptr;
}

void tactx_Term()
{
	for (TA_context *context : ctx_list)
		delete context;
	ctx_list.clear();
	{
		std::lock_guard<cMutex> lock(mtx_pool);
		for (TA_context *context : ctx_pool)
			delete context;
		ctx_pool.clear();
	}
}
