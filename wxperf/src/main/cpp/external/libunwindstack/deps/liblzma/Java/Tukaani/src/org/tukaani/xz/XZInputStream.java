/*
 * XZInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.EOFException;
import org.tukaani.xz.common.DecoderUtil;

/**
 * Decompresses a .xz file in streamed mode (no seeking).
 * <p>
 * Use this to decompress regular standalone .xz files. This reads from
 * its input stream until the end of the input or until an error occurs.
 * This supports decompressing concatenated .xz files.
 *
 * <h4>Typical use cases</h4>
 * <p>
 * Getting an input stream to decompress a .xz file:
 * <p><blockquote><pre>
 * InputStream infile = new FileInputStream("foo.xz");
 * XZInputStream inxz = new XZInputStream(infile);
 * </pre></blockquote>
 * <p>
 * It's important to keep in mind that decompressor memory usage depends
 * on the settings used to compress the file. The worst-case memory usage
 * of XZInputStream is currently 1.5&nbsp;GiB. Still, very few files will
 * require more than about 65&nbsp;MiB because that's how much decompressing
 * a file created with the highest preset level will need, and only a few
 * people use settings other than the predefined presets.
 * <p>
 * It is possible to specify a memory usage limit for
 * <code>XZInputStream</code>. If decompression requires more memory than
 * the specified limit, MemoryLimitException will be thrown when reading
 * from the stream. For example, the following sets the memory usage limit
 * to 100&nbsp;MiB:
 * <p><blockquote><pre>
 * InputStream infile = new FileInputStream("foo.xz");
 * XZInputStream inxz = new XZInputStream(infile, 100 * 1024);
 * </pre></blockquote>
 *
 * <h4>When uncompressed size is known beforehand</h4>
 * <p>
 * If you are decompressing complete files and your application knows
 * exactly how much uncompressed data there should be, it is good to try
 * reading one more byte by calling <code>read()</code> and checking
 * that it returns <code>-1</code>. This way the decompressor will parse the
 * file footers and verify the integrity checks, giving the caller more
 * confidence that the uncompressed data is valid. (This advice seems to
 * apply to
 * {@link java.util.zip.GZIPInputStream java.util.zip.GZIPInputStream} too.)
 *
 * @see SingleXZInputStream
 */
public class XZInputStream extends InputStream {
    private final int memoryLimit;
    private InputStream in;
    private SingleXZInputStream xzIn;
    private final boolean verifyCheck;
    private boolean endReached = false;
    private IOException exception = null;

    private final byte[] tempBuf = new byte[1];

    /**
     * Creates a new XZ decompressor without a memory usage limit.
     * <p>
     * This constructor reads and parses the XZ Stream Header (12 bytes)
     * from <code>in</code>. The header of the first Block is not read
     * until <code>read</code> is called.
     *
     * @param       in          input stream from which XZ-compressed
     *                          data is read
     *
     * @throws      XZFormatException
     *                          input is not in the XZ format
     *
     * @throws      CorruptedInputException
     *                          XZ header CRC32 doesn't match
     *
     * @throws      UnsupportedOptionsException
     *                          XZ header is valid but specifies options
     *                          not supported by this implementation
     *
     * @throws      EOFException
     *                          less than 12 bytes of input was available
     *                          from <code>in</code>
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public XZInputStream(InputStream in) throws IOException {
        this(in, -1);
    }

    /**
     * Creates a new XZ decompressor with an optional memory usage limit.
     * <p>
     * This is identical to <code>XZInputStream(InputStream)</code> except
     * that this takes also the <code>memoryLimit</code> argument.
     *
     * @param       in          input stream from which XZ-compressed
     *                          data is read
     *
     * @param       memoryLimit memory usage limit in kibibytes (KiB)
     *                          or <code>-1</code> to impose no
     *                          memory usage limit
     *
     * @throws      XZFormatException
     *                          input is not in the XZ format
     *
     * @throws      CorruptedInputException
     *                          XZ header CRC32 doesn't match
     *
     * @throws      UnsupportedOptionsException
     *                          XZ header is valid but specifies options
     *                          not supported by this implementation
     *
     * @throws      EOFException
     *                          less than 12 bytes of input was available
     *                          from <code>in</code>
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public XZInputStream(InputStream in, int memoryLimit) throws IOException {
        this(in, memoryLimit, true);
    }

    /**
     * Creates a new XZ decompressor with an optional memory usage limit
     * and ability to disable verification of integrity checks.
     * <p>
     * This is identical to <code>XZInputStream(InputStream,int)</code> except
     * that this takes also the <code>verifyCheck</code> argument.
     * <p>
     * Note that integrity check verification should almost never be disabled.
     * Possible reasons to disable integrity check verification:
     * <ul>
     *   <li>Trying to recover data from a corrupt .xz file.</li>
     *   <li>Speeding up decompression. This matters mostly with SHA-256
     *   or with files that have compressed extremely well. It's recommended
     *   that integrity checking isn't disabled for performance reasons
     *   unless the file integrity is verified externally in some other
     *   way.</li>
     * </ul>
     * <p>
     * <code>verifyCheck</code> only affects the integrity check of
     * the actual compressed data. The CRC32 fields in the headers
     * are always verified.
     *
     * @param       in          input stream from which XZ-compressed
     *                          data is read
     *
     * @param       memoryLimit memory usage limit in kibibytes (KiB)
     *                          or <code>-1</code> to impose no
     *                          memory usage limit
     *
     * @param       verifyCheck if <code>true</code>, the integrity checks
     *                          will be verified; this should almost never
     *                          be set to <code>false</code>
     *
     * @throws      XZFormatException
     *                          input is not in the XZ format
     *
     * @throws      CorruptedInputException
     *                          XZ header CRC32 doesn't match
     *
     * @throws      UnsupportedOptionsException
     *                          XZ header is valid but specifies options
     *                          not supported by this implementation
     *
     * @throws      EOFException
     *                          less than 12 bytes of input was available
     *                          from <code>in</code>
     *
     * @throws      IOException may be thrown by <code>in</code>
     *
     * @since 1.6
     */
    public XZInputStream(InputStream in, int memoryLimit, boolean verifyCheck)
            throws IOException {
        this.in = in;
        this.memoryLimit = memoryLimit;
        this.verifyCheck = verifyCheck;
        this.xzIn = new SingleXZInputStream(in, memoryLimit, verifyCheck);
    }

    /**
     * Decompresses the next byte from this input stream.
     * <p>
     * Reading lots of data with <code>read()</code> from this input stream
     * may be inefficient. Wrap it in {@link java.io.BufferedInputStream}
     * if you need to read lots of data one byte at a time.
     *
     * @return      the next decompressed byte, or <code>-1</code>
     *              to indicate the end of the compressed stream
     *
     * @throws      CorruptedInputException
     * @throws      UnsupportedOptionsException
     * @throws      MemoryLimitException
     *
     * @throws      XZIOException if the stream has been closed
     *
     * @throws      EOFException
     *                          compressed input is truncated or corrupt
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public int read() throws IOException {
        return read(tempBuf, 0, 1) == -1 ? -1 : (tempBuf[0] & 0xFF);
    }

    /**
     * Decompresses into an array of bytes.
     * <p>
     * If <code>len</code> is zero, no bytes are read and <code>0</code>
     * is returned. Otherwise this will try to decompress <code>len</code>
     * bytes of uncompressed data. Less than <code>len</code> bytes may
     * be read only in the following situations:
     * <ul>
     *   <li>The end of the compressed data was reached successfully.</li>
     *   <li>An error is detected after at least one but less <code>len</code>
     *       bytes have already been successfully decompressed.
     *       The next call with non-zero <code>len</code> will immediately
     *       throw the pending exception.</li>
     *   <li>An exception is thrown.</li>
     * </ul>
     *
     * @param       buf         target buffer for uncompressed data
     * @param       off         start offset in <code>buf</code>
     * @param       len         maximum number of uncompressed bytes to read
     *
     * @return      number of bytes read, or <code>-1</code> to indicate
     *              the end of the compressed stream
     *
     * @throws      CorruptedInputException
     * @throws      UnsupportedOptionsException
     * @throws      MemoryLimitException
     *
     * @throws      XZIOException if the stream has been closed
     *
     * @throws      EOFException
     *                          compressed input is truncated or corrupt
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public int read(byte[] buf, int off, int len) throws IOException {
        if (off < 0 || len < 0 || off + len < 0 || off + len > buf.length)
            throw new IndexOutOfBoundsException();

        if (len == 0)
            return 0;

        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        if (endReached)
            return -1;

        int size = 0;

        try {
            while (len > 0) {
                if (xzIn == null) {
                    prepareNextStream();
                    if (endReached)
                        return size == 0 ? -1 : size;
                }

                int ret = xzIn.read(buf, off, len);

                if (ret > 0) {
                    size += ret;
                    off += ret;
                    len -= ret;
                } else if (ret == -1) {
                    xzIn = null;
                }
            }
        } catch (IOException e) {
            exception = e;
            if (size == 0)
                throw e;
        }

        return size;
    }

    private void prepareNextStream() throws IOException {
        DataInputStream inData = new DataInputStream(in);
        byte[] buf = new byte[DecoderUtil.STREAM_HEADER_SIZE];

        // The size of Stream Padding must be a multiple of four bytes,
        // all bytes zero.
        do {
            // First try to read one byte to see if we have reached the end
            // of the file.
            int ret = inData.read(buf, 0, 1);
            if (ret == -1) {
                endReached = true;
                return;
            }

            // Since we got one byte of input, there must be at least
            // three more available in a valid file.
            inData.readFully(buf, 1, 3);

        } while (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0);

        // Not all bytes are zero. In a valid Stream it indicates the
        // beginning of the next Stream. Read the rest of the Stream Header
        // and initialize the XZ decoder.
        inData.readFully(buf, 4, DecoderUtil.STREAM_HEADER_SIZE - 4);

        try {
            xzIn = new SingleXZInputStream(in, memoryLimit, verifyCheck, buf);
        } catch (XZFormatException e) {
            // Since this isn't the first .xz Stream, it is more
            // logical to tell that the data is corrupt.
            throw new CorruptedInputException(
                    "Garbage after a valid XZ Stream");
        }
    }

    /**
     * Returns the number of uncompressed bytes that can be read
     * without blocking. The value is returned with an assumption
     * that the compressed input data will be valid. If the compressed
     * data is corrupt, <code>CorruptedInputException</code> may get
     * thrown before the number of bytes claimed to be available have
     * been read from this input stream.
     *
     * @return      the number of uncompressed bytes that can be read
     *              without blocking
     */
    public int available() throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        return xzIn == null ? 0 : xzIn.available();
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
