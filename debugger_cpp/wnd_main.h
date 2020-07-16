#ifndef WND_MAIN_H
#define WND_MAIN_H

#include <QWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "shared_contexts.h"
#include <sstream>
#include "../libswirl/utils/string_utils.hpp"

static constexpr auto k_default_code_history_size = 1024;
static constexpr auto k_max_code_size = 128*1024;


struct call_info_t {

};

struct op_data_t {

};

struct codebook_node_t {
    uint32_t addr;
    uint16_t op;
    std::string cached_diss;
    //TODO state
    //TODO modifiers
};

class code_book_c {
private:
    size_t m_code_ptr;
    size_t m_history_size;
    size_t m_max_code_size;
    std::unordered_map<uint32_t,op_data_t> m_ops;
    std::vector<codebook_node_t> m_code;
    std::vector<codebook_node_t> m_segmented_code;

public:
    code_book_c() : m_code_ptr(0) , m_history_size(k_default_code_history_size) , m_max_code_size(k_max_code_size) {

    }

    ~code_book_c() {

    }

    //TODO more args
    void put(const uint32_t pc,const uint16_t op,const char* diss = nullptr) {
        if (m_code_ptr < m_history_size) {

            ++m_code_ptr;
            return;
        }

        m_code[m_code_ptr].op = op;
        m_code[m_code_ptr].addr = pc;
        m_code[m_code_ptr].cached_diss = (diss != nullptr) ? std::string(diss) : "(null)";

        m_code_ptr = (m_code_ptr + 1) % m_code.size();
    }

    void apply_segmentation() {
        m_segmented_code.clear();
    }

    void reset() {
        m_code_ptr = 0;
        m_history_size = k_default_code_history_size;
        m_max_code_size = k_max_code_size;

        m_code.clear();
        m_ops.clear();
    }

};

namespace Ui {
    class wnd_main;
}


struct code_field_t {
    std::string diss;
    uint32_t pc,sp,pr;
    uint32_t gpr[16];
    size_t list_index;
    float fpr[32];

    code_field_t() {

    }

    code_field_t(const std::string& _diss,const uint32_t _pc = 0,const uint32_t _sp=0,const uint32_t _pr=0,const uint32_t* _gpr  = nullptr,const float* _fpr = nullptr,
                 const size_t _list_index = 0) :
        diss(_diss) , pc(_pc) , sp(_sp) , pr (_pr) , list_index(_list_index) {
        if (nullptr == _gpr)
            memset(gpr,0,sizeof(gpr));
        else
            memcpy(gpr,_gpr,sizeof(gpr));

        if (nullptr == _fpr)
            memset(fpr,0,sizeof(fpr));
        else
            memcpy(fpr,_fpr,sizeof(fpr));
    }
};

class wnd_main : public QWidget {
    Q_OBJECT

public slots:
    void pass_data(const char* class_name,void* data,const uint32_t sz);
    void push_msg(const char* which,const char* data);

public:
    explicit wnd_main(QWidget *parent = nullptr);
    ~wnd_main();
    void show_msgbox(const std::string& text);
    void add_list_item(const std::string& s);
    void upd_live_ctx(const std::string& s);
    void highlight_list_item(const size_t index);

private slots:
    void on_pushButton_7_clicked();
    void on_pushButton_6_clicked();
    void on_list_cpu_instructions_itemDoubleClicked(QListWidgetItem *item);
    void on_list_cpu_instructions_itemClicked(QListWidgetItem *item);
    void on_pushButton_clicked();
    void on_pushButton_11_clicked();
    void on_pushButton_12_clicked();
    void on_pushButton_13_clicked();
    void on_chk_active_toggled(bool checked);
    void on_btn_brk_rm_clicked();
    void on_btn_brk_add_clicked();

    void on_chk_sort_code_path_toggled(bool checked);

    void on_list_breakpoints_itemClicked(QListWidgetItem *item);

    void on_btn_rename_breakpoint_clicked();

private:
    void init();
    void upd_cpu_ctx();
    bool export_binary(const std::string& path,const std::string& flags);
    void update_breakpoint_list();
    void set_selected_breakpoint_marked(const bool marked);
    uint32_t reverse_breakpoint_pc_from_selection()  ;

private:
    std::unordered_map<uint32_t,std::string> m_breakpoints;
    std::unordered_map<uint32_t,code_field_t> m_code;
    std::vector<uint32_t> m_reverse_code_indices;
    cpu_ctx_t m_cpu_context;
    cpu_diss_t m_cpu_diss;
    code_book_c m_code_book;
    Ui::wnd_main *ui;
    int32_t m_code_list_index;
};

#endif // WND_MAIN_H
