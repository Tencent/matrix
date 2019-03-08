
cd ./matrix/matrix-android

./gradlew clean compileDebugJavaWithJavac -PforCoverity --no-daemon
./gradlew :matrix-io-canary:build
./gradlew :matrix-sqlite-lint:build

