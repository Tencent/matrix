/*
 * DeltaOutputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.IOException;
import org.tukaani.xz.delta.DeltaEncoder;

class DeltaOutputStream extends FinishableOutputStream {
    private static final int FILTER_BUF_SIZE = 4096;

    private FinishableOutputStream out;
    private final DeltaEncoder delta;
    private final byte[] filterBuf = new byte[FILTER_BUF_SIZE];

    private boolean finished = false;
    private IOException exception = null;

    private final byte[] tempBuf = new byte[1];

    static int getMemoryUsage() {
        return 1 + FILTER_BUF_SIZE / 1024;
    }

    DeltaOutputStream(FinishableOutputStream out, DeltaOptions options) {
        this.out = out;
        delta = new DeltaEncoder(options.getDistance());
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
            throw new XZIOException("Stream finished");

        try {
            while (len > FILTER_BUF_SIZE) {
                delta.encode(buf, off, FILTER_BUF_SIZE, filterBuf);
                out.write(filterBuf);
                off += FILTER_BUF_SIZE;
                len -= FILTER_BUF_SIZE;
            }

            delta.encode(buf, off, len, filterBuf);
            out.write(filterBuf, 0, len);
        } catch (IOException e) {
            exception = e;
            throw e;
        }
    }

    public void flush() throws IOException {
        if (exception != null)
            throw exception;

        if (finished)
            throw new XZIOException("Stream finished or closed");

        try {
            out.flush();
        } catch (IOException e) {
            exception = e;
            throw e;
        }
    }

    public void finish() throws IOException {
        if (!finished) {
            if (exception != null)
                throw exception;

            try {
                out.finish();
            } catch (IOException e) {
                exception = e;
                throw e;
            }

            finished = true;
        }
    }

    public void close() throws IOException {
        if (out != null) {
            try {
                out.close();
            } catch (IOException e) {
                if (exception == null)
                    exception = e;
            }

            out = null;
        }

        if (exception != null)
            throw exception;
    }
}
