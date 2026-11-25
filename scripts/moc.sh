#!/bin/bash
# Qt 4.8 meta-object compiler (moc) for the build host, so the app can have real C++ classes behind its QML.
# Built by hand from the Qt 4.8.7 source (bootstrap subset of QtCore + moc), no configure run.  -> /b/host/moc
set -e
mkdir -p /b/work/qt4 /b/dl; cd /b/dl
[ -f qt4.tar.gz ] || curl -fsSL -o qt4.tar.gz https://download.qt.io/archive/qt/4.8/4.8.7/qt-everywhere-opensource-src-4.8.7.tar.gz
cd /b/work/qt4
[ -d src/tools/moc ] || tar xzf /b/dl/qt4.tar.gz --strip-components=1 --wildcards '*/src/tools/moc/*' '*/src/tools/bootstrap/*' '*/src/corelib/*' '*/src/3rdparty/*' '*/src/xml/*' '*/include/*' '*/mkspecs/*'
printf '#define Q_BIG_ENDIAN 4321\n#define Q_LITTLE_ENDIAN 1234\n#define Q_BYTE_ORDER Q_LITTLE_ENDIAN\n#define QT_EDITION QT_EDITION_OPENSOURCE\n#define QT_POINTER_SIZE 8\n#define QT_ARCH_X86_64\n#define QT_BUILD_KEY "bootstrap"\n#define QT_LARGEFILE_SUPPORT 64\n' > src/corelib/global/qconfig.h
cat > src/corelib/global/qconfig.cpp <<'QEOF'
static const char qt_configure_licensee_str[256 + 12] = "qt_lcnsuser=Open Source";
static const char qt_configure_licensed_products_str[256 + 12] = "qt_lcnsprod=OpenSource";
static const char qt_configure_installation[12 + 11] = "qt_instdate=2026-01-01";
#define QT_CONFIGURE_LICENSEE qt_configure_licensee_str + 12;
#define QT_CONFIGURE_LICENSED_PRODUCTS qt_configure_licensed_products_str + 12;
QEOF
mkdir -p obj; C=src/corelib
SRC="$C/codecs/qisciicodec.cpp $C/codecs/qlatincodec.cpp $C/codecs/qsimplecodec.cpp $C/codecs/qtextcodec.cpp $C/codecs/qtsciicodec.cpp $C/codecs/qutfcodec.cpp
 $C/global/qglobal.cpp $C/global/qmalloc.cpp $C/global/qnumeric.cpp $C/io/qabstractfileengine.cpp $C/io/qbuffer.cpp $C/io/qdatastream.cpp $C/io/qdir.cpp
 $C/io/qdiriterator.cpp $C/io/qfile.cpp $C/io/qfileinfo.cpp $C/io/qfilesystementry.cpp $C/io/qfilesystemengine.cpp $C/io/qfsfileengine.cpp
 $C/io/qfsfileengine_iterator.cpp $C/io/qiodevice.cpp $C/io/qtemporaryfile.cpp $C/io/qtextstream.cpp $C/io/qurl.cpp $C/kernel/qmetatype.cpp
 $C/kernel/qvariant.cpp $C/kernel/qsystemerror.cpp $C/tools/qbitarray.cpp $C/tools/qbytearray.cpp $C/tools/qbytearraymatcher.cpp $C/tools/qdatetime.cpp
 $C/tools/qhash.cpp $C/tools/qlist.cpp $C/tools/qlocale.cpp $C/tools/qlocale_tools.cpp $C/tools/qmap.cpp $C/tools/qregexp.cpp $C/tools/qstring.cpp
 $C/tools/qstringlist.cpp $C/tools/qvector.cpp $C/tools/qvsnprintf.cpp $C/io/qfilesystemengine_unix.cpp $C/io/qfilesystemiterator_unix.cpp
 $C/io/qfsfileengine_unix.cpp $C/tools/qlocale_unix.cpp
 src/tools/moc/moc.cpp src/tools/moc/preprocessor.cpp src/tools/moc/generator.cpp src/tools/moc/parser.cpp src/tools/moc/token.cpp src/tools/moc/main.cpp"
FL="-std=gnu++98 -O1 -w -fpermissive -DQT_BOOTSTRAPPED -DQT_LITE_UNICODE -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CODECS -DQT_NO_DATASTREAM -DQT_NO_GEOM_VARIANT
 -DQT_NO_LIBRARY -DQT_NO_QOBJECT -DQT_NO_STL -DQT_NO_SYSTEMLOCALE -DQT_NO_TEXTSTREAM -DQT_NO_THREAD -DQT_NO_UNICODETABLES -DQT_NO_USING_NAMESPACE -DQT_NO_DEPRECATED -DQT_MOC
 -DQT_NODLL -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE
 -Iinclude -Iinclude/QtCore -Iinclude/QtXml -Isrc/corelib/global -Isrc/tools/moc -Isrc/3rdparty/harfbuzz/src -Imkspecs/linux-g++ -Isrc/corelib/tools"
fail=0
for f in $SRC; do o=obj/$(basename ${f%.cpp}).o; [ -f $o ] || g++ $FL -c $f -o $o 2> obj/err.txt || { echo "FAILED $f"; grep -m3 "error" obj/err.txt; fail=1; break; }; done
[ $fail = 0 ] || exit 1
g++ obj/*.o -lz -o /b/host/moc 2> obj/link.txt || { grep -o "undefined reference to .*" obj/link.txt | sort | uniq -c | sort -rn | head -30; exit 1; }
printf '#include <QObject>\nclass A : public QObject { Q_OBJECT\n Q_PROPERTY(int x READ x NOTIFY xChanged)\npublic: int x() const; Q_INVOKABLE void f(const QString &s);\nQ_SIGNALS: void xChanged(); };\n' > /tmp/a.h
/b/host/moc /tmp/a.h | grep -n "revision\|qt_static_metacall\|Q_MOC_OUTPUT_REVISION" | head -5
