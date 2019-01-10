
cd /home/android/hudson/workspace/open-check/build/repositories/matrix/matrix/matrix-android

./gradlew clean compileDebugJavaWithJavac -PforCoverity --no-daemon
./gradlew :matrix-io-canary:build
./gradlew :matrix-sqlite-lint:build

