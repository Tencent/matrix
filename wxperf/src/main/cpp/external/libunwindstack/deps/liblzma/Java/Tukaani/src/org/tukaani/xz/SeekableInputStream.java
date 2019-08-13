/*
 * SeekableInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.IOException;

/**
 * Input stream with random access support.
 */
public abstract class SeekableInputStream extends InputStream {
    /**
     * Seeks <code>n</code> bytes forward in this stream.
     * <p>
     * This will not seek past the end of the file. If the current position
     * is already at or past the end of the file, this doesn't seek at all
     * and returns <code>0</code>. Otherwise, if skipping <code>n</code> bytes
     * would cause the position to exceed the stream size, this will do
     * equivalent of <code>seek(length())</code> and the return value will
     * be adjusted accordingly.
     * <p>
     * If <code>n</code> is negative, the position isn't changed and
     * the return value is <code>0</code>. It doesn't seek backward
     * because it would conflict with the specification of
     * {@link InputStream#skip(long) InputStream.skip}.
     *
     * @return      <code>0</code> if <code>n</code> is negative,
     *              less than <code>n</code> if skipping <code>n</code>
     *              bytes would seek past the end of the file,
     *              <code>n</code> otherwise
     *
     * @throws      IOException might be thrown by {@link #seek(long)}
     */
    public long skip(long n) throws IOException {
        if (n <= 0)
            return 0;

        long size = length();
        long pos = position();
        if (pos >= size)
            return 0;

        if (size - pos < n)
            n = size - pos;

        seek(pos + n);
        return n;
    }

    /**
     * Gets the size of the stream.
     */
    public abstract long length() throws IOException;

    /**
     * Gets the current position in the stream.
     */
    public abstract long position() throws IOException;

    /**
     * Seeks to the specified absolute position in the stream.
     * <p>
     * Seeking past the end of the file should be supported by the subclasses
     * unless there is a good reason to do otherwise. If one has seeked
     * past the end of the stream, <code>read</code> will return
     * <code>-1</code> to indicate end of stream.
     *
     * @param       pos         new read position in the stream
     *
     * @throws      IOException if <code>pos</code> is negative or if
     *                          a stream-specific I/O error occurs
     */
    public abstract void seek(long pos) throws IOException;
}
