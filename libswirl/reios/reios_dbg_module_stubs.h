#pragma once

#include "../types.h"
//TODO Implement dummy debugger interface

#if 0
    #define EXPORT_STUB __attribute__((__visibility__("default")))
    #define IMPORT_STUB __declspec(dllimport)
#else
    #define EXPORT_STUB __attribute__((__visibility__("default")))
    #define IMPORT_STUB 
#endif

#if 0
extern bool IMPORT_STUB debugger_entry_point(int argc, char** argv, void((*event_func) (const char* which, const char* state)));
extern bool IMPORT_STUB debugger_running();
extern bool IMPORT_STUB debugger_shutdown();
extern bool IMPORT_STUB debugger_pass_data(const char* class_name, void* data, const uint32_t size);
extern bool IMPORT_STUB debugger_push_event(const char* which, const char* data);
#else
extern bool (*debugger_entry_point)(int argc, char** argv, void((*event_func) (const char* which, const char* state)))  ;
extern bool (*debugger_running)() ;
extern bool (*debugger_shutdown)() ;
extern bool (*debugger_pass_data)(const char* class_name, void* data, const uint32_t size)  ;
extern bool (*debugger_push_event)(const char* which, const char* data) ;
#endif
