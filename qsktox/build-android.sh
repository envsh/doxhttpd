#!/bin/bash
set -e
export JAVA_HOME=/opt/jdk-17.0.13+11
export ANDROID_SDK_ROOT=/opt/android-sdk
export ANDROID_NDK_ROOT=/opt/android-ndk-r26b

APP_VERSION_CODE=$(git rev-list --count HEAD)
if [ -z "$APP_VERSION_CODE" ]; then APP_VERSION_CODE=1; fi
cat > src/version_config.h <<H
#ifndef VERSION_CONFIG_H
#define VERSION_CONFIG_H
#define APP_VERSION_NAME "1.0"
#define APP_VERSION_CODE ${APP_VERSION_CODE}
#endif
H
echo "[build-android.sh] versionConfig: 1.0 (${APP_VERSION_CODE})"

QT_ANDROID=/opt/qt/6.7.3/android_arm64_v8a
QSK_ANDROID=/opt/qt/qskinny-arm64
QT_HOST=/opt/qt/6.7.3/gcc_64

BUILD_DIR=build-android
# rm -rf "$BUILD_DIR"  # 保留 cmake 缓存以支持增量编译

# 1. cmake configure via qt-cmake
"$QT_ANDROID/bin/qt-cmake" -S . -B "$BUILD_DIR" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_PREFIX_PATH="$QSK_ANDROID" \
    -DQSkinny_DIR="$QSK_ANDROID/lib/cmake/QSkinny" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_STRIP=/bin/true

# 2. build native .so only (not apk target)
#cmake --build "$BUILD_DIR" -j$(nproc) --target qsktox
cmake --build "$BUILD_DIR" -j1 --target qsktox

# 3. prepare android-build dir (copy .so + Qt deps, no Gradle)
APK_DIR="$BUILD_DIR/android-build"
# rm -rf "$APK_DIR"
LIB_DIR="$APK_DIR/libs/arm64-v8a"
mkdir -p "$LIB_DIR"
cp "$BUILD_DIR/libqsktox_arm64-v8a.so" "$LIB_DIR/"

# 平铺复制 qskinny 皮肤插件到 JNI lib 根目录（Android 无子目录）
cp "$QSK_ANDROID/lib/qskinny/plugins/skins/"*.so "$LIB_DIR/"
cp "$QSK_ANDROID/lib/qskinny/plugins/platforminputcontexts/"*.so "$LIB_DIR/"

# 复制 extras 共享库到 APK
cp "$BUILD_DIR/libgoso.so" "$LIB_DIR/"
cp "$BUILD_DIR/libcso.so" "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libsqlite3.so "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libcurl.so "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libssl.so "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libcrypto.so "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libssl_3.so "$LIB_DIR/"
cp ../vendor/lib/${ANDROID_ABI:-arm64-v8a}/libcrypto_3.so "$LIB_DIR/"

"$QT_HOST/bin/androiddeployqt" \
    --input "$BUILD_DIR/android-qsktox-deployment-settings.json" \
    --output "$APK_DIR" \
    --deployment bundled \
     --builddir "$BUILD_DIR" \
     --aux-mode

# 兜底：libc++_shared.so 只靠 androiddeployqt 的"缺了才补"复制，
# 构建被中止后残留缺失增量状态时会打出残缺 APK → 显式强制覆盖
# （NDK 官方：libc++ 非系统库，"libc++_shared.so must be included in the APK"）
STDCPP="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
cp -f "$STDCPP" "$LIB_DIR/"

# 禁止 Gradle strip：保留 .debug_info，便于 addr2line/ndk-stack 符号化崩溃
# （androiddeployqt --aux-mode 不覆写已存在 build.gradle，每次注入并防重；
#  模板含 packagingOptions.jniLibs 块，插入独立 packagingOptions 块前置）
if ! grep -q 'doNotStrip' "$APK_DIR/build.gradle"; then
    sed -i '0,/^android {/s//android {\n    packagingOptions {\n        doNotStrip "**\/*.so"\n    }/' "$APK_DIR/build.gradle"
fi

# 复制自定义 Java 源码到 Android 构建目录
mkdir -p "$APK_DIR/src/main/java"
cp -R android/src/java/* "$APK_DIR/src/main/java/"
# 复制应用图标到 Android res
mkdir -p "$APK_DIR/res/drawable"
cp app_icon.png "$APK_DIR/res/drawable/ic_launcher.png"

# 移除 renderscript.srcDirs 配置（build-tools 34+ 不再支持，且会干扰自定义 Java 源码）
sed -i '/renderscript\.srcDirs/d' "$APK_DIR/build.gradle"

# 注入版本号（defaultConfig 覆盖 Manifest 默认值，版本随 git 提交单调递增）
sed -i "/defaultConfig {/a\\        versionCode ${APP_VERSION_CODE}\n        versionName \"1.0\"" "$APK_DIR/build.gradle"

# ── 注入 UnifiedPush connector（Maven 依赖，见 vendor/vendorinfos.md）──
# build.gradle 是 androiddeployqt 生成的产物，aux-mode 不覆写已存在的文件，
# 本地残留手工修改不会自动出现在全新生成的 CI 工程中，故每次注入并防重
if ! grep -q "org.unifiedpush.android:connector" "$APK_DIR/build.gradle"; then
    cat > /tmp/up_conf.txt <<'EOF'

configurations.configureEach {
    def tink = "com.google.crypto.tink:tink-android:1.20.0"
    resolutionStrategy {
        force(tink)
        dependencySubstitution {
           substitute module('com.google.crypto.tink:tink') using module(tink)
        }
        force 'com.google.errorprone:error_prone_annotations:2.20.0'
        // 锁定与本地一致的 androidx 版本链（CI 直连 Google Maven 会拉到
        // core 1.13.1 / experimental 1.4.0，强制要求 compileSdk ≥34）
        force 'androidx.core:core:1.10.1'
        force 'androidx.annotation:annotation-experimental:1.3.0'
    }
}
EOF
    sed -i "/^apply plugin: 'com.android.application'/r /tmp/up_conf.txt" "$APK_DIR/build.gradle"
    echo "    implementation('org.unifiedpush.android:connector:3.3.3')" > /tmp/up_dep.txt
    sed -i '/^dependencies {/r /tmp/up_dep.txt' "$APK_DIR/build.gradle"
fi

# Qt 模板生成 useLegacyPackaging true（.so DEFLATE 压缩，安装时解压到磁盘）。
# 注意：Qt 6.9 之前不支持直接从 APK 加载未压缩(STORED)的 .so —— 若改成 false，
# QtLoader 的 System.load() 无法 dlopen 平台插件，JNI_OnLoad 不执行，
# 首个 native 调用(QtDisplayManager.handleOrientationChanged)即 UnsatisfiedLinkError 闪退。
# 必须保持 true；压缩所需堆内存由下方 jvmargs/GRADLE_OPTS 提升解决。
# 注意：androiddeployqt --aux-mode 不覆写已存在的 build.gradle，故旧产物里的
# false 会残留，这里显式改回 true（模板默认即 true，全新生成时为 no-op）。
sed -i 's/packagingOptions.jniLibs.useLegacyPackaging false/packagingOptions.jniLibs.useLegacyPackaging true/' "$APK_DIR/build.gradle"

# 覆盖 manifest（包含所有自定义：activity、intent-filter、权限、service）
cp android/AndroidManifest.xml "$APK_DIR/AndroidManifest.xml"

# 4. append Qt-specific properties (missing from --aux-mode)
cat >> "$APK_DIR/gradle.properties" <<PROPS
# 模板默认 -Xmx386m 过小，packageDebug 打包 Qt+vendor 大 .so 时 OOM；
# java.util.Properties 后值覆盖前值，此行覆盖模板头部的旧 jvmargs
org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=1024m -Dfile.encoding=UTF-8
androidBuildToolsVersion=34.0.0
androidCompileSdkVersion=android-33
androidNdkVersion=26.1.10909125
androidPackageName=io.fedlet.qsktox
buildDir=build
qtAndroidDir=${QT_ANDROID}/src/android/java
qtMinSdkVersion=23
qtTargetAbiList=arm64-v8a
qtTargetSdkVersion=34
PROPS

# 5. patch Gradle heap (low-memory device); DEFLATE 压缩 111MB lib 需要足够堆
sed -i 's/-Xmx[0-9]*m/-Xmx2048m/' "$APK_DIR/gradle.properties"

# 6. build debug APK (auto-signed with Android debug key)
GRADLE_OPTS="-Xmx2048m" "$APK_DIR/gradlew" --no-daemon --max-workers=2 -p "$APK_DIR" assembleDebug

# 6. verify output
APK=$(find "$APK_DIR/build/outputs" -name "*.apk" 2>/dev/null | head -1)
if [ -n "$APK" ]; then
    echo "=== APK built: $APK ==="
    ls -lh "$APK"
fi

# 打包完整性校验：libc++_shared.so 必须在 APK 内（缺了装到设备才会炸）
if [ -n "$APK" ] && ! unzip -l "$APK" | grep -q "lib/arm64-v8a/libc++_shared.so"; then
    echo "=== ERROR: libc++_shared.so missing from APK (stale gradle output?) ===" >&2
    exit 1
fi

find build-android -name *qsktox*.so | xargs ls -lh
objdump -x build-android/android-build/libs/arm64-v8a/libqsktox_arm64-v8a.so|grep NEEDED|grep -v libQt|grep -v libc|grep -v libm|grep -v libc 

### todo kill java of this process forked
