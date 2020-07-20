#ifndef SHARED_CONTEXTS_H
#define SHARED_CONTEXTS_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

//POD / DO NOT TOUCH!!
struct cpu_ctx_t {
    uint32_t pc,sp,pr;
    uint32_t r[16];
    float    fr[32];
};

struct cpu_diss_t {
    char buf[126];
    uint16_t op;
    uint32_t pc;
};

struct gdrom_ctx_t {

};

struct pvr_ctx_t {

};

struct aica_ctx_t {

};

namespace shared_contexts_utils {
    static void deep_copy(cpu_ctx_t* dst,const cpu_ctx_t* src) {
        dst->pc = src->pc;
        dst->sp = src->sp;
        dst->pr = src->pr;
        memcpy(dst->r,src->r,sizeof(src->r));
        memcpy(dst->fr,src->fr,sizeof(src->fr));
    }

    static void deep_copy_diss(cpu_diss_t* dst,const cpu_diss_t* src) {
        strncpy_s(dst->buf,src->buf,sizeof(dst->buf));
        dst->pc = src->pc;
        dst->op = src->op;

    }
}
#endif // SHARED_CONTEXTS_H
