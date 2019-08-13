/*
 * SimpleOutputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.IOException;
import org.tukaani.xz.simple.SimpleFilter;

class SimpleOutputStream extends FinishableOutputStream {
    private static final int FILTER_BUF_SIZE = 4096;

    private FinishableOutputStream out;
    private final SimpleFilter simpleFilter;

    private final byte[] filterBuf = new byte[FILTER_BUF_SIZE];
    private int pos = 0;
    private int unfiltered = 0;

    private IOException exception = null;
    private boolean finished = false;

    private final byte[] tempBuf = new byte[1];

    static int getMemoryUsage() {
        return 1 + FILTER_BUF_SIZE / 1024;
    }

    SimpleOutputStream(FinishableOutputStream out,
                       SimpleFilter simpleFilter) {
        if (out == null)
            throw new NullPointerException();

        this.out = out;
        this.simpleFilter = simpleFilter;
    }

    public void write(int b) throws IOException {
        tempBuf[0] = (byte)b;
        write(tempBuf, 0, 1);
    }

    public void write(byte[] buf, int off, int len) throws IOException {
        if (off < 0 || len < 0 || off + len < 0 || off + len > buf.length)
            throw new IndexOutOfBoundsException();

        if (exception != null)
            throw exception;

        if (finished)
            throw new XZIOException("Stream finished or closed");

        while (len > 0) {
            // Copy more unfiltered data into filterBuf.
            int copySize = Math.min(len, FILTER_BUF_SIZE - (pos + unfiltered));
            System.arraycopy(buf, off, filterBuf, pos + unfiltered, copySize);
            off += copySize;
            len -= copySize;
            unfiltered += copySize;

            // Filter the data in filterBuf.
            int filtered = simpleFilter.code(filterBuf, pos, unfiltered);
            assert filtered <= unfiltered;
            unfiltered -= filtered;

            // Write out the filtered data.
            try {
                out.write(filterBuf, pos, filtered);
            } catch (IOException e) {
                exception = e;
                throw e;
            }

            pos += filtered;

            // If end of filterBuf was reached, move the pending unfiltered
            // data to the beginning of the buffer so that more data can
            // be copied into filterBuf on the next loop iteration.
            if (pos + unfiltered == FILTER_BUF_SIZE) {
                System.arraycopy(filterBuf, pos, filterBuf, 0, unfiltered);
                pos = 0;
            }
        }
    }

    private void writePending() throws IOException {
        assert !finished;

        if (exception != null)
            throw exception;

        try {
            out.write(filterBuf, pos, unfiltered);
        } catch (IOException e) {
            exception = e;
            throw e;
        }

        finished = true;
    }

    public void flush() throws IOException {
        throw new UnsupportedOptionsException("Flushing is not supported");
    }

    public void finish() throws IOException {
        if (!finished) {
            // If it fails, don't call out.finish().
            writePending();

            try {
                out.finish();
            } catch (IOException e) {
                exception = e;
                throw e;
            }
        }
    }

    public void close() throws IOException {
        if (out != null) {
            if (!finished) {
                // out.close() must be called even if writePending() fails.
                // writePending() saves the possible exception so we can
                // ignore exceptions here.
                try {
                    writePending();
                } catch (IOException e) {}
            }

            try {
                out.close();
            } catch (IOException e) {
                // If there is an earlier exception, the exception
                // from out.close() is lost.
                if (exception == null)
                    exception = e;
            }

            out = null;
        }

        if (exception != null)
            throw exception;
    }
}
