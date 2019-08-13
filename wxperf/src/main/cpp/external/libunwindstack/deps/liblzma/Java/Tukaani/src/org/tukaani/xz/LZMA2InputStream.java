/*
 * LZMA2InputStream
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.DataInputStream;
import java.io.IOException;
import org.tukaani.xz.lz.LZDecoder;
import org.tukaani.xz.rangecoder.RangeDecoderFromBuffer;
import org.tukaani.xz.lzma.LZMADecoder;

/**
 * Decompresses a raw LZMA2 stream (no XZ headers).
 */
public class LZMA2InputStream extends InputStream {
    /**
     * Smallest valid LZMA2 dictionary size.
     * <p>
     * Very tiny dictionaries would be a performance problem, so
     * the minimum is 4 KiB.
     */
    public static final int DICT_SIZE_MIN = 4096;

    /**
     * Largest dictionary size supported by this implementation.
     * <p>
     * The LZMA2 algorithm allows dictionaries up to one byte less than 4 GiB.
     * This implementation supports only 16 bytes less than 2 GiB for raw
     * LZMA2 streams, and for .xz files the maximum is 1.5 GiB. This
     * limitation is due to Java using signed 32-bit integers for array
     * indexing. The limitation shouldn't matter much in practice since so
     * huge dictionaries are not normally used.
     */
    public static final int DICT_SIZE_MAX = Integer.MAX_VALUE & ~15;

    private static final int COMPRESSED_SIZE_MAX = 1 << 16;

    private DataInputStream in;

    private final LZDecoder lz;
    private final RangeDecoderFromBuffer rc
            = new RangeDecoderFromBuffer(COMPRESSED_SIZE_MAX);
    private LZMADecoder lzma;

    private int uncompressedSize = 0;
    private boolean isLZMAChunk;

    private boolean needDictReset = true;
    private boolean needProps = true;
    private boolean endReached = false;

    private IOException exception = null;

    private final byte[] tempBuf = new byte[1];

    /**
     * Gets approximate decompressor memory requirements as kibibytes for
     * the given dictionary size.
     *
     * @param       dictSize    LZMA2 dictionary size as bytes, must be
     *                          in the range [<code>DICT_SIZE_MIN</code>,
     *                          <code>DICT_SIZE_MAX</code>]
     *
     * @return      approximate memory requirements as kibibytes (KiB)
     */
    public static int getMemoryUsage(int dictSize) {
        // The base state is around 30-40 KiB (probabilities etc.),
        // range decoder needs COMPRESSED_SIZE_MAX bytes for buffering,
        // and LZ decoder needs a dictionary buffer.
        return 40 + COMPRESSED_SIZE_MAX / 1024 + getDictSize(dictSize) / 1024;
    }

    private static int getDictSize(int dictSize) {
        if (dictSize < DICT_SIZE_MIN || dictSize > DICT_SIZE_MAX)
            throw new IllegalArgumentException(
                    "Unsupported dictionary size " + dictSize);

        // Round dictionary size upward to a multiple of 16. This way LZMA
        // can use LZDecoder.getPos() for calculating LZMA's posMask.
        // Note that this check is needed only for raw LZMA2 streams; it is
        // redundant with .xz.
        return (dictSize + 15) & ~15;
    }

    /**
     * Creates a new input stream that decompresses raw LZMA2 data
     * from <code>in</code>.
     * <p>
     * The caller needs to know the dictionary size used when compressing;
     * the dictionary size isn't stored as part of a raw LZMA2 stream.
     * <p>
     * Specifying a too small dictionary size will prevent decompressing
     * the stream. Specifying a too big dictionary is waste of memory but
     * decompression will work.
     * <p>
     * There is no need to specify a dictionary bigger than
     * the uncompressed size of the data even if a bigger dictionary
     * was used when compressing. If you know the uncompressed size
     * of the data, this might allow saving some memory.
     *
     * @param       in          input stream from which LZMA2-compressed
     *                          data is read
     *
     * @param       dictSize    LZMA2 dictionary size as bytes, must be
     *                          in the range [<code>DICT_SIZE_MIN</code>,
     *                          <code>DICT_SIZE_MAX</code>]
     */
    public LZMA2InputStream(InputStream in, int dictSize) {
        this(in, dictSize, null);
    }

    /**
     * Creates a new LZMA2 decompressor using a preset dictionary.
     * <p>
     * This is like <code>LZMA2InputStream(InputStream, int)</code> except
     * that the dictionary may be initialized using a preset dictionary.
     * If a preset dictionary was used when compressing the data, the
     * same preset dictionary must be provided when decompressing.
     *
     * @param       in          input stream from which LZMA2-compressed
     *                          data is read
     *
     * @param       dictSize    LZMA2 dictionary size as bytes, must be
     *                          in the range [<code>DICT_SIZE_MIN</code>,
     *                          <code>DICT_SIZE_MAX</code>]
     *
     * @param       presetDict  preset dictionary or <code>null</code>
     *                          to use no preset dictionary
     */
    public LZMA2InputStream(InputStream in, int dictSize, byte[] presetDict) {
        // Check for null because otherwise null isn't detect
        // in this constructor.
        if (in == null)
            throw new NullPointerException();

        this.in = new DataInputStream(in);
        this.lz = new LZDecoder(getDictSize(dictSize), presetDict);

        if (presetDict != null && presetDict.length > 0)
            needDictReset = false;
    }

    /**
     * Decompresses the next byte from this input stream.
     * <p>
     * Reading lots of data with <code>read()</code> from this input stream
     * may be inefficient. Wrap it in <code>java.io.BufferedInputStream</code>
     * if you need to read lots of data one byte at a time.
     *
     * @return      the next decompressed byte, or <code>-1</code>
     *              to indicate the end of the compressed stream
     *
     * @throws      CorruptedInputException
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
     * is returned. Otherwise this will block until <code>len</code>
     * bytes have been decompressed, the end of the LZMA2 stream is reached,
     * or an exception is thrown.
     *
     * @param       buf         target buffer for uncompressed data
     * @param       off         start offset in <code>buf</code>
     * @param       len         maximum number of uncompressed bytes to read
     *
     * @return      number of bytes read, or <code>-1</code> to indicate
     *              the end of the compressed stream
     *
     * @throws      CorruptedInputException
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

        try {
            int size = 0;

            while (len > 0) {
                if (uncompressedSize == 0) {
                    decodeChunkHeader();
                    if (endReached)
                        return size == 0 ? -1 : size;
                }

                int copySizeMax = Math.min(uncompressedSize, len);

                if (!isLZMAChunk) {
                    lz.copyUncompressed(in, copySizeMax);
                } else {
                    lz.setLimit(copySizeMax);
                    lzma.decode();
                    if (!rc.isInBufferOK())
                        throw new CorruptedInputException();
                }

                int copiedSize = lz.flush(buf, off);
                off += copiedSize;
                len -= copiedSize;
                size += copiedSize;
                uncompressedSize -= copiedSize;

                if (uncompressedSize == 0)
                    if (!rc.isFinished() || lz.hasPending())
                        throw new CorruptedInputException();
            }

            return size;

        } catch (IOException e) {
            exception = e;
            throw e;
        }
    }

    private void decodeChunkHeader() throws IOException {
        int control = in.readUnsignedByte();

        if (control == 0x00) {
            endReached = true;
            return;
        }

        if (control >= 0xE0 || control == 0x01) {
            needProps = true;
            needDictReset = false;
            lz.reset();
        } else if (needDictReset) {
            throw new CorruptedInputException();
        }

        if (control >= 0x80) {
            isLZMAChunk = true;

            uncompressedSize = (control & 0x1F) << 16;
            uncompressedSize += in.readUnsignedShort() + 1;

            int compressedSize = in.readUnsignedShort() + 1;

            if (control >= 0xC0) {
                needProps = false;
                decodeProps();

            } else if (needProps) {
                throw new CorruptedInputException();

            } else if (control >= 0xA0) {
                lzma.reset();
            }

            rc.prepareInputBuffer(in, compressedSize);

        } else if (control > 0x02) {
            throw new CorruptedInputException();

        } else {
            isLZMAChunk = false;
            uncompressedSize = in.readUnsignedShort() + 1;
        }
    }

    private void decodeProps() throws IOException {
        int props = in.readUnsignedByte();

        if (props > (4 * 5 + 4) * 9 + 8)
            throw new CorruptedInputException();

        int pb = props / (9 * 5);
        props -= pb * 9 * 5;
        int lp = props / 9;
        int lc = props - lp * 9;

        if (lc + lp > 4)
            throw new CorruptedInputException();

        lzma = new LZMADecoder(lz, rc, lc, lp, pb);
    }

    /**
     * Returns the number of uncompressed bytes that can be read
     * without blocking. The value is returned with an assumption
     * that the compressed input data will be valid. If the compressed
     * data is corrupt, <code>CorruptedInputException</code> may get
     * thrown before the number of bytes claimed to be available have
     * been read from this input stream.
     * <p>
     * In LZMA2InputStream, the return value will be non-zero when the
     * decompressor is in the middle of an LZMA2 chunk. The return value
     * will then be the number of uncompressed bytes remaining from that
     * chunk.
     *
     * @return      the number of uncompressed bytes that can be read
     *              without blocking
     */
    public int available() throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (exception != null)
            throw exception;

        return uncompressedSize;
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
