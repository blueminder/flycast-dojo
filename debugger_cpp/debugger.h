#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "debugger_global.h"
#include "wnd_main.h"
#include <QApplication>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

class gui_ctl_c : public QObject {
    Q_OBJECT
private:
    QApplication* m_app;
    wnd_main* m_wnd;

public:
    explicit gui_ctl_c(QApplication* app = nullptr,wnd_main* wnd = nullptr) : m_app(app),m_wnd(wnd){ }
    void setup(QApplication* app,wnd_main* wnd) {
        this->m_app = app;
        this->m_wnd = wnd;

        QObject::connect(this, SIGNAL(qpass_data(const char*,void*,const uint32_t)), this->m_wnd, SLOT(pass_data(const char*,void*,const uint32_t))  ,Qt::AutoConnection);
        QObject::connect(this, SIGNAL(qpass_data_blocking(const char*,void*,const uint32_t)), this->m_wnd, SLOT(pass_data(const char*,void*,const uint32_t))  ,Qt::BlockingQueuedConnection);
        QObject::connect(this, SIGNAL(qpush_msg(const char*,const char*)), this->m_wnd, SLOT(push_msg(const char*,const char*))  ,Qt::AutoConnection);
        QObject::connect(this, SIGNAL(qpass_daqpush_msg_blockingta_blocking(const char*,const char*)), this->m_wnd, SLOT(push_msg(const char*,const char*))  ,Qt::BlockingQueuedConnection);
    }
    /*
    virtual bool event( QEvent *ev )
    {
      if( ev->type() == QEvent::User )
      {
        w = new wnd_main();
        w->show();
        return true;
      }
      return false;
    }*/

signals:
    void qpass_data(const char* class_name,void* data,const uint32_t sz);
    void qpass_data_blocking(const char* class_name,void* data,const uint32_t sz);
    void qpush_msg(const char* which,const char* data);
    void qpush_msg_blocking(const char* which,const char* data);
};

#endif // DEBUGGER_H
