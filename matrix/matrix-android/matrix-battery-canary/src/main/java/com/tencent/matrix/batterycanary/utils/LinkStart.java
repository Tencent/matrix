package com.tencent.matrix.batterycanary.utils;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.SystemClock;
import androidx.annotation.NonNull;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * 林库死大多
 *
 * @author Kaede
 * @since date 2016/10/31
 */
@SuppressWarnings("unused")
final public class LinkStart {

    final Ticker mTicker;
    final Session mRoot;

    Session mCurrent;
    boolean mIsFinished = true;


    public LinkStart() {
        this(new SystemClockMillisTicker());
    }

    public LinkStart(Ticker ticker) {
        mTicker = ticker;
        mRoot = new Session("WatchCat");
        mCurrent = mRoot;
    }

    public Session getCurrent() {
        return mCurrent;
    }

    public void setCurrent(Session current) {
        mCurrent = current;
    }

    public LinkStart start() {
        mIsFinished = false;
        mRoot.startTime = mTicker.currentTime();
        return this;
    }

    public Session insert(String session) {
        if (!mIsFinished) {
            Session entry = mCurrent.insert(session);
            mCurrent = entry;
            return entry;
        }

        throw new IllegalStateException("this link already is finished.");
    }

    public Session enter(String session) {
        if (!mIsFinished) {
            if (mCurrent == mRoot) {
                throw new IllegalStateException("root session can not have peer.");
            }

            Session entry = mCurrent.enter(session);
            mCurrent = entry;
            return entry;
        }

        throw new IllegalStateException("this link already is finished.");
    }

    public void end(Session session) {
        session.end();
        mCurrent = session;
    }

    public void finish() {
        mRoot.end();
        mIsFinished = true;
    }

    @NonNull
    @Override
    public String toString() {
        if (!mIsFinished) {
            return "watch cat is not finished yet.";
        }

        StringBuilder sb = new StringBuilder("watch cat, time unit = " + mTicker.getUnit() + "\n");
        buildString(mRoot, sb);
        return sb.toString();
    }

    private void buildString(Session session, StringBuilder sb) {
        sb.append(session.toString()).append("\n");

        if (session.children != null) {
            for (int i = 0; i < session.children.size(); i++) {
                buildString(session.children.get(i), sb);
            }
        }
    }


    @SuppressWarnings("UnusedReturnValue")
    public class Session {
        private static final String PREFIX = "----";
        private static final int MAX_TITLE_LENGTH = 30;

        String name;
        long startTime;
        long endTime;

        int path;
        Session parent;
        List<Session> children;

        Session(String name) {
            this.name = name;
            path = 0; // root
        }

        Session attach(Session parent) {
            this.parent = parent;
            return this;
        }

        Session insert(String childName) {
            Session child = new Session(childName).attach(this);
            child.startTime = mTicker.currentTime();
            child.path = this.path + 1;
            addChild(child);
            return child;
        }

        void addChild(Session child) {
            if (children == null) {
                children = new LinkedList<>();
            }
            children.add(child);
        }

        Session enter(String peerName) {
            if (parent == null) {
                throw new IllegalStateException("this method need a nonnull parent.");
            }

            Session next = new Session(peerName).attach(parent);
            next.startTime = mTicker.currentTime();
            next.path = this.path;
            parent.addChild(next);
            return next;
        }

        Session end() {
            this.endTime = mTicker.currentTime();
            return this;
        }

        @NonNull
        @Override
        public String toString() {
            String prefix = "";
            if (path > 0) {
                prefix = repeat(PREFIX, path);
            }

            String postfix;
            if (startTime == 0) {
                postfix = "losing record of bgn time";
            } else if (endTime == 0) {
                postfix = "losing record of end time";
            } else {
                postfix = String.valueOf(mTicker.getInterval(startTime, endTime));
            }

            String title = name;
            if (name.length() > MAX_TITLE_LENGTH) {
                title = name.substring(0, MAX_TITLE_LENGTH - 1);
            } else if (name.length() < MAX_TITLE_LENGTH) {
                title = name + repeat(" ", MAX_TITLE_LENGTH - name.length());
            }

            return "|" + prefix + " " + title + " : " + postfix;
        }
    }

    public interface Ticker {
        long currentTime();

        long getInterval(long start, long end);

        TimeUnit getUnit();
    }

    /**
     * Milli Second
     */
    public static class SystemClockMillisTicker implements Ticker {
        @Override
        public long currentTime() {
            return SystemClock.elapsedRealtime();
        }

        @Override
        public long getInterval(long start, long end) {
            return end - start;
        }

        @Override
        public TimeUnit getUnit() {
            return TimeUnit.MILLISECONDS;
        }
    }

    public static class SystemMillisTicker implements Ticker {
        @Override
        public long currentTime() {
            return System.currentTimeMillis();
        }

        @Override
        public long getInterval(long start, long end) {
            return end - start;
        }

        @Override
        public TimeUnit getUnit() {
            return TimeUnit.MILLISECONDS;
        }
    }

    /**
     * Nano Second
     */
    public static class NanoTicker implements Ticker {
        @SuppressLint("ObsoleteSdkInt")
        @Override
        public long currentTime() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
                return SystemClock.elapsedRealtimeNanos();
            } else {
                return System.nanoTime();
            }
        }

        @Override
        public long getInterval(long start, long end) {
            return end - start;
        }

        @Override
        public TimeUnit getUnit() {
            return TimeUnit.NANOSECONDS;
        }
    }

    private static final int PAD_LIMIT = 8192;

    static String repeat(final char ch, final int repeat) {
        if (repeat <= 0) {
            return "";
        }
        final char[] buf = new char[repeat];
        Arrays.fill(buf, ch);
        return new String(buf);
    }

    public static String repeat(final String str, final int repeat) {
        if (str == null) {
            return null;
        }
        if (repeat <= 0) {
            return "";
        }
        final int inputLength = str.length();
        if (repeat == 1 || inputLength == 0) {
            return str;
        }
        if (inputLength == 1 && repeat <= PAD_LIMIT) {
            return repeat(str.charAt(0), repeat);
        }

        final int outputLength = inputLength * repeat;
        switch (inputLength) {
            case 1 :
                return repeat(str.charAt(0), repeat);
            case 2 :
                final char ch0 = str.charAt(0);
                final char ch1 = str.charAt(1);
                final char[] output2 = new char[outputLength];
                for (int i = repeat * 2 - 2; i >= 0; i--, i--) {
                    output2[i] = ch0;
                    output2[i + 1] = ch1;
                }
                return new String(output2);
            default :
                final StringBuilder buf = new StringBuilder(outputLength);
                for (int i = 0; i < repeat; i++) {
                    buf.append(str);
                }
                return buf.toString();
        }
    }
}
