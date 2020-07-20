#include "wnd_main.h"
#include "ui_wnd_main.h"
#include <QMessageBox>
#include <QFileDialog>

extern void (*g_event_func)(const char* which,const char* state);

template <typename base_t>
static inline const std::string to_hex(const base_t v) {
    std::stringstream stream;
    stream << std::hex << v;
    return stream.str();
}

template <typename base_t>
static inline const base_t from_hex(const std::string& s) {
    std::istringstream i(s);
    base_t t;

    i >> std::hex >> t;
    return t;
}

wnd_main::wnd_main(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wnd_main) {

    ui->setupUi(this);
    m_code_list_index = -1;
    this->init();
}

wnd_main::~wnd_main()
{
    delete ui;
}

bool wnd_main::export_binary(const std::string& path,const std::string& flags) {
    FILE* f;
    fopen_s(&f,path.c_str(),"wb");

    if (!f)
        return false;

    size_t wr = 0;

    for (auto c : m_code)
        wr += fwrite((void*)&c.second.code,sizeof(uint16_t),1,f) ;

    fclose(f);

    return wr == m_code.size();
}

void wnd_main::show_msgbox(const std::string& text) {
    QMessageBox msgbox;
    msgbox.setText(text.c_str());
    msgbox.exec();
}

std::string wnd_main::dec_addr_from_codestream(const std::string& cstrm) {
    size_t i = cstrm.find(':');
    if (i == std::string::npos)
        return "null";

    return cstrm.substr(0,i);
}

void wnd_main::add_list_item(const std::string& s) {
    ui->list_cpu_instructions->addItem(s.c_str());
}

void wnd_main::highlight_list_item(const size_t index) {
   // show_msgbox(std::to_string(index));
    if (!ui->chk_trace->isChecked())
        return;

    ui->list_cpu_instructions->setCurrentRow((int)index);
}

void wnd_main::push_msg(const char* which,const char* data) {

}

void wnd_main::pass_data(const char* class_name,void* data,const uint32_t sz) { //THIS SHOULD BECOME NON BLOCKING

    std::string tmp = std::string(class_name);
    if (tmp == "cpu_ctx") {
        shared_contexts_utils::deep_copy(&m_cpu_context,static_cast<const cpu_ctx_t*>(data));
        upd_cpu_ctx();
    } else if (tmp == "cpu_diss") { // XXX : WARNING : ASSUMES CPU CONTEXT IS FRESH! (AKA UPDATED BEFORE)
        shared_contexts_utils::deep_copy_diss(&m_cpu_diss,static_cast<const cpu_diss_t*>(data));
        auto it = m_code.find(m_cpu_diss.pc);
        if (it == m_code.end()) {
            std::string cc = m_cpu_diss.buf;
            size_t i = cc.find(';');
            if (i != std::string::npos) {
                cc = cc.substr(0,i);
            }
            std::string tmp = std::string( to_hex(m_cpu_diss.pc) + ":" + cc );
            QListWidgetItem* item = new QListWidgetItem ( tmp.c_str()) ;
            ui->list_cpu_instructions->addItem(item);

            if (ui->chk_trace->isChecked())
                ui->list_cpu_instructions->setCurrentItem(item);

            m_code.insert({m_cpu_diss.pc, code_field_t ( cc  , m_cpu_context.pc , m_cpu_context.sp , m_cpu_context.pr , m_cpu_context.r,
                          m_cpu_context.fr , 0,item,m_cpu_diss.op )});
        } else {
            std::string cc = m_cpu_diss.buf;
            size_t i = cc.find(';');
            if (i != std::string::npos) {
                cc = cc.substr(0,i);
            }
            if (it->second.diss != cc) {
                it->second.diss = cc;
                it->second.pc = m_cpu_context.pc;
                it->second.sp = m_cpu_context.sp;
                it->second.pr = m_cpu_context.pr;

                memcpy(it->second.gpr,m_cpu_context.r,sizeof(m_cpu_context.r));
                memcpy(it->second.fpr,m_cpu_context.fr,sizeof(m_cpu_context.fr));
            }

            if (ui->chk_trace->isChecked())
                ui->list_cpu_instructions->setCurrentItem(it->second.item);
        }
    }
}

void wnd_main::upd_live_ctx(const std::string& s) {
    ui->lbl_reg->setText(s.c_str());
}

void wnd_main::upd_cpu_ctx() {
    std::string lbl_txt;

    lbl_txt = "PC:" + to_hex(m_cpu_context.pc) + "\n";
    lbl_txt += "PR:" + to_hex(m_cpu_context.pr) + "\n";
    lbl_txt += "SP:" + to_hex(m_cpu_context.sp) + "\n";

    for (size_t i = 0;i < 16;++i) {
        lbl_txt += "R" + std::to_string(i) + ":" + to_hex(m_cpu_context.r[i])  +"\n";
    }

    for (size_t i = 0;i < 16;++i) {
        lbl_txt +=  "fR" + std::to_string(i) + ":" + std::to_string(m_cpu_context.fr[i])  ;
        lbl_txt +=  " | fR" + std::to_string(16+i) + ":" + std::to_string(m_cpu_context.fr[16+i])  +"\n";
    }
    upd_live_ctx(lbl_txt.c_str());
}

void wnd_main::init() {
    std::string lbl_txt;

    ui->reg_combo_box->addItem("PC");
    ui->reg_combo_box->addItem("SP");
    ui->reg_combo_box->addItem("PR");

    lbl_txt = "PC:$0\nSP:$0\nPR:$0\n";

    for (size_t i = 0;i < 16;++i) {
        lbl_txt += "R" + std::to_string(i) + ":$0\n";
        ui->reg_combo_box->addItem(std::string("R" + std::to_string(i)).c_str());
    }

    for (size_t i = 0;i < 16;++i) {
        lbl_txt +=  "fR" + std::to_string(i)  +":$0\n";
        ui->reg_combo_box->addItem(std::string("fR" + std::to_string(i)).c_str());
    }

    ui->lbl_reg->setText(lbl_txt.c_str());
}

void wnd_main::on_pushButton_7_clicked()
{
    g_event_func("set_pc",ui->txt_set_pc->text().toStdString().c_str());
}

void wnd_main::on_pushButton_6_clicked()
{
    uint32_t data[2]; //todo : FPR / SP / PC / ETC

    g_event_func("set_reg",(const char*)data);
}


void wnd_main::on_list_cpu_instructions_itemDoubleClicked(QListWidgetItem *item)
{

}

void wnd_main::on_list_cpu_instructions_itemClicked(QListWidgetItem *item) {
   if (!item || item->text().isEmpty())
       return;

   const code_field_t& fld =  m_code[ from_hex<uint32_t>(  dec_addr_from_codestream(item->text().toStdString())) ];

   std::string lbl_txt;

   lbl_txt = "PC:" + to_hex(fld.pc) + "\n";
   lbl_txt += "PR:" + to_hex(fld.pr) + "\n";
   lbl_txt += "SP:" + to_hex(fld.sp) + "\n";

   for (size_t i = 0;i < 16;++i) {
       lbl_txt += "R" + std::to_string(i) + ":" + to_hex(fld.gpr[i])  +"\n";
   }
   for (size_t i = 0;i < 16;++i) { //32 FpRs
       lbl_txt +=  "fR" + std::to_string(i) + ":" + std::to_string(fld.fpr[i])  ;
       lbl_txt +=  " | fR" + std::to_string(16+i) + ":" + std::to_string(fld.fpr[16+i])  +"\n";
   }
   ui->lbl_state_data->setText(lbl_txt.c_str());
}

void wnd_main::on_pushButton_clicked()
{
    g_event_func("step","");
}

void wnd_main::on_pushButton_11_clicked()
{
    g_event_func("stop","");
}

void wnd_main::on_pushButton_12_clicked()
{
    g_event_func("start","");
}

void wnd_main::on_pushButton_13_clicked()
{
    g_event_func("reset","");
}

void wnd_main::on_chk_active_toggled(bool checked) {
    g_event_func("dbg_enable",(checked) ? "true":"false");

    if (checked) {
        m_code.clear();
        ui->list_cpu_instructions->clear();
    }
}

void wnd_main::set_selected_breakpoint_marked(const bool marked) {
    const int32_t idx = ui->list_cpu_instructions->currentIndex().row();

    if ( (idx < 0) || (idx > ui->list_cpu_instructions->count() ))
         return;

    //std::string s = (marked) ?
      //          "color:#00ff00;\nbackground-color:transparent;\n" : "color:#000000;\nbackground-color:transparent;\n";

    ui->list_cpu_instructions->currentItem()->setTextColor((marked) ? Qt::red : Qt::black);
}

void wnd_main::update_breakpoint_list() {
    ui->list_breakpoints->clear();

    for (auto s : m_breakpoints)
        ui->list_breakpoints->addItem( s.second.c_str());
}

uint32_t wnd_main::reverse_breakpoint_pc_from_selection()  {
    const int32_t idx = ui->list_cpu_instructions->currentIndex().row();

    if ( (idx < 0) || (idx > ui->list_cpu_instructions->count() ))
         return (uint32_t)-1U;

    return m_code[ from_hex<uint32_t>(  dec_addr_from_codestream(ui->list_cpu_instructions->item(idx)->text().toStdString() ))  ].pc;
}

void wnd_main::on_btn_brk_rm_clicked() {
    const uint32_t pc = reverse_breakpoint_pc_from_selection();
    if (pc == (uint32_t)-1U)
        return;

    auto it = m_breakpoints.find(pc);
    if ( it == m_breakpoints.end())
        return;

    set_selected_breakpoint_marked(false);

    m_breakpoints.erase(it);
    update_breakpoint_list();
}

void wnd_main::on_btn_brk_add_clicked() {
    const uint32_t pc = reverse_breakpoint_pc_from_selection();
    if (pc == (uint32_t)-1U)
        return;

    if (m_breakpoints.find(pc) != m_breakpoints.end())
        return;

    const std::string s = to_hex(pc);

    set_selected_breakpoint_marked(true);
    m_breakpoints.insert( {pc,s} );

    update_breakpoint_list();
}

void wnd_main::on_chk_sort_code_path_toggled(bool checked) {
    ui->list_breakpoints->sortItems(Qt::AscendingOrder);
    ui->list_cpu_instructions->sortItems(Qt::AscendingOrder);
    ui->list_breakpoints->setSortingEnabled(checked);
    ui->list_cpu_instructions->setSortingEnabled(checked);
}

void wnd_main::on_list_breakpoints_itemClicked(QListWidgetItem *item)
{
    ui->txt_rename_breakpoint->setText(item->text());
}

void wnd_main::on_btn_rename_breakpoint_clicked()
{
    if (ui->txt_rename_breakpoint->text().length() < 1)
        return;

    //XXX check if exists -_-

    const int32_t idx = ui->list_breakpoints->currentIndex().row();
    if ( (idx < 0) || (idx > ui->list_breakpoints->count() ))
         return ;

   // const uint32_t pc = m_breakpoints[ ui->list_breakpoints->currentItem()->text().toStdString() ];

}

void wnd_main::on_chk_active_stateChanged(int arg1)
{

}

void wnd_main::on_btn_export_bin_clicked() {
    QString ff = QFileDialog::getSaveFileName(this,
            tr("Binary"), "",
            tr("Binary (*.bin);;All Files (*)"));

    export_binary(ff.toStdString() ,"sh4bin");

}
