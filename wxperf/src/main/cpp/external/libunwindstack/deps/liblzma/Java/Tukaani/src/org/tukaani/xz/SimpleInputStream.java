/*
 * SimpleInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.IOException;
import org.tukaani.xz.simple.SimpleFilter;

class SimpleInputStream extends InputStream {
    private static final int FILTER_BUF_SIZE = 4096;

    private InputStream in;
    private final SimpleFilter simpleFilter;

    private final byte[] filterBuf = new byte[FILTER_BUF_SIZE];
    private int pos = 0;
    private int filtered = 0;
    private int unfiltered = 0;

    private boolean endReached = false;
    private IOException exception = null;

    private final byte[] tempBuf = new byte[1];

    static int getMemoryUsage() {
        return 1 + FILTER_BUF_SIZE / 1024;
    }

    SimpleInputStream(InputStream in, SimpleFilter simpleFilter) {
        // Check for null because otherwise null isn't detect
        // in this constructor.
        if (in == null)
            throw new NullPointerException();

        // The simpleFilter argument comes from this package
        // so it is known to be non-null already.
        assert simpleFilter != null;

        this.in = in;
        this.simpleFilter = simpleFilter;
    }

    public int read() throws IOException {
        return read(tempBuf, 0, 1) == -1 ? -1 : (tempBuf[0] & 0xFF);
    }

    public int read(byte[] buf, int off, int len) throws IOException {
        if (off < 0 || len < 0 || off + len < 0 || off + len > buf.length)
            throw new IndexOutOfBoundsException();

        if (len == 0)
            return 0;

        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        try {
            int size = 0;

            while (true) {
                // Copy filtered data into the caller-provided buffer.
                int copySize = Math.min(filtered, len);
                System.arraycopy(filterBuf, pos, buf, off, copySize);
                pos += copySize;
                filtered -= copySize;
                off += copySize;
                len -= copySize;
                size += copySize;

                // If end of filterBuf was reached, move the pending data to
                // the beginning of the buffer so that more data can be
                // copied into filterBuf on the next loop iteration.
                if (pos + filtered + unfiltered == FILTER_BUF_SIZE) {
                    System.arraycopy(filterBuf, pos, filterBuf, 0,
                                     filtered + unfiltered);
                    pos = 0;
                }

                if (len == 0 || endReached)
                    return size > 0 ? size : -1;

                assert filtered == 0;

                // Get more data into the temporary buffer.
                int inSize = FILTER_BUF_SIZE - (pos + filtered + unfiltered);
                inSize = in.read(filterBuf, pos + filtered + unfiltered,
                                 inSize);

                if (inSize == -1) {
                    // Mark the remaining unfiltered bytes to be ready
                    // to be copied out.
                    endReached = true;
                    filtered = unfiltered;
                    unfiltered = 0;
                } else {
                    // Filter the data in filterBuf.
                    unfiltered += inSize;
                    filtered = simpleFilter.code(filterBuf, pos, unfiltered);
                    assert filtered <= unfiltered;
                    unfiltered -= filtered;
                }
            }
        } catch (IOException e) {
            exception = e;
            throw e;
        }
    }

    public int available() throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        return filtered;
    }

    public void close() throws IOException {
        if (in != null) {
            try {
                in.close();
            } finally {
                in = null;
            }
        }
    }
}
