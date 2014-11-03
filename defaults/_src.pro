TARGET = view
TEMPLATE = lib
CONFIG += shared
QT += network xml
QT -= gui
DEFINES += TF_DLL
INCLUDEPATH += ../../helpers ../../models
DEPENDPATH  += ../../helpers ../../models
DESTDIR = ../../lib
LIBS += -L../../lib -lhelper -lmodel
QMAKE_CLEAN = *.cpp source.list

tmake.target = source.list
tmake.commands = tmake -f ../../config/application.ini -v .. -d . -P
tmake.depends = qmake
QMAKE_EXTRA_TARGETS = tmake
PRE_TARGETDEPS = source.list

include(../../appbase.pri)
!exists(source.list) {
  system( $$tmake.commands )
}
include(source.list)
