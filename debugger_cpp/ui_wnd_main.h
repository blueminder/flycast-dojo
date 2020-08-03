/********************************************************************************
** Form generated from reading UI file 'wnd_main.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WND_MAIN_H
#define UI_WND_MAIN_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_wnd_main
{
public:
    QGridLayout *gridLayout;
    QTabWidget *tabWidget;
    QWidget *tab;
    QGridLayout *gridLayout_2;
    QTabWidget *tabWidget_2;
    QWidget *tab_3;
    QGridLayout *gridLayout_5;
    QListWidget *list_cpu_instructions;
    QFrame *frame_2;
    QGridLayout *gridLayout_7;
    QFrame *frame_3;
    QPushButton *pushButton_6;
    QLabel *label_2;
    QLabel *label_3;
    QLineEdit *sel_reg_linedit;
    QComboBox *reg_combo_box;
    QLabel *label_4;
    QLineEdit *txt_set_pc;
    QPushButton *pushButton_7;
    QCheckBox *chk_sel;
    QPushButton *btn_export_bin;
    QCheckBox *chk_ptr;
    QPushButton *pushButton_10;
    QFrame *frame_6;
    QGridLayout *gridLayout_8;
    QTabWidget *tabWidget_4;
    QWidget *tab_10;
    QGridLayout *gridLayout_9;
    QLabel *lbl_state_data;
    QWidget *tab_11;
    QWidget *tab_12;
    QGridLayout *gridLayout_10;
    QListWidget *list_breakpoints;
    QLabel *label_9;
    QLineEdit *txt_rename_breakpoint;
    QPushButton *btn_rename_breakpoint;
    QLabel *label_11;
    QLineEdit *txt_manual_add_brk;
    QPushButton *btn_rename_breakpoint_2;
    QWidget *tab_13;
    QComboBox *reg_combo_box_2;
    QLabel *label_5;
    QLabel *label_6;
    QLineEdit *sel_reg_linedit_2;
    QLabel *label_7;
    QComboBox *reg_combo_box_3;
    QLabel *label_8;
    QLineEdit *sel_reg_linedit_3;
    QPushButton *pushButton_9;
    QWidget *tab_15;
    QGridLayout *gridLayout_11;
    QLabel *label_10;
    QComboBox *pick_gdrom_field;
    QPushButton *btn_gdfilter_refresh;
    QListWidget *list_gd_field;
    QWidget *tab_14;
    QCheckBox *chk_active_2;
    QFrame *frame_4;
    QGridLayout *gridLayout_6;
    QCheckBox *chk_trace;
    QCheckBox *chk_dec;
    QCheckBox *chk_flt;
    QCheckBox *chk_str;
    QCheckBox *chk_hex;
    QLabel *lbl_reg;
    QFrame *frame;
    QGridLayout *gridLayout_4;
    QPushButton *pushButton;
    QPushButton *pushButton_12;
    QPushButton *pushButton_11;
    QPushButton *pushButton_13;
    QPushButton *btn_brk_add;
    QPushButton *btn_brk_rm;
    QPushButton *pushButton_4;
    QPushButton *pushButton_5;
    QCheckBox *chk_active;
    QCheckBox *chk_sort_code_path;
    QCheckBox *checkBox_3;
    QWidget *tab_4;
    QWidget *tab_2;
    QGridLayout *gridLayout_3;
    QTabWidget *tabWidget_3;
    QWidget *tab_5;
    QWidget *tab_6;
    QWidget *tab_7;
    QWidget *tab_8;
    QWidget *tab_9;
    QListWidget *scripts_list;
    QPushButton *pushButton_2;
    QTextEdit *script_log;

    void setupUi(QWidget *wnd_main)
    {
        if (wnd_main->objectName().isEmpty())
            wnd_main->setObjectName(QString::fromUtf8("wnd_main"));
        wnd_main->resize(1282, 622);
        QFont font;
        font.setFamily(QString::fromUtf8("Calibri"));
        font.setPointSize(12);
        font.setBold(false);
        font.setWeight(50);
        font.setStyleStrategy(QFont::PreferAntialias);
        wnd_main->setFont(font);
        wnd_main->setStyleSheet(QString::fromUtf8("background-color:qlineargradient(spread:pad, x1:0, y1:1, x2:1, y2:0, stop:0 rgba(61, 61, 61, 255), stop:1 rgba(255, 255, 255, 255));\n"
"color:rgba(255, 255, 255,255)\n"
""));
        gridLayout = new QGridLayout(wnd_main);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        tabWidget = new QTabWidget(wnd_main);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        QFont font1;
        font1.setFamily(QString::fromUtf8("Calibri"));
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        font1.setStyleStrategy(QFont::PreferAntialias);
        tabWidget->setFont(font1);
        tabWidget->setAutoFillBackground(false);
        tabWidget->setStyleSheet(QString::fromUtf8(""));
        tab = new QWidget();
        tab->setObjectName(QString::fromUtf8("tab"));
        gridLayout_2 = new QGridLayout(tab);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        tabWidget_2 = new QTabWidget(tab);
        tabWidget_2->setObjectName(QString::fromUtf8("tabWidget_2"));
        tabWidget_2->setFont(font1);
        tabWidget_2->setAutoFillBackground(false);
        tab_3 = new QWidget();
        tab_3->setObjectName(QString::fromUtf8("tab_3"));
        gridLayout_5 = new QGridLayout(tab_3);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        list_cpu_instructions = new QListWidget(tab_3);
        list_cpu_instructions->setObjectName(QString::fromUtf8("list_cpu_instructions"));
        list_cpu_instructions->setMaximumSize(QSize(500, 10000));
        QFont font2;
        font2.setFamily(QString::fromUtf8("Calibri"));
        font2.setPointSize(14);
        font2.setBold(true);
        font2.setWeight(75);
        font2.setStyleStrategy(QFont::PreferAntialias);
        list_cpu_instructions->setFont(font2);
        list_cpu_instructions->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
"color:rgb(0,0,0);"));
        list_cpu_instructions->setFrameShape(QFrame::WinPanel);
        list_cpu_instructions->setFrameShadow(QFrame::Plain);
        list_cpu_instructions->setLineWidth(4);
        list_cpu_instructions->setMidLineWidth(4);
        list_cpu_instructions->setSortingEnabled(true);

        gridLayout_5->addWidget(list_cpu_instructions, 1, 0, 1, 1);

        frame_2 = new QFrame(tab_3);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        frame_2->setMaximumSize(QSize(1000, 16777215));
        QFont font3;
        font3.setStyleStrategy(QFont::PreferAntialias);
        frame_2->setFont(font3);
        frame_2->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
"color:rgb(0,0,0);"));
        frame_2->setFrameShape(QFrame::WinPanel);
        frame_2->setFrameShadow(QFrame::Plain);
        frame_2->setLineWidth(4);
        frame_2->setMidLineWidth(4);
        gridLayout_7 = new QGridLayout(frame_2);
        gridLayout_7->setObjectName(QString::fromUtf8("gridLayout_7"));
        frame_3 = new QFrame(frame_2);
        frame_3->setObjectName(QString::fromUtf8("frame_3"));
        frame_3->setMinimumSize(QSize(220, 200));
        frame_3->setMaximumSize(QSize(1000, 500));
        QFont font4;
        font4.setFamily(QString::fromUtf8("Calibri"));
        font4.setPointSize(10);
        frame_3->setFont(font4);
        frame_3->setFrameShape(QFrame::WinPanel);
        frame_3->setFrameShadow(QFrame::Plain);
        frame_3->setLineWidth(4);
        frame_3->setMidLineWidth(4);
        pushButton_6 = new QPushButton(frame_3);
        pushButton_6->setObjectName(QString::fromUtf8("pushButton_6"));
        pushButton_6->setGeometry(QRect(80, 70, 101, 23));
        QFont font5;
        font5.setFamily(QString::fromUtf8("Calibri"));
        font5.setBold(true);
        font5.setWeight(75);
        font5.setStyleStrategy(QFont::PreferAntialias);
        pushButton_6->setFont(font5);
        label_2 = new QLabel(frame_3);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(7, 10, 131, 16));
        label_2->setFont(font1);
        label_2->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        label_3 = new QLabel(frame_3);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(7, 40, 131, 16));
        label_3->setFont(font1);
        label_3->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        sel_reg_linedit = new QLineEdit(frame_3);
        sel_reg_linedit->setObjectName(QString::fromUtf8("sel_reg_linedit"));
        sel_reg_linedit->setGeometry(QRect(80, 40, 101, 20));
        sel_reg_linedit->setFont(font3);
        sel_reg_linedit->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        reg_combo_box = new QComboBox(frame_3);
        reg_combo_box->setObjectName(QString::fromUtf8("reg_combo_box"));
        reg_combo_box->setGeometry(QRect(80, 10, 101, 22));
        reg_combo_box->setFont(font3);
        reg_combo_box->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        label_4 = new QLabel(frame_3);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(260, 10, 131, 16));
        label_4->setFont(font1);
        label_4->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        txt_set_pc = new QLineEdit(frame_3);
        txt_set_pc->setObjectName(QString::fromUtf8("txt_set_pc"));
        txt_set_pc->setGeometry(QRect(320, 10, 101, 20));
        txt_set_pc->setFont(font3);
        txt_set_pc->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        pushButton_7 = new QPushButton(frame_3);
        pushButton_7->setObjectName(QString::fromUtf8("pushButton_7"));
        pushButton_7->setGeometry(QRect(320, 40, 101, 23));
        pushButton_7->setFont(font5);
        chk_sel = new QCheckBox(frame_3);
        chk_sel->setObjectName(QString::fromUtf8("chk_sel"));
        chk_sel->setGeometry(QRect(10, 70, 80, 17));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(chk_sel->sizePolicy().hasHeightForWidth());
        chk_sel->setSizePolicy(sizePolicy);
        chk_sel->setMinimumSize(QSize(80, 0));
        chk_sel->setMaximumSize(QSize(80, 16777215));
        chk_sel->setFont(font1);
        chk_sel->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_sel->setChecked(false);
        btn_export_bin = new QPushButton(frame_3);
        btn_export_bin->setObjectName(QString::fromUtf8("btn_export_bin"));
        btn_export_bin->setGeometry(QRect(320, 70, 101, 23));
        btn_export_bin->setMinimumSize(QSize(100, 0));
        QFont font6;
        font6.setFamily(QString::fromUtf8("Calibri"));
        font6.setPointSize(8);
        font6.setBold(true);
        font6.setWeight(75);
        font6.setStyleStrategy(QFont::PreferAntialias);
        btn_export_bin->setFont(font6);
        chk_ptr = new QCheckBox(frame_3);
        chk_ptr->setObjectName(QString::fromUtf8("chk_ptr"));
        chk_ptr->setGeometry(QRect(10, 90, 80, 17));
        sizePolicy.setHeightForWidth(chk_ptr->sizePolicy().hasHeightForWidth());
        chk_ptr->setSizePolicy(sizePolicy);
        chk_ptr->setMinimumSize(QSize(80, 0));
        chk_ptr->setMaximumSize(QSize(80, 16777215));
        chk_ptr->setFont(font1);
        chk_ptr->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_ptr->setChecked(false);
        pushButton_10 = new QPushButton(frame_3);
        pushButton_10->setObjectName(QString::fromUtf8("pushButton_10"));
        pushButton_10->setGeometry(QRect(80, 100, 101, 23));
        pushButton_10->setFont(font5);

        gridLayout_7->addWidget(frame_3, 0, 1, 1, 1);

        frame_6 = new QFrame(frame_2);
        frame_6->setObjectName(QString::fromUtf8("frame_6"));
        frame_6->setMaximumSize(QSize(16777215, 600));
        frame_6->setFont(font4);
        frame_6->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
""));
        frame_6->setFrameShape(QFrame::WinPanel);
        frame_6->setFrameShadow(QFrame::Plain);
        frame_6->setLineWidth(4);
        frame_6->setMidLineWidth(4);
        gridLayout_8 = new QGridLayout(frame_6);
        gridLayout_8->setObjectName(QString::fromUtf8("gridLayout_8"));
        tabWidget_4 = new QTabWidget(frame_6);
        tabWidget_4->setObjectName(QString::fromUtf8("tabWidget_4"));
        tabWidget_4->setMaximumSize(QSize(16777215, 16777215));
        tabWidget_4->setFont(font1);
        tabWidget_4->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
""));
        tab_10 = new QWidget();
        tab_10->setObjectName(QString::fromUtf8("tab_10"));
        gridLayout_9 = new QGridLayout(tab_10);
        gridLayout_9->setObjectName(QString::fromUtf8("gridLayout_9"));
        lbl_state_data = new QLabel(tab_10);
        lbl_state_data->setObjectName(QString::fromUtf8("lbl_state_data"));
        lbl_state_data->setFont(font1);
        lbl_state_data->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));

        gridLayout_9->addWidget(lbl_state_data, 0, 0, 1, 1);

        tabWidget_4->addTab(tab_10, QString());
        tab_11 = new QWidget();
        tab_11->setObjectName(QString::fromUtf8("tab_11"));
        tabWidget_4->addTab(tab_11, QString());
        tab_12 = new QWidget();
        tab_12->setObjectName(QString::fromUtf8("tab_12"));
        gridLayout_10 = new QGridLayout(tab_12);
        gridLayout_10->setObjectName(QString::fromUtf8("gridLayout_10"));
        list_breakpoints = new QListWidget(tab_12);
        list_breakpoints->setObjectName(QString::fromUtf8("list_breakpoints"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(list_breakpoints->sizePolicy().hasHeightForWidth());
        list_breakpoints->setSizePolicy(sizePolicy1);
        list_breakpoints->setMinimumSize(QSize(0, 150));
        list_breakpoints->setMaximumSize(QSize(200, 100000));
        list_breakpoints->setFont(font2);
        list_breakpoints->setLayoutDirection(Qt::LeftToRight);
        list_breakpoints->setAutoFillBackground(false);
        list_breakpoints->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
""));
        list_breakpoints->setFrameShape(QFrame::WinPanel);
        list_breakpoints->setFrameShadow(QFrame::Plain);
        list_breakpoints->setLineWidth(4);
        list_breakpoints->setMidLineWidth(4);
        list_breakpoints->setSortingEnabled(true);

        gridLayout_10->addWidget(list_breakpoints, 0, 0, 4, 1);

        label_9 = new QLabel(tab_12);
        label_9->setObjectName(QString::fromUtf8("label_9"));
        label_9->setFont(font1);
        label_9->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));

        gridLayout_10->addWidget(label_9, 0, 1, 1, 1);

        txt_rename_breakpoint = new QLineEdit(tab_12);
        txt_rename_breakpoint->setObjectName(QString::fromUtf8("txt_rename_breakpoint"));
        txt_rename_breakpoint->setFont(font3);
        txt_rename_breakpoint->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));

        gridLayout_10->addWidget(txt_rename_breakpoint, 0, 2, 1, 1);

        btn_rename_breakpoint = new QPushButton(tab_12);
        btn_rename_breakpoint->setObjectName(QString::fromUtf8("btn_rename_breakpoint"));
        btn_rename_breakpoint->setFont(font5);

        gridLayout_10->addWidget(btn_rename_breakpoint, 1, 2, 1, 1);

        label_11 = new QLabel(tab_12);
        label_11->setObjectName(QString::fromUtf8("label_11"));
        label_11->setFont(font1);
        label_11->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));

        gridLayout_10->addWidget(label_11, 2, 1, 1, 1);

        txt_manual_add_brk = new QLineEdit(tab_12);
        txt_manual_add_brk->setObjectName(QString::fromUtf8("txt_manual_add_brk"));
        txt_manual_add_brk->setFont(font3);
        txt_manual_add_brk->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));

        gridLayout_10->addWidget(txt_manual_add_brk, 2, 2, 1, 1);

        btn_rename_breakpoint_2 = new QPushButton(tab_12);
        btn_rename_breakpoint_2->setObjectName(QString::fromUtf8("btn_rename_breakpoint_2"));
        btn_rename_breakpoint_2->setFont(font5);

        gridLayout_10->addWidget(btn_rename_breakpoint_2, 3, 2, 1, 1);

        tabWidget_4->addTab(tab_12, QString());
        tab_13 = new QWidget();
        tab_13->setObjectName(QString::fromUtf8("tab_13"));
        reg_combo_box_2 = new QComboBox(tab_13);
        reg_combo_box_2->setObjectName(QString::fromUtf8("reg_combo_box_2"));
        reg_combo_box_2->setGeometry(QRect(110, 10, 101, 22));
        reg_combo_box_2->setFont(font3);
        reg_combo_box_2->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        label_5 = new QLabel(tab_13);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(10, 10, 131, 16));
        label_5->setFont(font1);
        label_5->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        label_6 = new QLabel(tab_13);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setGeometry(QRect(8, 70, 131, 16));
        label_6->setFont(font1);
        label_6->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        sel_reg_linedit_2 = new QLineEdit(tab_13);
        sel_reg_linedit_2->setObjectName(QString::fromUtf8("sel_reg_linedit_2"));
        sel_reg_linedit_2->setGeometry(QRect(110, 70, 101, 20));
        sel_reg_linedit_2->setFont(font3);
        sel_reg_linedit_2->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        label_7 = new QLabel(tab_13);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setGeometry(QRect(10, 40, 131, 16));
        label_7->setFont(font1);
        label_7->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        reg_combo_box_3 = new QComboBox(tab_13);
        reg_combo_box_3->setObjectName(QString::fromUtf8("reg_combo_box_3"));
        reg_combo_box_3->setGeometry(QRect(110, 40, 101, 22));
        reg_combo_box_3->setFont(font3);
        reg_combo_box_3->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        label_8 = new QLabel(tab_13);
        label_8->setObjectName(QString::fromUtf8("label_8"));
        label_8->setGeometry(QRect(8, 100, 131, 16));
        label_8->setFont(font1);
        label_8->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));
        sel_reg_linedit_3 = new QLineEdit(tab_13);
        sel_reg_linedit_3->setObjectName(QString::fromUtf8("sel_reg_linedit_3"));
        sel_reg_linedit_3->setGeometry(QRect(110, 100, 101, 20));
        sel_reg_linedit_3->setFont(font3);
        sel_reg_linedit_3->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));
        pushButton_9 = new QPushButton(tab_13);
        pushButton_9->setObjectName(QString::fromUtf8("pushButton_9"));
        pushButton_9->setGeometry(QRect(110, 130, 101, 23));
        pushButton_9->setFont(font5);
        tabWidget_4->addTab(tab_13, QString());
        tab_15 = new QWidget();
        tab_15->setObjectName(QString::fromUtf8("tab_15"));
        gridLayout_11 = new QGridLayout(tab_15);
        gridLayout_11->setObjectName(QString::fromUtf8("gridLayout_11"));
        label_10 = new QLabel(tab_15);
        label_10->setObjectName(QString::fromUtf8("label_10"));
        label_10->setFont(font1);
        label_10->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));

        gridLayout_11->addWidget(label_10, 0, 0, 1, 1);

        pick_gdrom_field = new QComboBox(tab_15);
        pick_gdrom_field->setObjectName(QString::fromUtf8("pick_gdrom_field"));
        pick_gdrom_field->setFont(font3);
        pick_gdrom_field->setStyleSheet(QString::fromUtf8("background-color:rgb(255,255,255);"));

        gridLayout_11->addWidget(pick_gdrom_field, 0, 1, 1, 1);

        btn_gdfilter_refresh = new QPushButton(tab_15);
        btn_gdfilter_refresh->setObjectName(QString::fromUtf8("btn_gdfilter_refresh"));
        btn_gdfilter_refresh->setFont(font5);

        gridLayout_11->addWidget(btn_gdfilter_refresh, 0, 2, 1, 1);

        list_gd_field = new QListWidget(tab_15);
        list_gd_field->setObjectName(QString::fromUtf8("list_gd_field"));
        list_gd_field->setMaximumSize(QSize(500, 10000));
        list_gd_field->setFont(font1);
        list_gd_field->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);\n"
"color:rgb(0,0,0);"));
        list_gd_field->setFrameShape(QFrame::WinPanel);
        list_gd_field->setFrameShadow(QFrame::Plain);
        list_gd_field->setLineWidth(4);
        list_gd_field->setMidLineWidth(4);
        list_gd_field->setSortingEnabled(true);

        gridLayout_11->addWidget(list_gd_field, 1, 0, 1, 3);

        tabWidget_4->addTab(tab_15, QString());
        tab_14 = new QWidget();
        tab_14->setObjectName(QString::fromUtf8("tab_14"));
        chk_active_2 = new QCheckBox(tab_14);
        chk_active_2->setObjectName(QString::fromUtf8("chk_active_2"));
        chk_active_2->setGeometry(QRect(10, 10, 60, 23));
        chk_active_2->setMaximumSize(QSize(60, 16777215));
        QFont font7;
        font7.setBold(true);
        font7.setWeight(75);
        font7.setStyleStrategy(QFont::PreferAntialias);
        chk_active_2->setFont(font7);
        chk_active_2->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_active_2->setChecked(true);
        tabWidget_4->addTab(tab_14, QString());

        gridLayout_8->addWidget(tabWidget_4, 0, 0, 1, 1);


        gridLayout_7->addWidget(frame_6, 1, 1, 1, 1);

        frame_4 = new QFrame(frame_2);
        frame_4->setObjectName(QString::fromUtf8("frame_4"));
        frame_4->setMaximumSize(QSize(250, 800));
        frame_4->setFont(font4);
        frame_4->setFrameShape(QFrame::WinPanel);
        frame_4->setFrameShadow(QFrame::Plain);
        frame_4->setLineWidth(4);
        frame_4->setMidLineWidth(4);
        gridLayout_6 = new QGridLayout(frame_4);
        gridLayout_6->setObjectName(QString::fromUtf8("gridLayout_6"));
        chk_trace = new QCheckBox(frame_4);
        chk_trace->setObjectName(QString::fromUtf8("chk_trace"));
        sizePolicy.setHeightForWidth(chk_trace->sizePolicy().hasHeightForWidth());
        chk_trace->setSizePolicy(sizePolicy);
        chk_trace->setMinimumSize(QSize(60, 0));
        chk_trace->setMaximumSize(QSize(80, 16777215));
        chk_trace->setFont(font6);
        chk_trace->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_trace->setChecked(true);

        gridLayout_6->addWidget(chk_trace, 0, 0, 1, 1);

        chk_dec = new QCheckBox(frame_4);
        chk_dec->setObjectName(QString::fromUtf8("chk_dec"));
        sizePolicy.setHeightForWidth(chk_dec->sizePolicy().hasHeightForWidth());
        chk_dec->setSizePolicy(sizePolicy);
        chk_dec->setMinimumSize(QSize(60, 0));
        chk_dec->setMaximumSize(QSize(40, 16777215));
        chk_dec->setFont(font6);
        chk_dec->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));

        gridLayout_6->addWidget(chk_dec, 0, 1, 1, 1);

        chk_flt = new QCheckBox(frame_4);
        chk_flt->setObjectName(QString::fromUtf8("chk_flt"));
        sizePolicy.setHeightForWidth(chk_flt->sizePolicy().hasHeightForWidth());
        chk_flt->setSizePolicy(sizePolicy);
        chk_flt->setMinimumSize(QSize(40, 0));
        chk_flt->setMaximumSize(QSize(40, 16777215));
        chk_flt->setFont(font6);
        chk_flt->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));

        gridLayout_6->addWidget(chk_flt, 0, 2, 1, 1);

        chk_str = new QCheckBox(frame_4);
        chk_str->setObjectName(QString::fromUtf8("chk_str"));
        sizePolicy.setHeightForWidth(chk_str->sizePolicy().hasHeightForWidth());
        chk_str->setSizePolicy(sizePolicy);
        chk_str->setMinimumSize(QSize(60, 0));
        chk_str->setMaximumSize(QSize(70, 16777215));
        chk_str->setFont(font6);
        chk_str->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));

        gridLayout_6->addWidget(chk_str, 0, 3, 1, 1);

        chk_hex = new QCheckBox(frame_4);
        chk_hex->setObjectName(QString::fromUtf8("chk_hex"));
        sizePolicy.setHeightForWidth(chk_hex->sizePolicy().hasHeightForWidth());
        chk_hex->setSizePolicy(sizePolicy);
        chk_hex->setMinimumSize(QSize(60, 0));
        chk_hex->setMaximumSize(QSize(40, 16777215));
        chk_hex->setFont(font6);
        chk_hex->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_hex->setChecked(true);

        gridLayout_6->addWidget(chk_hex, 0, 4, 1, 1);

        lbl_reg = new QLabel(frame_4);
        lbl_reg->setObjectName(QString::fromUtf8("lbl_reg"));
        lbl_reg->setMinimumSize(QSize(200, 0));
        lbl_reg->setFont(font1);
        lbl_reg->setStyleSheet(QString::fromUtf8("background-color:rgba(0,0,0,0);"));

        gridLayout_6->addWidget(lbl_reg, 1, 0, 1, 5);


        gridLayout_7->addWidget(frame_4, 0, 0, 2, 1);


        gridLayout_5->addWidget(frame_2, 1, 1, 1, 1);

        frame = new QFrame(tab_3);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFont(font3);
        frame->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,100);\n"
"color:rgb(0,0,0);"));
        frame->setFrameShape(QFrame::WinPanel);
        frame->setFrameShadow(QFrame::Plain);
        frame->setLineWidth(4);
        frame->setMidLineWidth(4);
        gridLayout_4 = new QGridLayout(frame);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        pushButton = new QPushButton(frame);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        pushButton->setMaximumSize(QSize(60, 16777215));
        pushButton->setFont(font5);
        pushButton->setStyleSheet(QString::fromUtf8(""));

        gridLayout_4->addWidget(pushButton, 0, 0, 1, 1);

        pushButton_12 = new QPushButton(frame);
        pushButton_12->setObjectName(QString::fromUtf8("pushButton_12"));
        pushButton_12->setMaximumSize(QSize(60, 16777215));
        pushButton_12->setFont(font5);
        pushButton_12->setStyleSheet(QString::fromUtf8(""));

        gridLayout_4->addWidget(pushButton_12, 0, 1, 1, 1);

        pushButton_11 = new QPushButton(frame);
        pushButton_11->setObjectName(QString::fromUtf8("pushButton_11"));
        pushButton_11->setMaximumSize(QSize(60, 16777215));
        pushButton_11->setFont(font5);

        gridLayout_4->addWidget(pushButton_11, 0, 2, 1, 1);

        pushButton_13 = new QPushButton(frame);
        pushButton_13->setObjectName(QString::fromUtf8("pushButton_13"));
        pushButton_13->setMaximumSize(QSize(60, 16777215));
        pushButton_13->setFont(font5);
        pushButton_13->setStyleSheet(QString::fromUtf8(""));

        gridLayout_4->addWidget(pushButton_13, 0, 3, 1, 1);

        btn_brk_add = new QPushButton(frame);
        btn_brk_add->setObjectName(QString::fromUtf8("btn_brk_add"));
        btn_brk_add->setMaximumSize(QSize(60, 16777215));
        btn_brk_add->setFont(font5);

        gridLayout_4->addWidget(btn_brk_add, 0, 4, 1, 1);

        btn_brk_rm = new QPushButton(frame);
        btn_brk_rm->setObjectName(QString::fromUtf8("btn_brk_rm"));
        btn_brk_rm->setMaximumSize(QSize(60, 16777215));
        btn_brk_rm->setFont(font5);

        gridLayout_4->addWidget(btn_brk_rm, 0, 5, 1, 1);

        pushButton_4 = new QPushButton(frame);
        pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));
        pushButton_4->setMaximumSize(QSize(60, 16777215));
        pushButton_4->setFont(font5);

        gridLayout_4->addWidget(pushButton_4, 0, 6, 1, 1);

        pushButton_5 = new QPushButton(frame);
        pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));
        pushButton_5->setMaximumSize(QSize(60, 16777215));
        pushButton_5->setFont(font5);
        pushButton_5->setStyleSheet(QString::fromUtf8(""));

        gridLayout_4->addWidget(pushButton_5, 0, 7, 1, 1);

        chk_active = new QCheckBox(frame);
        chk_active->setObjectName(QString::fromUtf8("chk_active"));
        chk_active->setMaximumSize(QSize(60, 16777215));
        chk_active->setFont(font7);
        chk_active->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_active->setChecked(true);

        gridLayout_4->addWidget(chk_active, 0, 8, 1, 1);

        chk_sort_code_path = new QCheckBox(frame);
        chk_sort_code_path->setObjectName(QString::fromUtf8("chk_sort_code_path"));
        chk_sort_code_path->setMinimumSize(QSize(110, 0));
        chk_sort_code_path->setMaximumSize(QSize(110, 16777215));
        chk_sort_code_path->setFont(font7);
        chk_sort_code_path->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));
        chk_sort_code_path->setChecked(true);
        chk_sort_code_path->setTristate(true);

        gridLayout_4->addWidget(chk_sort_code_path, 0, 9, 1, 1);

        checkBox_3 = new QCheckBox(frame);
        checkBox_3->setObjectName(QString::fromUtf8("checkBox_3"));
        checkBox_3->setFont(font7);
        checkBox_3->setStyleSheet(QString::fromUtf8("background-color:rgba(255,255,255,0);"));

        gridLayout_4->addWidget(checkBox_3, 0, 10, 1, 1);


        gridLayout_5->addWidget(frame, 0, 0, 1, 2);

        tabWidget_2->addTab(tab_3, QString());
        tab_4 = new QWidget();
        tab_4->setObjectName(QString::fromUtf8("tab_4"));
        tabWidget_2->addTab(tab_4, QString());

        gridLayout_2->addWidget(tabWidget_2, 0, 0, 1, 1);

        tabWidget->addTab(tab, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QString::fromUtf8("tab_2"));
        gridLayout_3 = new QGridLayout(tab_2);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        tabWidget_3 = new QTabWidget(tab_2);
        tabWidget_3->setObjectName(QString::fromUtf8("tabWidget_3"));
        tabWidget_3->setFont(font1);
        tab_5 = new QWidget();
        tab_5->setObjectName(QString::fromUtf8("tab_5"));
        tabWidget_3->addTab(tab_5, QString());
        tab_6 = new QWidget();
        tab_6->setObjectName(QString::fromUtf8("tab_6"));
        tabWidget_3->addTab(tab_6, QString());
        tab_7 = new QWidget();
        tab_7->setObjectName(QString::fromUtf8("tab_7"));
        tabWidget_3->addTab(tab_7, QString());
        tab_8 = new QWidget();
        tab_8->setObjectName(QString::fromUtf8("tab_8"));
        tabWidget_3->addTab(tab_8, QString());

        gridLayout_3->addWidget(tabWidget_3, 0, 0, 1, 1);

        tabWidget->addTab(tab_2, QString());
        tab_9 = new QWidget();
        tab_9->setObjectName(QString::fromUtf8("tab_9"));
        scripts_list = new QListWidget(tab_9);
        scripts_list->setObjectName(QString::fromUtf8("scripts_list"));
        scripts_list->setGeometry(QRect(10, 10, 331, 551));
        QFont font8;
        font8.setBold(true);
        font8.setWeight(75);
        scripts_list->setFont(font8);
        pushButton_2 = new QPushButton(tab_9);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
        pushButton_2->setGeometry(QRect(350, 10, 60, 23));
        pushButton_2->setMaximumSize(QSize(60, 16777215));
        pushButton_2->setFont(font5);
        pushButton_2->setStyleSheet(QString::fromUtf8(""));
        script_log = new QTextEdit(tab_9);
        script_log->setObjectName(QString::fromUtf8("script_log"));
        script_log->setGeometry(QRect(350, 40, 861, 521));
        tabWidget->addTab(tab_9, QString());

        gridLayout->addWidget(tabWidget, 0, 0, 1, 1);


        retranslateUi(wnd_main);

        tabWidget->setCurrentIndex(0);
        tabWidget_2->setCurrentIndex(0);
        tabWidget_4->setCurrentIndex(2);
        tabWidget_3->setCurrentIndex(3);


        QMetaObject::connectSlotsByName(wnd_main);
    } // setupUi

    void retranslateUi(QWidget *wnd_main)
    {
        wnd_main->setWindowTitle(QApplication::translate("wnd_main", "ReiDBG", nullptr));
        pushButton_6->setText(QApplication::translate("wnd_main", "WRITE", nullptr));
        label_2->setText(QApplication::translate("wnd_main", "Register", nullptr));
        label_3->setText(QApplication::translate("wnd_main", "Value:", nullptr));
        label_4->setText(QApplication::translate("wnd_main", "Jump to:", nullptr));
        pushButton_7->setText(QApplication::translate("wnd_main", "JMP", nullptr));
        chk_sel->setText(QApplication::translate("wnd_main", "SEL", nullptr));
        btn_export_bin->setText(QApplication::translate("wnd_main", "EXPORT BINARY", nullptr));
        chk_ptr->setText(QApplication::translate("wnd_main", "PTR", nullptr));
        pushButton_10->setText(QApplication::translate("wnd_main", "READ", nullptr));
        lbl_state_data->setText(QApplication::translate("wnd_main", "State data", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_10), QApplication::translate("wnd_main", "CPU", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_11), QApplication::translate("wnd_main", "MEM", nullptr));
        label_9->setText(QApplication::translate("wnd_main", "Rename:", nullptr));
        btn_rename_breakpoint->setText(QApplication::translate("wnd_main", "Rename", nullptr));
        label_11->setText(QApplication::translate("wnd_main", "Add:", nullptr));
        btn_rename_breakpoint_2->setText(QApplication::translate("wnd_main", "Manual add", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_12), QApplication::translate("wnd_main", "BREAKPOINTS", nullptr));
        label_5->setText(QApplication::translate("wnd_main", "Mode:", nullptr));
        label_6->setText(QApplication::translate("wnd_main", "Address:", nullptr));
        label_7->setText(QApplication::translate("wnd_main", "Type:", nullptr));
        label_8->setText(QApplication::translate("wnd_main", "Alias:", nullptr));
        pushButton_9->setText(QApplication::translate("wnd_main", "ADD", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_13), QApplication::translate("wnd_main", "WATCHERS", nullptr));
        label_10->setText(QApplication::translate("wnd_main", "Filter", nullptr));
        btn_gdfilter_refresh->setText(QApplication::translate("wnd_main", "REFRESH", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_15), QApplication::translate("wnd_main", "GDROM", nullptr));
        chk_active_2->setText(QApplication::translate("wnd_main", "Capture HLE results", nullptr));
        tabWidget_4->setTabText(tabWidget_4->indexOf(tab_14), QApplication::translate("wnd_main", "HLE Tools", nullptr));
        chk_trace->setText(QApplication::translate("wnd_main", "TRC", nullptr));
        chk_dec->setText(QApplication::translate("wnd_main", "DEC", nullptr));
        chk_flt->setText(QApplication::translate("wnd_main", "FLT", nullptr));
        chk_str->setText(QApplication::translate("wnd_main", "STR", nullptr));
        chk_hex->setText(QApplication::translate("wnd_main", "HEX", nullptr));
        lbl_reg->setText(QApplication::translate("wnd_main", "DATA", nullptr));
        pushButton->setText(QApplication::translate("wnd_main", "STEP", nullptr));
        pushButton_12->setText(QApplication::translate("wnd_main", "START", nullptr));
        pushButton_11->setText(QApplication::translate("wnd_main", "STOP", nullptr));
        pushButton_13->setText(QApplication::translate("wnd_main", "RESET", nullptr));
        btn_brk_add->setText(QApplication::translate("wnd_main", "+BRK", nullptr));
        btn_brk_rm->setText(QApplication::translate("wnd_main", "-BRK", nullptr));
        pushButton_4->setText(QApplication::translate("wnd_main", "TRACE-A", nullptr));
        pushButton_5->setText(QApplication::translate("wnd_main", "TRACE-B", nullptr));
        chk_active->setText(QApplication::translate("wnd_main", "Active", nullptr));
        chk_sort_code_path->setText(QApplication::translate("wnd_main", "Sort code path", nullptr));
        checkBox_3->setText(QApplication::translate("wnd_main", "Full state capture per cycle", nullptr));
        tabWidget_2->setTabText(tabWidget_2->indexOf(tab_3), QApplication::translate("wnd_main", "Instructions", nullptr));
        tabWidget_2->setTabText(tabWidget_2->indexOf(tab_4), QApplication::translate("wnd_main", "Breakpoints", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("wnd_main", "CPU", nullptr));
        tabWidget_3->setTabText(tabWidget_3->indexOf(tab_5), QApplication::translate("wnd_main", "RAM", nullptr));
        tabWidget_3->setTabText(tabWidget_3->indexOf(tab_6), QApplication::translate("wnd_main", "PVR", nullptr));
        tabWidget_3->setTabText(tabWidget_3->indexOf(tab_7), QApplication::translate("wnd_main", "AICA", nullptr));
        tabWidget_3->setTabText(tabWidget_3->indexOf(tab_8), QApplication::translate("wnd_main", "GDROM", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("wnd_main", "Memory / IO", nullptr));
        pushButton_2->setText(QApplication::translate("wnd_main", "Refresh", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_9), QApplication::translate("wnd_main", "Scripts", nullptr));
    } // retranslateUi

};

namespace Ui {
    class wnd_main: public Ui_wnd_main {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WND_MAIN_H
