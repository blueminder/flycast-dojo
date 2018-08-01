int debugger_init();
void debugger_tick();
void debugger_destroy();
bool debugger_trap(u32 exception_code);
bool is_debugging();
bool check_breakpoint(u32 pc);
bool check_read_watchpoint(u32 addr);
bool check_write_watchpoint(u32 addr);
