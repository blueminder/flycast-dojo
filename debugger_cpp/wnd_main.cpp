#include "wnd_main.h"
#include "ui_wnd_main.h"
#include <QMessageBox>
#include <QFileDialog>
#include <ctime>
#include <sys/time.h>

extern void (*g_event_func)(const char* which,const char* state);



inline uint64_t get_time() {
    timeval time;
    gettimeofday(&time, NULL);
    return (uint64_t)time.tv_sec*1000 + (uint64_t)time.tv_usec/1000;
}

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

void script_cb(const asSMessageInfo *msg, void *param) {
    const char *type = "ERR ";
    if( msg->type == asMSGTYPE_WARNING )
        type = "WARN";
    else if( msg->type == asMSGTYPE_INFORMATION )
        type = "INFO";

    printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

void script_dgb_print(std::string &str) {
    printf("Script: %s\n",str.c_str());
}


void script_ln_brk(asIScriptContext *ctx, uint32_t *timeOut) {


    if( *timeOut < get_time() )
        ctx->Abort();
}

int script_compile(asIScriptEngine *engine,const std::string& path)
{
    int r;

    // We will load the script from a file on the disk.
    FILE *f = fopen(path.c_str(), "rb");
    if( f == 0 )
    {
        std::cout << "Failed to open the script file : " << path << std::endl;
        return -1;
    }

    // Determine the size of the file
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);

    // On Win32 it is possible to do the following instead
    // int len = _filelength(_fileno(f));

    // Read the entire file
    std::string script;
    script.resize(len);
    size_t c = fread(&script[0], len, 1, f);
    fclose(f);

    if( c == 0 )
    {
        //cout << "Failed to load script file." << endl;
        return -1;
    }

    // Add the script sections that will be compiled into executable code.
    // If we want to combine more than one file into the same script, then
    // we can call AddScriptSection() several times for the same module and
    // the script engine will treat them all as if they were one. The script
    // section name, will allow us to localize any errors in the script code.
    asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
    r = mod->AddScriptSection("script", &script[0], len);
    if( r < 0 )
    {
        //cout << "AddScriptSection() failed" << endl;
        return -1;
    }

    // Compile the script. If there are any compiler messages they will
    // be written to the message stream that we set right after creating the
    // script engine. If there are no errors, and no warnings, nothing will
    // be written to the stream.
    r = mod->Build();
    if( r < 0 )
    {
        //cout << "Build() failed" << endl;
        return -1;
    }

    // The engine doesn't keep a copy of the script sections after Build() has
    // returned. So if the script needs to be recompiled, then all the script
    // sections must be added again.

    // If we want to have several scripts executing at different times but
    // that have no direct relation with each other, then we can compile them
    // into separate script modules. Each module use their own namespace and
    // scope, so function names, and global variables will not conflict with
    // each other.

    return 0;
}

void wnd_main::refresh_scripts() {
    this->ui->scripts_list->clear();


}

wnd_main::wnd_main(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wnd_main) {

    ui->setupUi(this);
    m_script_engine = nullptr;
    m_code_list_index = -1;

    static constexpr uint32_t dc_bios_syscall_system = 0x8C0000B0;
    static constexpr uint32_t dc_bios_syscall_font = 0x8C0000B4;
    static constexpr uint32_t dc_bios_syscall_flashrom = 0x8C0000B8;
    static constexpr uint32_t dc_bios_syscall_gd = 0x8C0000BC;
    static constexpr uint32_t dc_bios_syscall_misc = 0x8c0000E0;
    static constexpr uint32_t dc_bios_entrypoint_gd_do_bioscall = 0x8c0010F0; //At least one game (ooga) uses this directly
    static constexpr uint32_t SYSINFO_ID_ADDR = 0x8C001010;

    m_reios_syscalls.insert({dc_bios_syscall_system});
    m_reios_syscalls.insert({dc_bios_syscall_font});
    m_reios_syscalls.insert({dc_bios_syscall_flashrom});
    m_reios_syscalls.insert({dc_bios_syscall_gd});
    m_reios_syscalls.insert({dc_bios_syscall_misc});
    m_reios_syscalls.insert({dc_bios_entrypoint_gd_do_bioscall});
    m_reios_syscalls.insert({SYSINFO_ID_ADDR});

    this->init();
}

wnd_main::~wnd_main()
{
    for (auto s:m_script_ctxs) {
        s->Abort();
        s->Release();
    }

    if (m_script_engine != nullptr) {
        m_script_engine->Release();
    }
    delete ui;
}

bool wnd_main::export_binary(const std::string& path,const std::string& flags) {
    FILE* f;
    f = fopen(path.c_str(),"wb");

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

void wnd_main::upd_gdrom_fields() {
    ui->pick_gdrom_field->clear();

    for(auto s : m_gd_data)
        ui->pick_gdrom_field->addItem(s.first.c_str());

    ui->pick_gdrom_field->addItem("*");
}

void wnd_main::pass_data(const char* class_name,void* data,const uint32_t sz) { //THIS SHOULD BECOME NON BLOCKING
    std::string tmp = std::string(class_name);

    if ((tmp == "syscall_enter") || (tmp == "syscall_leave")) {
        uint32_t t;
        memcpy(&t,data,sz);
        auto it = m_code.find(t);
        if (it == m_code.end())
            return;

        it->second.item->setTextColor( (tmp == "syscall_enter") ? Qt::green : Qt::red);
    }
    else if (tmp.find("gdrom_") != std::string::npos) {
        auto it = m_gd_data.find(tmp);
        if (it == m_gd_data.end()) {
            std::vector<gd_data_t> cc;
            if (data != nullptr) {
                cc.push_back(gd_data_t());
                cc.back().time = get_time();
                cc.back().cname = tmp;
                memcpy(&cc.back(),data,sz);
            }
            m_gd_data.insert( {tmp,std::move(cc)});
            upd_gdrom_fields();
        } else {
            if (data != nullptr) {
                it->second.push_back(gd_data_t());
                it->second.back().cname=tmp;
                memcpy(&it->second.back(),data,sz);
            }
        }
    }
    else if (tmp == "cpu_ctx") {
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

            if (!m_reios_syscalls.empty()) {
                if (m_reios_syscalls.find(m_cpu_diss.pc) != m_reios_syscalls.end()) {
                    item->setTextColor(Qt::yellow);
                }
            }

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


            if (!m_reios_syscalls.empty()) {
                if (m_reios_syscalls.find(m_cpu_diss.pc) != m_reios_syscalls.end()) {
                    it->second.item->setTextColor(Qt::yellow);
                }
            }

            if (ui->chk_trace->isChecked())
                ui->list_cpu_instructions->setCurrentItem(it->second.item);
        }
#if 0
        if (m_script_ctxs.empty()) {
            return;
        }

        for (auto s : m_script_ctxs){
            uint32_t timeOut;
            int r = s->SetLineCallback(asFUNCTION(script_ln_brk), &timeOut, asCALL_CDECL);
            if( r < 0 )
            {

            }

            timeOut = get_time() + 2000;

            //on_cpu_op(u32* i_regs,float* f_regs,u32 pc,u32 op,u32 sp,u32 pr)
            s->SetArgAddress(0,m_cpu_context.r);
            s->SetArgAddress(1,m_cpu_context.fr);
            s->SetArgDWord(2,m_cpu_context.pc);
            s->SetArgDWord(2,m_cpu_context.sp);
            s->SetArgDWord(2,m_cpu_context.pr);
            s->Execute();
        }
#endif
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
    int idx = ui->reg_combo_box->currentIndex();
    if (idx < 0)
        return ;

    std::string data;
    data = ui->reg_combo_box->itemText(idx).toStdString() + ":"+
    ui->sel_reg_linedit->text().toStdString();
    g_event_func("set_reg",data.c_str());
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

void wnd_main::on_btn_import_script_clicked()
{
    QString ff = QFileDialog::getOpenFileName(this,
            tr("Angel Script"), "",
            tr("Angel Script (*.as);;All Files (*)"));

    refresh_scripts();
}

void wnd_main::on_pushButton_2_clicked()
{
#if 0
    printf("Loading AS..\n");
    try {
    m_script_engine =  asCreateScriptEngine(ANGELSCRIPT_VERSION);

    if (m_script_engine != nullptr) {
        m_script_engine->SetMessageCallback(asFUNCTION( script_cb), 0, asCALL_CDECL);
    return;
        RegisterStdString(m_script_engine);
        m_script_engine->RegisterGlobalFunction("void dbg_print(string &in)", asFUNCTION(script_dgb_print), asCALL_CDECL);

        int r;

        r = script_compile(m_script_engine,"scripts/main.as");
        if (r >= 0) {



            // Create a context that will execute the script.
            asIScriptContext *ctx = m_script_engine->CreateContext();
            if( ctx == 0 )
            {

            } else
                m_script_ctxs.push_back(ctx);


            if (!m_script_engine->GetModule(0))
                return;
            asIScriptFunction *func = m_script_engine->GetModule(0)->GetFunctionByDecl("void on_cpu_op(u32* i_regs,float* f_regs,u32 pc,u32 op,u32 sp,u32 pr)");
            if( func == 0 ) {
                ctx->Release();
                m_script_ctxs.clear();
            } else if (!m_script_ctxs.empty()) {
                m_script_ctxs[0]->Prepare(func);
            }
        }
    }
    } catch (...) {

    }

#endif
}

void wnd_main::on_pick_gdrom_field_currentIndexChanged(const QString &arg1)
{
    m_gd_filter = arg1.toStdString();
}

void wnd_main::on_btn_gdfilter_refresh_clicked()
{
    ui->list_gd_field->clear();

    if (m_gd_filter == "*") {
        std::vector<gd_data_t> tmp;
        for (auto s : m_gd_data) {
            for (auto m : s.second)
                tmp.push_back(m);
        }
        std::sort(tmp.begin(),tmp.end(),[](const gd_data_t& a,const gd_data_t& b) -> bool {
            return b.time < a.time;
        } );

        for (auto q : tmp) {
            std::string v = q.cname+":R";
            for (size_t i = 0;i<16;++i)
                v += std::to_string(i)+":"+to_hex(q.data.regs[i]) + ",";

            v += "S:";

            for (size_t i = 0;i<4;++i)
                v += std::to_string(i)+":"+to_hex(q.data.sess[i]) + ",";

            v += "PC:" + to_hex(q.data.pc);
            v += ",Ra:" + to_hex(q.data.r_addr);
            v += ",Wa:" + to_hex(q.data.w_addr);

            //for (auto m : q.data)
              //  v += to_hex(m) + ",";

            ui->list_gd_field->addItem(v.c_str());
        }



    } else if (!m_gd_filter.empty()) {
        auto it = m_gd_data.find(m_gd_filter);
        if (it == m_gd_data.end())
            return;

         for (auto q : it->second) {

                std::string v = ":R";
                for (size_t i = 0;i<16;++i)
                    v += std::to_string(i)+":"+to_hex(q.data.regs[i]) + ",";

                v += "S:";

                for (size_t i = 0;i<4;++i)
                    v += std::to_string(i)+":"+to_hex(q.data.sess[i]) + ",";

                v += "PC:" + to_hex(q.data.pc);
                v += ",Ra:" + to_hex(q.data.r_addr);
                v += ",Wa:" + to_hex(q.data.w_addr);

                ui->list_gd_field->addItem(v.c_str());
         }
    }
}
