/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10MainWindowE = QtMocHelpers::stringData(
    "MainWindow",
    "openAudioFile",
    "",
    "saveAudioFile",
    "playAudio",
    "stopAudio",
    "updateTime",
    "updateBPM",
    "increaseBPM",
    "decreaseBPM",
    "updateTimeLabel",
    "msPosition",
    "updatePlaybackPosition",
    "position",
    "toggleMetronome",
    "toggleLoop",
    "updateLoopPoints",
    "showMetronomeSettings",
    "setColorScheme",
    "scheme",
    "setTheme",
    "theme",
    "showKeyboardShortcuts",
    "setLoopStart",
    "setLoopEnd"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      19,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  128,    2, 0x08,    1 /* Private */,
       3,    0,  129,    2, 0x08,    2 /* Private */,
       4,    0,  130,    2, 0x08,    3 /* Private */,
       5,    0,  131,    2, 0x08,    4 /* Private */,
       6,    0,  132,    2, 0x08,    5 /* Private */,
       7,    0,  133,    2, 0x08,    6 /* Private */,
       8,    0,  134,    2, 0x08,    7 /* Private */,
       9,    0,  135,    2, 0x08,    8 /* Private */,
      10,    1,  136,    2, 0x08,    9 /* Private */,
      12,    1,  139,    2, 0x08,   11 /* Private */,
      14,    0,  142,    2, 0x08,   13 /* Private */,
      15,    0,  143,    2, 0x08,   14 /* Private */,
      16,    0,  144,    2, 0x08,   15 /* Private */,
      17,    0,  145,    2, 0x08,   16 /* Private */,
      18,    1,  146,    2, 0x08,   17 /* Private */,
      20,    1,  149,    2, 0x08,   19 /* Private */,
      22,    0,  152,    2, 0x08,   21 /* Private */,
      23,    0,  153,    2, 0x08,   22 /* Private */,
      24,    0,  154,    2, 0x08,   23 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong,   11,
    QMetaType::Void, QMetaType::LongLong,   13,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'openAudioFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveAudioFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'playAudio'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stopAudio'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateTime'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateBPM'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'increaseBPM'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'decreaseBPM'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateTimeLabel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'updatePlaybackPosition'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'toggleMetronome'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleLoop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateLoopPoints'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showMetronomeSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setColorScheme'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setTheme'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'showKeyboardShortcuts'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setLoopStart'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setLoopEnd'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->openAudioFile(); break;
        case 1: _t->saveAudioFile(); break;
        case 2: _t->playAudio(); break;
        case 3: _t->stopAudio(); break;
        case 4: _t->updateTime(); break;
        case 5: _t->updateBPM(); break;
        case 6: _t->increaseBPM(); break;
        case 7: _t->decreaseBPM(); break;
        case 8: _t->updateTimeLabel((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 9: _t->updatePlaybackPosition((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 10: _t->toggleMetronome(); break;
        case 11: _t->toggleLoop(); break;
        case 12: _t->updateLoopPoints(); break;
        case 13: _t->showMetronomeSettings(); break;
        case 14: _t->setColorScheme((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->setTheme((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->showKeyboardShortcuts(); break;
        case 17: _t->setLoopStart(); break;
        case 18: _t->setLoopEnd(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 19;
    }
    return _id;
}
QT_WARNING_POP
