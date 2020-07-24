QT += core gui widgets

TEMPLATE = lib
DEFINES += DEBUGGER_LIBRARY

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    debugger.cpp \
    script/angelscript/source/as_atomic.cpp \
    script/angelscript/source/as_builder.cpp \
    script/angelscript/source/as_bytecode.cpp \
    script/angelscript/source/as_callfunc.cpp \
    script/angelscript/source/as_callfunc_arm.cpp \
    script/angelscript/source/as_callfunc_mips.cpp \
    script/angelscript/source/as_callfunc_ppc.cpp \
    script/angelscript/source/as_callfunc_ppc_64.cpp \
    script/angelscript/source/as_callfunc_sh4.cpp \
    script/angelscript/source/as_callfunc_x64_gcc.cpp \
    script/angelscript/source/as_callfunc_x64_mingw.cpp \
    script/angelscript/source/as_callfunc_x64_msvc.cpp \
    script/angelscript/source/as_callfunc_x86.cpp \
    script/angelscript/source/as_callfunc_xenon.cpp \
    script/angelscript/source/as_compiler.cpp \
    script/angelscript/source/as_configgroup.cpp \
    script/angelscript/source/as_context.cpp \
    script/angelscript/source/as_datatype.cpp \
    script/angelscript/source/as_gc.cpp \
    script/angelscript/source/as_generic.cpp \
    script/angelscript/source/as_globalproperty.cpp \
    script/angelscript/source/as_memory.cpp \
    script/angelscript/source/as_module.cpp \
    script/angelscript/source/as_objecttype.cpp \
    script/angelscript/source/as_outputbuffer.cpp \
    script/angelscript/source/as_parser.cpp \
    script/angelscript/source/as_restore.cpp \
    script/angelscript/source/as_scriptcode.cpp \
    script/angelscript/source/as_scriptengine.cpp \
    script/angelscript/source/as_scriptfunction.cpp \
    script/angelscript/source/as_scriptnode.cpp \
    script/angelscript/source/as_scriptobject.cpp \
    script/angelscript/source/as_string.cpp \
    script/angelscript/source/as_string_util.cpp \
    script/angelscript/source/as_thread.cpp \
    script/angelscript/source/as_tokenizer.cpp \
    script/angelscript/source/as_typeinfo.cpp \
    script/angelscript/source/as_variablescope.cpp \
    wnd_main.cpp

HEADERS += \
    ../libswirl/utils/string_utils.hpp \
    debugger_global.h \
    debugger.h \
    debugger_interface.h \
    script/angelscript/include/angelscript.h \
    script/angelscript/source/as_array.h \
    script/angelscript/source/as_atomic.h \
    script/angelscript/source/as_builder.h \
    script/angelscript/source/as_bytecode.h \
    script/angelscript/source/as_callfunc.h \
    script/angelscript/source/as_callfunc_arm_gcc.S \
    script/angelscript/source/as_callfunc_arm_vita.S \
    script/angelscript/source/as_callfunc_arm_xcode.S \
    script/angelscript/source/as_compiler.h \
    script/angelscript/source/as_config.h \
    script/angelscript/source/as_configgroup.h \
    script/angelscript/source/as_context.h \
    script/angelscript/source/as_criticalsection.h \
    script/angelscript/source/as_datatype.h \
    script/angelscript/source/as_debug.h \
    script/angelscript/source/as_gc.h \
    script/angelscript/source/as_generic.h \
    script/angelscript/source/as_map.h \
    script/angelscript/source/as_memory.h \
    script/angelscript/source/as_module.h \
    script/angelscript/source/as_namespace.h \
    script/angelscript/source/as_objecttype.h \
    script/angelscript/source/as_outputbuffer.h \
    script/angelscript/source/as_parser.h \
    script/angelscript/source/as_property.h \
    script/angelscript/source/as_restore.h \
    script/angelscript/source/as_scriptcode.h \
    script/angelscript/source/as_scriptengine.h \
    script/angelscript/source/as_scriptfunction.h \
    script/angelscript/source/as_scriptnode.h \
    script/angelscript/source/as_scriptobject.h \
    script/angelscript/source/as_string.h \
    script/angelscript/source/as_string_util.h \
    script/angelscript/source/as_symboltable.h \
    script/angelscript/source/as_texts.h \
    script/angelscript/source/as_thread.h \
    script/angelscript/source/as_tokendef.h \
    script/angelscript/source/as_tokenizer.h \
    script/angelscript/source/as_typeinfo.h \
    script/angelscript/source/as_variablescope.h \
    shared_contexts.h \
    wnd_main.h

TRANSLATIONS += \
    debugger_en_150.ts

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

FORMS += \
    wnd_main.ui

DISTFILES += \
    script/angelscript/source/as_callfunc_arm_msvc.asm \
    script/angelscript/source/as_callfunc_x64_msvc_asm.asm
