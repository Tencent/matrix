package com.tencent.matrix.batterycanary.utils;

import android.os.Process;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.StandardCharsets;

/**
 * see {@linkplain com.android.internal.os.ProcessCpuTracker}
 *
 * @author Kaede
 * @since 2020/11/6
 */
@SuppressWarnings({"JavadocReference", "SpellCheckingInspection"})
public final class ProcStatUtil {
    private static final String TAG = "Matrix.battery.ProcStatUtil";
    private static final ThreadLocal<byte[]> sBufferRef = new ThreadLocal<>();
    @Nullable
    private static OnParseError sParseError;

    static byte[] getLocalBuffers() {
        if (sBufferRef.get() == null) {
            sBufferRef.set(new byte[128]);
        }
        return sBufferRef.get();
    }

    ProcStatUtil() {
    }

    @Nullable
    public static ProcStat currentPid() {
        return of(Process.myPid());
    }

    @Nullable
    public static ProcStat current() {
        return of(Process.myPid(), Process.myTid());
    }

    @Nullable
    public static ProcStat of(int pid) {
        return parse("/proc/" + pid + "/stat");
    }

    @Nullable
    public static ProcStat of(int pid, int tid) {
        return parse("/proc/" + pid + "/task/" + tid + "/stat");
    }

    @Nullable
    public static ProcStat parse(String path) {
        try {
            ProcStat procStatInfo = null;
            try {
                procStatInfo = parseWithBufferForPath(path, getLocalBuffers());
            } catch (ParseException e) {
                if (sParseError != null) {
                    sParseError.onError(1, e.content);
                }
            }

            if (procStatInfo == null || procStatInfo.comm == null) {
                MatrixLog.w(TAG, "#parseJiffies read with buffer fail, fallback with spilts");
                try {
                    procStatInfo = parseWithSplits(BatteryCanaryUtil.cat(path));
                } catch (ParseException e) {
                    if (sParseError != null) {
                        sParseError.onError(2, e.content);
                    }
                }
                if (procStatInfo == null || procStatInfo.comm == null) {
                    MatrixLog.w(TAG, "#parseJiffies read with splits fail");
                    return null;
                }
            }
            return procStatInfo;
        } catch (Throwable e) {
            MatrixLog.w(TAG, "#parseJiffies fail: " + e.getMessage());
            if (sParseError != null) {
                sParseError.onError(0, BatteryCanaryUtil.cat(path) + "\n" + e.getMessage());
            }
            return null;
        }
    }

    public static ProcStat parseWithBufferForPath(String path, byte[] buffer) throws ParseException {
        File file = new File(path);
        if (!file.exists()) {
            return null;
        }

        int readBytes;
        try (FileInputStream fis = new FileInputStream(file)) {
            readBytes = fis.read(buffer);
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "read buffer from file fail");
            readBytes = -1;
        }
        if (readBytes <= 0) {
            return null;
        }

        return parseWithBuffer(buffer);
    }

    /**
     * Do NOT modfiy this method untlil all the test cases within {@link ProcStatUtilsTest} is passed.
     */
    @VisibleForTesting
    static ProcStat parseWithBuffer(byte[] statBuffer) throws ParseException {
        /*
         * 样本:
         * 10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 0 0 20 0 17 0 9087400 5414273024
         *  24109 18446744073709551615 421814448128 421814472944 549131058960 0 0 0 4612 1 1073775864
         *  1 0 0 17 7 0 0 0 0 0 421814476800 421814478232 422247952384 549131060923 549131061022 549131061022
         *  549131063262 0
         *
         * 字段:
         * - pid:  进程ID.
         * - comm: task_struct结构体的进程名
         * - state: 进程状态, 此处为S
         * - ppid: 父进程ID （父进程是指通过fork方式, 通过clone并非父进程）
         * - pgrp: 进程组ID
         * - session: 进程会话组ID
         * - tty_nr: 当前进程的tty终点设备号
         * - tpgid: 控制进程终端的前台进程号
         * - flags: 进程标识位, 定义在include/linux/sched.h中的PF_*, 此处等于1077952832
         * - minflt:  次要缺页中断的次数, 即无需从磁盘加载内存页. 比如COW和匿名页
         * - cminflt: 当前进程等待子进程的minflt
         * - majflt: 主要缺页中断的次数, 需要从磁盘加载内存页. 比如map文件
         * - majflt: 当前进程等待子进程的majflt
         * - utime: 该进程处于用户态的时间, 单位jiffies, 此处等于166114
         * - stime: 该进程处于内核态的时间, 单位jiffies, 此处等于129684
         * - cutime: 当前进程等待子进程的utime
         * - cstime: 当前进程等待子进程的utime
         * - priority: 进程优先级, 此次等于10.
         * - nice: nice值, 取值范围[19, -20], 此处等于-10
         * - num_threads: 线程个数, 此处等于221
         * - itrealvalue: 该字段已废弃, 恒等于0
         * - starttime: 自系统启动后的进程创建时间, 单位jiffies, 此处等于2284
         * - vsize: 进程的虚拟内存大小, 单位为bytes
         * - rss: 进程独占内存+共享库, 单位pages, 此处等于93087
         * - rsslim: rss大小上限
         *
         * 说明:
         * 第10~17行主要是随着时间而改变的量；
         * 内核时间单位, sysconf(_SC_CLK_TCK)一般地定义为jiffies(一般地等于10ms)
         * starttime: 此值单位为jiffies, 结合/proc/stat的btime, 可知道每一个线程启动的时间点
         * 1500827856 + 2284/100 = 1500827856, 转换成北京时间为2017/7/24 0:37:58
         * 第四行数据很少使用,只说一下该行第7至9个数的含义:
         * signal: 即将要处理的信号, 十进制, 此处等于6660
         * blocked: 阻塞的信号, 十进制
         * sigignore: 被忽略的信号, 十进制, 此处等于36088
         */

        ProcStat stat = new ProcStat();
        int statBytes = statBuffer.length;
        for (int i = 0, spaceIdx = 0; i < statBytes;) {
            if (Character.isSpaceChar(statBuffer[i])) {
                spaceIdx++;
                i++;
                continue;
            }

            switch (spaceIdx) {
                case 1: { // read comm (thread name)
                    int readIdx = i, window = 0;
                    // seek end symobl of comm: ')'
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && ')' != statBuffer[i]) {
                        i++;
                        window++;
                    }
                    if ('(' == statBuffer[readIdx]) {
                        readIdx++;
                        window--;
                    }
                    if (')' == statBuffer[readIdx + window - 1]) {
                        window--;
                    }
                    if (window > 0) {
                        stat.comm = safeBytesToString(statBuffer, readIdx, window);
                    }
                    spaceIdx = 2;
                    break;
                }

                case 3: { // thread state
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && !Character.isSpaceChar(statBuffer[i])) {
                        i++;
                        window++;
                    }
                    stat.stat = safeBytesToString(statBuffer, readIdx, window);
                    break;
                }

                case 14: { // utime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && !Character.isSpaceChar(statBuffer[i])) {
                        i++;
                        window++;
                    }
                    String num = safeBytesToString(statBuffer, readIdx, window);
                    if (!isNumeric(num)) {
                        throw new ParseException(safeBytesToString(statBuffer, 0, statBuffer.length) + "\nutime: " + num);
                    }
                    stat.utime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 15: { // stime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && !Character.isSpaceChar(statBuffer[i])) {
                        i++;
                        window++;
                    }
                    String num = safeBytesToString(statBuffer, readIdx, window);
                    if (!isNumeric(num)) {
                        throw new ParseException(safeBytesToString(statBuffer, 0, statBuffer.length) + "\nstime: " + num);
                    }
                    stat.stime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 16: { // cutime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && !Character.isSpaceChar(statBuffer[i])) {
                        i++;
                        window++;
                    }
                    String num = safeBytesToString(statBuffer, readIdx, window);
                    if (!isNumeric(num)) {
                        throw new ParseException(safeBytesToString(statBuffer, 0, statBuffer.length) + "\ncutime: " + num);
                    }
                    stat.cutime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 17: { // cstime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    while (i < statBytes && !Character.isSpaceChar(statBuffer[i])) {
                        i++;
                        window++;
                    }
                    String num = safeBytesToString(statBuffer, readIdx, window);
                    if (!isNumeric(num)) {
                        throw new ParseException(safeBytesToString(statBuffer, 0, statBuffer.length) + "\ncstime: " + num);
                    }
                    stat.cstime = MatrixUtil.parseLong(num, 0);
                    break;
                }

                default:
                    i++;
            }
        }
        return stat;
    }

    @VisibleForTesting
    static ProcStat parseWithSplits(String cat) throws ParseException {
        ProcStat stat = new ProcStat();
        if (!TextUtils.isEmpty(cat)) {
            int index = cat.indexOf(")");
            if (index <= 0) throw new IllegalStateException(cat + " has not ')'");
            String prefix = cat.substring(0, index);
            int indexBgn = prefix.indexOf("(") + "(".length();
            stat.comm = prefix.substring(indexBgn, index);

            String suffix = cat.substring(index + ")".length());
            String[] splits = suffix.split(" ");

            if (!isNumeric(splits[12])) {
                throw new ParseException(cat + "\nutime: " + splits[12]);
            }
            if (!isNumeric(splits[13])) {
                throw new ParseException(cat + "\nstime: " + splits[13]);
            }
            if (!isNumeric(splits[14])) {
                throw new ParseException(cat + "\ncutime: " + splits[14]);
            }
            if (!isNumeric(splits[15])) {
                throw new ParseException(cat + "\ncstime: " + splits[15]);
            }
            stat.stat = splits[1];
            stat.utime = MatrixUtil.parseLong(splits[12], 0);
            stat.stime = MatrixUtil.parseLong(splits[13], 0);
            stat.cutime = MatrixUtil.parseLong(splits[14], 0);
            stat.cstime = MatrixUtil.parseLong(splits[15], 0);
        }
        return stat;
    }

    @VisibleForTesting
    static String safeBytesToString(byte[] buffer, int offset, int length) {
        try {
            CharBuffer charBuffer = StandardCharsets.UTF_8.decode(ByteBuffer.wrap(buffer, offset, length));
            return String.valueOf(charBuffer.array(), 0, charBuffer.limit());
        } catch (IndexOutOfBoundsException e) {
            MatrixLog.w(TAG, "#safeBytesToString failed: " + e.getMessage());
            return "";
        }
    }

    static boolean isNumeric(String text) {
        if (TextUtils.isEmpty(text)) return false;
        if (text.startsWith("-")) {
            // negative number
            return TextUtils.isDigitsOnly(text.substring(1));
        }
        return TextUtils.isDigitsOnly(text);
    }

    public static void setParseErrorListener(OnParseError parseError) {
        sParseError = parseError;
    }

    @SuppressWarnings("SpellCheckingInspection")
    public static class ProcStat {
        public String comm = "";
        public String stat = "_";
        public long utime = -1;
        public long stime = -1;
        public long cutime = -1;
        public long cstime = -1;

        public long getJiffies() {
            return utime + stime + cutime + cstime;
        }
    }

    public interface OnParseError {
        void onError(int mode, String input);
    }

    public static class ParseException extends Exception {
        public final String content;

        public ParseException(String content) {
            this.content = content;
        }
    }
}
