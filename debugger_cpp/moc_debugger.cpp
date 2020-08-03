/****************************************************************************
** Meta object code from reading C++ file 'debugger.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "debugger.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'debugger.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_gui_ctl_c_t {
    QByteArrayData data[12];
    char stringdata0[117];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_gui_ctl_c_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_gui_ctl_c_t qt_meta_stringdata_gui_ctl_c = {
    {
QT_MOC_LITERAL(0, 0, 9), // "gui_ctl_c"
QT_MOC_LITERAL(1, 10, 10), // "qpass_data"
QT_MOC_LITERAL(2, 21, 0), // ""
QT_MOC_LITERAL(3, 22, 11), // "const char*"
QT_MOC_LITERAL(4, 34, 10), // "class_name"
QT_MOC_LITERAL(5, 45, 4), // "data"
QT_MOC_LITERAL(6, 50, 8), // "uint32_t"
QT_MOC_LITERAL(7, 59, 2), // "sz"
QT_MOC_LITERAL(8, 62, 19), // "qpass_data_blocking"
QT_MOC_LITERAL(9, 82, 9), // "qpush_msg"
QT_MOC_LITERAL(10, 92, 5), // "which"
QT_MOC_LITERAL(11, 98, 18) // "qpush_msg_blocking"

    },
    "gui_ctl_c\0qpass_data\0\0const char*\0"
    "class_name\0data\0uint32_t\0sz\0"
    "qpass_data_blocking\0qpush_msg\0which\0"
    "qpush_msg_blocking"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_gui_ctl_c[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    3,   34,    2, 0x06 /* Public */,
       8,    3,   41,    2, 0x06 /* Public */,
       9,    2,   48,    2, 0x06 /* Public */,
      11,    2,   53,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::VoidStar, 0x80000000 | 6,    4,    5,    7,
    QMetaType::Void, 0x80000000 | 3, QMetaType::VoidStar, 0x80000000 | 6,    4,    5,    7,
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 3,   10,    5,
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 3,   10,    5,

       0        // eod
};

void gui_ctl_c::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<gui_ctl_c *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->qpass_data((*reinterpret_cast< const char*(*)>(_a[1])),(*reinterpret_cast< void*(*)>(_a[2])),(*reinterpret_cast< const uint32_t(*)>(_a[3]))); break;
        case 1: _t->qpass_data_blocking((*reinterpret_cast< const char*(*)>(_a[1])),(*reinterpret_cast< void*(*)>(_a[2])),(*reinterpret_cast< const uint32_t(*)>(_a[3]))); break;
        case 2: _t->qpush_msg((*reinterpret_cast< const char*(*)>(_a[1])),(*reinterpret_cast< const char*(*)>(_a[2]))); break;
        case 3: _t->qpush_msg_blocking((*reinterpret_cast< const char*(*)>(_a[1])),(*reinterpret_cast< const char*(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (gui_ctl_c::*)(const char * , void * , const uint32_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&gui_ctl_c::qpass_data)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (gui_ctl_c::*)(const char * , void * , const uint32_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&gui_ctl_c::qpass_data_blocking)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (gui_ctl_c::*)(const char * , const char * );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&gui_ctl_c::qpush_msg)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (gui_ctl_c::*)(const char * , const char * );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&gui_ctl_c::qpush_msg_blocking)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject gui_ctl_c::staticMetaObject = { {
    &QObject::staticMetaObject,
    qt_meta_stringdata_gui_ctl_c.data,
    qt_meta_data_gui_ctl_c,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *gui_ctl_c::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *gui_ctl_c::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_gui_ctl_c.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int gui_ctl_c::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void gui_ctl_c::qpass_data(const char * _t1, void * _t2, const uint32_t _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void gui_ctl_c::qpass_data_blocking(const char * _t1, void * _t2, const uint32_t _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void gui_ctl_c::qpush_msg(const char * _t1, const char * _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void gui_ctl_c::qpush_msg_blocking(const char * _t1, const char * _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
