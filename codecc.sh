
cd ./matrix/matrix-android

./gradlew clean compileDebugJavaWithJavac -PforCoverity --no-daemon
./gradlew :matrix-io-canary:build
./gradlew :matrix-sqlite-lint:build

cd ../matrix-iOS
xcodebuild clean -project Matrix/Matrix.xcodeproj  -scheme MatrixiOS -configuration Debug
xcodebuild build -project Matrix/Matrix.xcodeproj  -scheme MatrixiOS -configuration Debug
xcodebuild clean -project Matrix/Matrix.xcodeproj  -scheme MatrixMac -configuration Debug
xcodebuild build -project Matrix/Matrix.xcodeproj  -scheme MatrixMac -configuration Debug

