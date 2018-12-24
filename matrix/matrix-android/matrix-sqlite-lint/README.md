## SQLiteLint
SQLiteLint is a SQLite best practices aids, it can detect potential, suspicious SQLite usage issues during development or testing.
## Getting started
Add dependencies by adding the following lines to your app/build.gradle.
```
dependencies {
    debugCompile "com.tencent.matrix:matrix-sqlite-lint-android-sdk:${MATRIX_VERSION}"
    releaseCompile "com.tencent.matrix:matrix-sqlite-lint-android-sdk-no-op:${MATRIX_VERSION}"
}
```
In your Application's onCreate, call the following function.
```
private void prepareSQLiteLint() {
    SQLiteLintPlugin plugin = (SQLiteLintPlugin) Matrix.with().getPluginByClass(SQLiteLintPlugin.class);
    if (plugin == null) {
        return;
    }
    plugin.addConcernedDB(new SQLiteLintConfig.ConcernDb(TestDBHelper.get().getWritableDatabase())
                    .setWhiteListXml(R.xml.sqlite_lint_whitelist)
                    .enableAllCheckers());
}
```
That's all. SQLiteLint will work by hooking 'sqlite3_profile' interface to collect sql's execution(Compatible with the latest Android 8.0),
 or you can just call SQLiteLint.notifySqlExecution() to tell SQLiteLint that a sql is executed.