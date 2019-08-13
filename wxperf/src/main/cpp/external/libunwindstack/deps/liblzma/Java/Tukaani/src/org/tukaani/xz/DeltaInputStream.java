/*
 * DeltaInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.IOException;
import org.tukaani.xz.delta.DeltaDecoder;

/**
 * Decodes raw Delta-filtered data (no XZ headers).
 * <p>
 * The delta filter doesn't change the size of the data and thus it
 * cannot have an end-of-payload marker. It will simply decode until
 * its input stream indicates end of input.
 */
public class DeltaInputStream extends InputStream {
    /**
     * Smallest supported delta calculation distance.
     */
    public static final int DISTANCE_MIN = 1;

    /**
     * Largest supported delta calculation distance.
     */
    public static final int DISTANCE_MAX = 256;

    private InputStream in;
    private final DeltaDecoder delta;

    private IOException exception = null;

    private final byte[] tempBuf = new byte[1];

    /**
     * Creates a new Delta decoder with the given delta calculation distance.
     *
     * @param       in          input stream from which Delta filtered data
     *                          is read
     *
     * @param       distance    delta calculation distance, must be in the
     *                          range [<code>DISTANCE_MIN</code>,
     *                          <code>DISTANCE_MAX</code>]
     */
    public DeltaInputStream(InputStream in, int distance) {
        // Check for null because otherwise null isn't detect
        // in this constructor.
        if (in == null)
            throw new NullPointerException();

        this.in = in;
        this.delta = new DeltaDecoder(distance);
    }

    /**
     * Decode the next byte from this input stream.
     *
     * @return      the next decoded byte, or <code>-1</code> to indicate
     *              the end of input on the input stream <code>in</code>
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public int read() throws IOException {
        return read(tempBuf, 0, 1) == -1 ? -1 : (tempBuf[0] & 0xFF);
    }

    /**
     * Decode into an array of bytes.
     * <p>
     * This calls <code>in.read(buf, off, len)</code> and defilters the
     * returned data.
     *
     * @param       buf         target buffer for decoded data
     * @param       off         start offset in <code>buf</code>
     * @param       len         maximum number of bytes to read
     *
     * @return      number of bytes read, or <code>-1</code> to indicate
     *              the end of the input stream <code>in</code>
     *
     * @throws      XZIOException if the stream has been closed
     *
     * @throws      IOException may be thrown by underlaying input
     *                          stream <code>in</code>
     */
    public int read(byte[] buf, int off, int len) throws IOException {
        if (len == 0)
            return 0;

        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        int size;
        try {
            size = in.read(buf, off, len);
        } catch (IOException e) {
            exception = e;
            throw e;
        }

        if (size == -1)
            return -1;

        delta.decode(buf, off, size);
        return size;
    }

    /**
     * Calls <code>in.available()</code>.
     *
     * @return      the value returned by <code>in.available()</code>
     */
    public int available() throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        return in.available();
    }

    /**
     * Closes the stream and calls <code>in.close()</code>.
     * If the stream was already closed, this does nothing.
     *
     * @throws  IOException if thrown by <code>in.close()</code>
     */
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
