
cd ./matrix/matrix-android

# ./gradlew clean compileDebugJavaWithJavac -PforCoverity --no-daemon
# ./gradlew :matrix-io-canary:build
# ./gradlew :matrix-sqlite-lint:build

cd ../matrix-iOS
TARGET_NAME=Matrix

# 模拟器
xcodebuild clean -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk iphonesimulator VALID_ARCHS="arm64 armv7 i386 x86_64" 
xcodebuild -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk iphonesimulator VALID_ARCHS="arm64 armv7 i386 x86_64" 

# iphone真机
xcodebuild clean -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk iphoneos VALID_ARCHS="arm64 armv7 i386 x86_64" 
xcodebuild -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk iphoneos VALID_ARCHS="arm64 armv7 i386 x86_64" 

# macOS
xcodebuild clean -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk macosx VALID_ARCHS="x86_64" 

xcodebuild -project Matrix/Matrix.xcodeproj -target $TARGET_NAME -configuration Release -sdk macosx VALID_ARCHS="x86_64" 



