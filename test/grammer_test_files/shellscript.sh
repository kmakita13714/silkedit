#!/bin/bash

# Constants

API_DOC_DIR=jsdoc_out
VERSION=$(grep -E -o "[0-9]+\.[0-9]+\.[0-9]+" ./src/version.h)

if [ "$(uname)" == 'Darwin' ]; then
  OS='Mac'
elif [ "$(expr substr $(uname -s) 1 5)" == 'Linux' ]; then
  OS='Linux'
elif [ "$(expr substr $(uname -s) 1 4)" == 'MSYS' ]; then
  if [ "$(uname -m)" == "x86_64" ]; then
    OS='Win64'
    MSVC="Visual Studio 14 2015 Win64"
    ONIG_BUILD_BAT='"C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat" amd64 & "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\bin\\amd64\\nmake"'
    VCVARSALL_ARG=amd64
    PROGRAM_FILES="Program Files (x86)"
    ARCH=x64
  else
    OS='Win32'
    MSVC="Visual Studio 14 2015"
    ONIG_BUILD_BAT='"C:\\Program Files\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat" x86 & "C:\\Program Files\\Microsoft Visual Studio 14.0\\VC\\bin\\nmake"'
    VCVARSALL_ARG=x86
    ARCH=x86
    PROGRAM_FILES="Program Files"
  fi

  # Add path to MSBuild.exe and jom.exe
  PATH="/c/$PROGRAM_FILES/MSBuild/14.0/Bin":/c/jom:$PATH
  UCRT_DIR="/c/$PROGRAM_FILES/Microsoft Visual Studio 14.0/Common7/IDE/Remote Debugger/$ARCH"
  VCRT_DIR="/c/$PROGRAM_FILES/Microsoft Visual Studio 14.0/VC/redist/$ARCH/Microsoft.VC140.CRT"
  VisualStudioVersion=14.0
elif [ "$(expr substr $(uname -s) 1 5)" == 'MINGW' ]; then
  echo "You're running this program on MINGW. Please run it from msys2_shell.bat"
  exit 1
else
  echo "Your platform ($(uname -a)) is not supported."
  exit 1
fi

format() {
    find src -name \*.cpp -or -name \*.h | xargs clang-format -i
    find test/src -name \*.cpp -or -name \*.h | xargs clang-format -i
    if [ "$1" = '-commit' -o "$1" = '-push' ]; then
        git commit -am "clang-format"
    fi

    if [ "$1" = '-push' ]; then
        if git status | grep -sq "Your branch is ahead of"; then
            git push origin HEAD:master
        fi
        exit 0
    fi
}

# Create a new directory with the specified name if it doesn't exist.
# Recreate it if it already exists.
ensure_dir() {
    if [ -d $1 ]; then
        echo "Recreating $1 directory..."
        rm -rf $1
    else
        echo "Creating $1 directory..."
    fi

    mkdir -p $1
}

clean() {
    # Somehow git clean fails to remove these directories, so remove them manually
    rm -rf build helper/bin

    # clean submodules
    git submodule foreach git reset --hard HEAD
    git submodule foreach git clean -xdf

    # remove everything not tracked in git
    git clean -xdf
}

# resolve dependency
resolve() {
    # Update submodules
    git submodule sync
    git submodule update --init
    
    # jsdoc
    npm install -g jsdoc@latest

    # install built-in packages
    npm install --prefix packages --production https://github.com/silkedit/vim/tarball/0.1.6
    if [ ${OS} == 'Mac' ]; then
      npm install --prefix packages --production https://github.com/silkedit/markdown_preview/tarball/0.1.1
    fi

    # Source Han Code JP font
    if [ ! -f resources/fonts/SourceHanCodeJP-Normal.otf ]; then
        wget silksmiths.sakura.ne.jp/files/SourceHanCodeJP-Normal.otf silksmiths.sakura.ne.jp/files/SourceHanCodeJP-Regular.otf silksmiths.sakura.ne.jp/files/SourceHanCodeJP-Bold.otf
        mkdir -p resources/fonts
        mv *.otf resources/fonts
    fi

    if [ $OS == 'Mac' ]; then
      brew list yaml-cpp || brew install yaml-cpp --c++11 --with-static-lib
      brew list cmake || brew install cmake
      brew list ninja || brew install ninja
      brew list gnu-sed || brew install gnu-sed --with-default-names
      brew list uchardet || brew install uchardet
      brew list boost || brew install boost --c++11

      # Onigmo
      if [ ! -f /usr/local/lib/libonig.a ]; then
        cd Onigmo
        ./configure && make && make install
        cd ..
      fi

      # breakpad
      if [ ! -d breakpad/src/client/mac/build/Release/Breakpad.framework ]; then
        echo 'Building Breakpad client ...'

        cd breakpad
        xcodebuild -project ./src/client/mac/Breakpad.xcodeproj -sdk macosx -target Breakpad ARCHS=x86_64 GCC_VERSION=com.apple.compilers.llvm.clang.1_0 OTHER_CFLAGS=-stdlib=libc++ OTHER_LDFLAGS=-stdlib=libc++ ONLY_ACTIVE_ARCH=YES -configuration Debug
        xcodebuild -project ./src/client/mac/Breakpad.xcodeproj -sdk macosx -target Breakpad ARCHS=x86_64 GCC_VERSION=com.apple.compilers.llvm.clang.1_0 OTHER_CFLAGS=-stdlib=libc++ OTHER_LDFLAGS=-stdlib=libc++ ONLY_ACTIVE_ARCH=YES -configuration Release
        cd ..
      fi

      # Download BugSplat breakpad to send a symbol using dump_sdk_symbols (symupload can't be built on Mac)
      mkdir -p bugsplat
      cd bugsplat
      wget http://silksmiths.sakura.ne.jp/files/Breakpad.framework.zip
      unzip -o Breakpad.framework.zip
      rm Breakpad.framework.zip
      cd ..

			# build libnode.dylib
			cd vendor/node
			# Build Release version
			./configure --enable-shared
			make -j8
      # Build Debug version
			./configure --debug --enable-shared
			make -j8
			mv out/Debug/libnode.dylib out/Debug/libnode_debug.dylib
			cd ../..

    else
        # yaml-cpp
        if [ ! -f lib/libyaml-cppmd.lib ]; then
            echo 'Building yaml-cpp ...'
            wget https://yaml-cpp.googlecode.com/files/yaml-cpp-0.5.1.tar.gz --no-check-certificate
            tar zxf yaml-cpp-0.5.1.tar.gz
            cd yaml-cpp-0.5.1

            # apply a patch to fix an issue of CMakeLists.txt
            wget http://silksmiths.sakura.ne.jp/files/yaml-cpp/CMakeLists.txt.patch
            patch < CMakeLists.txt.patch

            # apply a patch to fix a compile error https://code.google.com/p/yaml-cpp/issues/detail?id=276&colspec=ID%20Type%20Status%20Priority%20Milestone%20Component%20Owner%20Summary
            cd src
            wget http://silksmiths.sakura.ne.jp/files/yaml-cpp/ostream_wrapper.patch
            patch < ostream_wrapper.patch
            cd ..

            ensure_dir build
            cd build
            cmake -G "$MSVC" -DCMAKE_INSTALL_PREFIX=../.. ..
            msbuild.exe ALL_BUILD.vcxproj //p:Configuration=Release //p:VisualStudioVersion=$VisualStudioVersion
            msbuild.exe INSTALL.vcxproj //p:Configuration=Release //p:VisualStudioVersion=$VisualStudioVersion
            msbuild.exe ALL_BUILD.vcxproj //p:Configuration=Debug //p:VisualStudioVersion=$VisualStudioVersion
            msbuild.exe INSTALL.vcxproj //p:Configuration=Debug //p:VisualStudioVersion=$VisualStudioVersion
            cd ../..
            # Don't delete yaml-cpp-0.5.1 dir because libyaml-cppmdd.lib refers to a pdb in there
            rm yaml-cpp-*.tar.gz
        fi

  			# build node.dll
        cd vendor/node
        # Build Release version
        ./vcbuild.bat nosign shared ${ARCH}
        # Build Debug version
        ./vcbuild.bat nosign shared debug ${ARCH}
        cd ../..

        # uchardet
        if [ ! -f lib/uchardet/Release/uchardet.lib ]; then
            echo 'Building uchardet ...'
            cd uchardet
            cmake -G "$MSVC" -DCMAKE_INSTALL_PREFIX=..
            msbuild.exe ALL_BUILD.vcxproj //p:Configuration=Release //p:VisualStudioVersion=$VisualStudioVersion
            msbuild.exe ALL_BUILD.vcxproj //p:Configuration=Debug //p:VisualStudioVersion=$VisualStudioVersion
            mkdir -p ../lib/uchardet
            cp -r src/Release ../lib/uchardet
            cp -r src/Debug ../lib/uchardet
            mkdir -p ../include/uchardet
            cp src/uchardet.h ../include/uchardet
            cd ..
        fi

        # Onigmo
        if [ ! -f lib/onig.dll ]; then
            echo 'Building Onigmo...'
            cd Onigmo
            cp win32/Makefile .
            cp win32/config.h .
            echo $ONIG_BUILD_BAT > /tmp/build_onigmo.bat && /tmp/build_onigmo.bat
            cp onig.lib onig.dll ../lib
            cd ..
        fi

		#breakpad
		if [ ! -f lib/breakpad/Release/common.lib ]; then
			echo 'Building Breakpad client ...'

			cd breakpad
			patch -p1 --binary -N < ../crashreporter/patch/breakpad_googletest_include.h.patch
			patch -p1 --binary -N < ../crashreporter/patch/symbolic_constants_win.cc.patch
			patch -p1 --binary -N < ../crashreporter/patch/common.gypi.patch

			/c/tools/python2/Scripts/gyp --no-circular-check -G msvs_version=2015 -D win_release_RuntimeLibrary=2 -D win_debug_RuntimeLibrary=3 ./src/client/windows/breakpad_client.gyp

      # //p:TreatWarningAsError=false //p:DisableSpecificWarnings=4091 //p:RuntimeLibrary=MultiThreadedDebugDLL
            mkdir -p ../lib/breakpad/Debug
			MSBuild.exe //p:Configuration="Debug" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/common.vcxproj
			cp ./src/client/windows/Debug/lib/common.lib ./src/client/windows/Debug
			MSBuild.exe //p:Configuration="Debug" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/handler/exception_handler.vcxproj
			cp ./src/client/windows/handler/Debug/lib/exception_handler.lib ./src/client/windows/Debug
			MSBuild.exe //p:Configuration="Debug" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/crash_generation/crash_generation_client.vcxproj
			cp ./src/client/windows/crash_generation/Debug/lib/crash_generation_client.lib ./src/client/windows/Debug
			MSBuild.exe //p:Configuration="Debug" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/crash_generation/crash_generation_server.vcxproj
			cp ./src/client/windows/crash_generation/Debug/lib/crash_generation_server.lib ./src/client/windows/Debug
			MSBuild.exe //p:Configuration="Debug" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/sender/crash_report_sender.vcxproj
			cp ./src/client/windows/sender/Debug/lib/crash_report_sender.lib ./src/client/windows/Debug

            cp ./src/client/windows/Debug/common.lib ./src/client/windows/Debug/exception_handler.lib ./src/client/windows/Debug/crash_generation_client.lib ./src/client/windows/Debug/crash_generation_server.lib ./src/client/windows/Debug/crash_report_sender.lib ../lib/breakpad/Debug/

            mkdir -p ../lib/breakpad/Release
			MSBuild.exe //p:Configuration="Release" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/common.vcxproj
			cp ./src/client/windows/Release/lib/common.lib ./src/client/windows/Release
			MSBuild.exe //p:Configuration="Release" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/handler/exception_handler.vcxproj
			cp ./src/client/windows/handler/Release/lib/exception_handler.lib ./src/client/windows/Release
			MSBuild.exe //p:Configuration="Release" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/crash_generation/crash_generation_client.vcxproj
			cp ./src/client/windows/crash_generation/Release/lib/crash_generation_client.lib ./src/client/windows/Release
			MSBuild.exe //p:Configuration="Release" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/crash_generation/crash_generation_server.vcxproj
			cp ./src/client/windows/crash_generation/Release/lib/crash_generation_server.lib ./src/client/windows/Release
			MSBuild.exe //p:Configuration="Release" //p:VisualStudioVersion=$VisualStudioVersion //p:Platform=$ARCH ./src/client/windows/sender/crash_report_sender.vcxproj
			cp ./src/client/windows/sender/Release/lib/crash_report_sender.lib ./src/client/windows/Release

            cp ./src/client/windows/Release/common.lib ./src/client/windows/Release/exception_handler.lib ./src/client/windows/Release/crash_generation_client.lib ./src/client/windows/Release/crash_generation_server.lib ./src/client/windows/Release/crash_report_sender.lib ../lib/breakpad/Release/

			cd ..
		fi
    fi

    # npm install for silkedit package
    cd jslib/node_modules/silkedit
    npm install --production
    cd ../../..

    # npm install for default packages
    cd packages/node_modules
    npm install --production
    cd ../..
}

# release build
build() {
    if [ ! -d build ]; then
        echo "build directory doesn't exist. Creating it..."
        ensure_dir build
    fi

    cd build


    if [ $OS == 'Mac' ]; then
        if [ "$1" = '-edge' ]; then
            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_EDGE=ON -G Ninja .. -DCMAKE_BUILD_TYPE=Release
        else
            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja .. -DCMAKE_BUILD_TYPE=Release
        fi

        ninja

        # crashreporter
        # todo: do these in cmake
        cp -r crashreporter/CrashReporter.app/Contents/MacOS/CrashReporter SilkEdit.app/Contents/MacOS
        cp crashreporter/CrashReporter.app/Contents/Resources/translations/crashreporter_*.qm SilkEdit.app/Contents/Resources/translations
    else
        if [ "$1" = '-edge' ]; then
          ../build.bat $VCVARSALL_ARG all edge
        else
          ../build.bat $VCVARSALL_ARG all
        fi

        # crashreporter
        cp crashreporter/crashreporter.exe .
        cp crashreporter/crashreporter_*.qm .
    fi

    cd ..
}

make_installer() {
    if [ ! -d build ]; then
      build
    fi

    cd build

    if [ $OS == 'Mac' ]; then
        echo "Creating dmg..."
        macdeployqt SilkEdit.app
        ensure_dir dmg
        cp -R SilkEdit.app dmg
        ln -s /Applications dmg/Applications
				n=0
				MAX=3
				until [ $n -ge $MAX ]
				do
          rm -rf /Volumes/SilkEdit*
          touch dmg/.Trash
          hdiutil create -volname SilkEdit -srcfolder dmg -ov -format UDZO SilkEdit.dmg && break
          echo "Failed to create DMG. retrying"
					n=$[$n+1]
					sleep 3
				done
				if [ $n == $MAX ]; then
          echo "Failed to create DMG $MAX times"
					exit 1
				fi
    else
        mkdir -p Release
        # Can't use mv because mv fails when Release/packages already exists
        cp -r silkedit.exe onig.dll node.dll jslib packages themes silkedit_ja.qm Release
        #crashreporter
        cp -r crashreporter/crashreporter.exe crashreporter/crashreporter_*.qm Release
        cd Release

        windeployqt --release --no-compiler-runtime silkedit.exe

        # Copy URCT
        RELEASE_DIR=`pwd`
        cd "$UCRT_DIR"
        cp api-ms-win-*.dll ucrtbase.dll $RELEASE_DIR

        # Copy VC Runtime
        cd "$VCRT_DIR"
        cp msvcp140.dll vcruntime140.dll $RELEASE_DIR
        attrib -r $RELEASE_DIR/msvcp140.dll
        attrib -r $RELEASE_DIR/vcruntime140.dll
        cd $RELEASE_DIR

        if [ $OS == 'Win32' ]; then
            # Change Arch to x86 and remove ArchitecturesAllowed=x64 for x86 installer
            cat ../../silkedit.iss | sed -e 's/#define Arch "x64"/#define Arch "x86"/' | grep -v ArchitecturesAllowed=x64 > ../../silkedit_x86.iss
            iscc ../../silkedit_x86.iss
            rm ../../silkedit_x86.iss
        else
            iscc ../../silkedit.iss
        fi
    fi

    cd ..
}

rebuild() {
    ensure_dir build
    build $1
}

# static analysis
analyze() {
    if [ $OS == 'Win32' ]; then
        echo "Not implemented yet on Windows"
        exit 1
    fi

    build
    tmpFile=/tmp/clang-check-err
    find src core -name '*.cpp' |xargs clang-check -analyze -p build 2>&1 | tee $tmpFile
    if [ $(cat $tmpFile | grep warning | wc -l) != 0 ]; then
      rm -rf $tmpFile
      exit 1
    fi
    rm -rf $tmpFile
}

run_tests() {
    ensure_dir build
    cd build
    if [ $OS == 'Mac' ]; then
        cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
        if [ -f build.ninja ]; then
            ninja unit_test
            ctest --output-on-failure --no-compress-output -T Test || /usr/bin/true
        fi
    else
        ../build.bat $VCVARSALL_ARG unit_test
        ctest --output-on-failure --no-compress-output -T Test || /usr/bin/true
    fi
}

set_version() {
    if [ $# -ne 1 ]; then
        echo 'Specify a version number (e.g., 1.0.0)'
    else
        sed -i -E "s/<string>[0-9]+\.[0-9]+\.[0-9]+<\/string>/<string>$1<\/string>/" src/SilkEdit-info.plist
        commaVersion=$(echo $1 | sed -E 's/\./,/g')
        sed -i -E "s/VER_VERSION             [0-9]+,[0-9]+,[0-9]+/VER_VERSION             $commaVersion/" resources/silkedit.rc
        sed -i -E "s/VER_VERSION_STR         \"[0-9]+\.[0-9]+\.[0-9]+\\\\0\"/VER_VERSION_STR         \"$1\\\\0\"/" resources/silkedit.rc
        sed -i -E "s/VERSION \"[0-9]+\.[0-9]+\.[0-9]+\"/VERSION \"$1\"/" src/version.h
        sed -i -E "s/version\": \"[0-9]+\.[0-9]+\.[0-9]+\"/version\": \"$1\"/" jsdoc/package.json
        git add src/SilkEdit-info.plist resources/silkedit.rc src/version.h jsdoc/package.json
        git commit -m "Bump version to $1"
        git tag v$1
    fi
}

set_build() {
    if [ $# -ne 1 ]; then
        echo 'Specify a build number'
    else
        sed -i -E "s/CFBundleVersion = \"[0-9]+\";/CFBundleVersion = \"$1\";/" src/SilkEdit-info.plist
        sed -i -E "s/BUILD \"[0-9]+\"/BUILD \"$1\"/" src/version.h
    fi
}

localize() {
    lupdate src core -ts translations/silkedit_ja.ts
    #CrashReporter
    lupdate ./crashreporter/src -ts ./crashreporter/trans/crashreporter_ja.ts
}

send_symbol() {
    build

    APP_NAME="SilkEdit"
    echo "sending symbol with app name:${APP_NAME}, version: ${VERSION}"

    if [ $OS == 'Mac' ]; then
      DSYM_DIR="/tmp/SilkEdit.dSYM"
      rm -rf ${DSYM_DIR}
      dsymutil build/SilkEdit.app/Contents/MacOS/SilkEdit -o ${DSYM_DIR}
      bugsplat/Breakpad.framework/Resources/dump_sdk_symbols -d ${BUGSPLAT_DATABASE} -a ${APP_NAME} -v ${VERSION} -s ${DSYM_DIR}
    else
      ./breakpad/src/tools/windows/binaries/symupload.exe ./build/SilkEdit.exe "${BUGSPLAT_SYMBOL_URL}?appName=${APP_NAME}&appVer=${VERSION}"
    fi
}

generate_doc() {
  rm -rf ${API_DOC_DIR} && jsdoc -r jslib/node_modules/silkedit -c jsdoc/conf.json --readme jsdoc/README.md --tutorials jsdoc/tutorial --package jsdoc/package.json -d ${API_DOC_DIR}
  
  # change JSDoc hard coded title
  TITLE="SilkEdit APIリファレンス"
  sed -i -E "s/JSDoc: Home/${TITLE}/" ${API_DOC_DIR}/SilkEdit/${VERSION}/index.html
  
  # set favicon
  FAVICON="http://silksmiths.sakura.ne.jp/files/SilkEdit/favicon.ico"
  sed -i -E "s#</head>#<link rel=\"icon\" href=\"${FAVICON}\"></head>#" ${API_DOC_DIR}/SilkEdit/${VERSION}/index.html

  echo "API document is generated in ${API_DOC_DIR}"
}

upload_doc() {
  generate_doc
  
  # upload to FTP
  scp -r ${API_DOC_DIR}/SilkEdit/${VERSION} silksmiths@silksmiths.sakura.ne.jp:~/www/docs/api/
  
  # make current version as the latest API document
  cp -r ${API_DOC_DIR}/SilkEdit/${VERSION} ${API_DOC_DIR}/SilkEdit/latest
  scp -r ${API_DOC_DIR}/SilkEdit/latest silksmiths@silksmiths.sakura.ne.jp:~/www/docs/api/
}

# -e: stops if any command returns non zero value
set -e

case "$1" in
    analyze)
        analyze
        echo
        ;;
    build)
        build $2
        echo
        ;;
    clean)
        clean
        echo
        ;;
    format)
        # grep return 1 if it doesn't match anything, so disable -e here
        set +e
        format $2
        echo
        ;;
    generate_doc)
        generate_doc
        echo
        ;;
    rebuild)
        rebuild $2
        echo
        ;;
    resolve)
        resolve $2
        echo
        ;;
    test)
        run_tests
        echo
        ;;
    set_version)
        set_version $2
        echo
        ;;
    set_build)
        set_build $2
        echo
        ;;
    localize)
        localize
        echo
        ;;
    make_installer)
        make_installer
        echo
        ;;
    send_symbol)
        send_symbol
        echo
        ;;
    upload_doc)
        upload_doc
        echo
        ;;
    *)

        cat << EOF
analyze        static analysis
build          build an app and an installer (-edge: Edge build)
clean          clean build dir
format         format sources
generate_doc   generate SilkEdit package API document
localize       run lupdate to generate a ts file for localization
make_installer make an installer
rebuild        build from scratch (-edge: Edge build)
resolve        resolve dependency
test           run tests
set_version    set version number
set_build      set build number
send_symbol    send application symbol to server
upload_doc     upload package API document
EOF

        exit 1
esac

exit 0
