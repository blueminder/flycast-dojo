#pragma once

#include "../types.h"
//TODO Implement dummy debugger interface
extern bool __declspec(dllimport) debugger_entry_point(int argc, char** argv, void((*event_func) (const char* which, const char* state)));
extern bool __declspec(dllimport) debugger_running();
extern bool __declspec(dllimport) debugger_shutdown();
extern bool __declspec(dllimport) debugger_pass_data(const char* class_name, void* data, const uint32_t size);
extern bool __declspec(dllimport) debugger_push_event(const char* which, const char* data);

