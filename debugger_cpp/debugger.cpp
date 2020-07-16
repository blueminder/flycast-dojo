#include "debugger.h"
#include <QThread>
#include <QObject>

static gui_ctl_c* g_ctl = nullptr;
static bool g_dbg_running = false;
static std::thread g_dbg_thread;
static std::thread::id g_thid ;
static void dummy_event_func(const char* which,const char* state) { }
void (*g_event_func)(const char* which,const char* state) = dummy_event_func;

static void debugger_thread(int argc,char** argv) {
    QApplication app(argc,argv);
    wnd_main wnd;
    g_thid = std::this_thread::get_id();
    delete g_ctl;
    g_ctl = new gui_ctl_c();
    g_ctl->setup(&app,&wnd);
    wnd.show();
    g_dbg_running = true;
    app.exec();
    g_dbg_running = false;
}

bool DEBUGGER_EXPORT debugger_entry_point(int argc,char** argv,void ( (*event_func) (const char*,const char*))) {
    if (g_dbg_running)
        return false;

    if (event_func == nullptr)
        g_event_func = dummy_event_func;
    else
        g_event_func = event_func;

    g_dbg_thread = std::thread(debugger_thread,argc,argv);
    return true;
}

bool DEBUGGER_EXPORT debugger_running() {
    return g_dbg_running;
}

bool DEBUGGER_EXPORT debugger_shutdown() {
    if (g_dbg_thread.joinable())
        g_dbg_thread.join();

    delete g_ctl;
    g_ctl = nullptr;
    g_dbg_running = false;
    return true;
}

bool DEBUGGER_EXPORT debugger_pass_data(const char* class_name,void* data,const uint32_t size) {
    const std::thread::id tid = std::this_thread::get_id();

    if (tid != g_thid)
        emit g_ctl->qpass_data_blocking(class_name,data,size);
    else
        emit g_ctl->qpass_data(class_name,data,size);

    return true;
}

bool DEBUGGER_EXPORT debugger_push_event(const char* which,const char* data) {
    const std::thread::id tid = std::this_thread::get_id();

    if (tid != g_thid)
        emit g_ctl->qpush_msg_blocking(which,data);
    else
        emit g_ctl->qpush_msg(which,data);

    return true;
}
