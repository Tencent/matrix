/*
 * SeekableXZInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.util.Arrays;
import java.util.ArrayList;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.EOFException;
import org.tukaani.xz.common.DecoderUtil;
import org.tukaani.xz.common.StreamFlags;
import org.tukaani.xz.check.Check;
import org.tukaani.xz.index.IndexDecoder;
import org.tukaani.xz.index.BlockInfo;

/**
 * Decompresses a .xz file in random access mode.
 * This supports decompressing concatenated .xz files.
 * <p>
 * Each .xz file consist of one or more Streams. Each Stream consist of zero
 * or more Blocks. Each Stream contains an Index of Streams' Blocks.
 * The Indexes from all Streams are loaded in RAM by a constructor of this
 * class. A typical .xz file has only one Stream, and parsing its Index will
 * need only three or four seeks.
 * <p>
 * To make random access possible, the data in a .xz file must be splitted
 * into multiple Blocks of reasonable size. Decompression can only start at
 * a Block boundary. When seeking to an uncompressed position that is not at
 * a Block boundary, decompression starts at the beginning of the Block and
 * throws away data until the target position is reached. Thus, smaller Blocks
 * mean faster seeks to arbitrary uncompressed positions. On the other hand,
 * smaller Blocks mean worse compression. So one has to make a compromise
 * between random access speed and compression ratio.
 * <p>
 * Implementation note: This class uses linear search to locate the correct
 * Stream from the data structures in RAM. It was the simplest to implement
 * and should be fine as long as there aren't too many Streams. The correct
 * Block inside a Stream is located using binary search and thus is fast
 * even with a huge number of Blocks.
 *
 * <h4>Memory usage</h4>
 * <p>
 * The amount of memory needed for the Indexes is taken into account when
 * checking the memory usage limit. Each Stream is calculated to need at
 * least 1&nbsp;KiB of memory and each Block 16 bytes of memory, rounded up
 * to the next kibibyte. So unless the file has a huge number of Streams or
 * Blocks, these don't take significant amount of memory.
 *
 * <h4>Creating random-accessible .xz files</h4>
 * <p>
 * When using {@link XZOutputStream}, a new Block can be started by calling
 * its {@link XZOutputStream#endBlock() endBlock} method. If you know
 * that the decompressor will only need to seek to certain uncompressed
 * positions, it can be a good idea to start a new Block at (some of) these
 * positions (and only at these positions to get better compression ratio).
 * <p>
 * liblzma in XZ Utils supports starting a new Block with
 * <code>LZMA_FULL_FLUSH</code>. XZ Utils 5.1.1alpha added threaded
 * compression which creates multi-Block .xz files. XZ Utils 5.1.1alpha
 * also added the option <code>--block-size=SIZE</code> to the xz command
 * line tool. XZ Utils 5.1.2alpha added a partial implementation of
 * <code>--block-list=SIZES</code> which allows specifying sizes of
 * individual Blocks.
 *
 * @see SeekableFileInputStream
 * @see XZInputStream
 * @see XZOutputStream
 */
public class SeekableXZInputStream extends SeekableInputStream {
    /**
     * The input stream containing XZ compressed data.
     */
    private SeekableInputStream in;

    /**
     * Memory usage limit after the memory usage of the IndexDecoders have
     * been substracted.
     */
    private final int memoryLimit;

    /**
     * Memory usage of the IndexDecoders.
     * <code>memoryLimit + indexMemoryUsage</code> equals the original
     * memory usage limit that was passed to the constructor.
     */
    private int indexMemoryUsage = 0;

    /**
     * List of IndexDecoders, one for each Stream in the file.
     * The list is in reverse order: The first element is
     * the last Stream in the file.
     */
    private final ArrayList streams = new ArrayList();

    /**
     * Bitmask of all Check IDs seen.
     */
    private int checkTypes = 0;

    /**
     * Uncompressed size of the file (all Streams).
     */
    private long uncompressedSize = 0;

    /**
     * Uncompressed size of the largest XZ Block in the file.
     */
    private long largestBlockSize = 0;

    /**
     * Number of XZ Blocks in the file.
     */
    private int blockCount = 0;

    /**
     * Size and position information about the current Block.
     * If there are no Blocks, all values will be <code>-1</code>.
     */
    private final BlockInfo curBlockInfo;

    /**
     * Temporary (and cached) information about the Block whose information
     * is queried via <code>getBlockPos</code> and related functions.
     */
    private final BlockInfo queriedBlockInfo;

    /**
     * Integrity Check in the current XZ Stream. The constructor leaves
     * this to point to the Check of the first Stream.
     */
    private Check check;

    /**
     * Flag indicating if the integrity checks will be verified.
     */
    private final boolean verifyCheck;

    /**
     * Decoder of the current XZ Block, if any.
     */
    private BlockInputStream blockDecoder = null;

    /**
     * Current uncompressed position.
     */
    private long curPos = 0;

    /**
     * Target position for seeking.
     */
    private long seekPos;

    /**
     * True when <code>seek(long)</code> has been called but the actual
     * seeking hasn't been done yet.
     */
    private boolean seekNeeded = false;

    /**
     * True when end of the file was reached. This can be cleared by
     * calling <code>seek(long)</code>.
     */
    private boolean endReached = false;

    /**
     * Pending exception from an earlier error.
     */
    private IOException exception = null;

    /**
     * Temporary buffer for read(). This avoids reallocating memory
     * on every read() call.
     */
    private final byte[] tempBuf = new byte[1];

    /**
     * Creates a new seekable XZ decompressor without a memory usage limit.
     *
     * @param       in          seekable input stream containing one or more
     *                          XZ Streams; the whole input stream is used
     *
     * @throws      XZFormatException
     *                          input is not in the XZ format
     *
     * @throws      CorruptedInputException
     *                          XZ data is corrupt or truncated
     *
     * @throws      UnsupportedOptionsException
     *                          XZ headers seem valid but they specify
     *                          options not supported by this implementation
     *
     * @throws      EOFException
     *                          less than 6 bytes of input was available
     *                          from <code>in</code>, or (unlikely) the size
     *                          of the underlying stream got smaller while
     *                          this was reading from it
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public SeekableXZInputStream(SeekableInputStream in)
            throws IOException {
        this(in, -1);
    }

    /**
     * Creates a new seekable XZ decomporessor with an optional
     * memory usage limit.
     *
     * @param       in          seekable input stream containing one or more
     *                          XZ Streams; the whole input stream is used
     *
     * @param       memoryLimit memory usage limit in kibibytes (KiB)
     *                          or <code>-1</code> to impose no
     *                          memory usage limit
     *
     * @throws      XZFormatException
     *                          input is not in the XZ format
     *
     * @throws      CorruptedInputException
     *                          XZ data is corrupt or truncated
     *
     * @throws      UnsupportedOptionsException
     *                          XZ headers seem valid but they specify
     *                          options not supported by this implementation
     *
     * @throws      MemoryLimitException
     *                          decoded XZ Indexes would need more memory
     *                          than allowed by the memory usage limit
     *
     * @throws      EOFException
     *                          less than 6 bytes of input was available
     *                          from <code>in</code>, or (unlikely) the size
     *                          of the underlying stream got smaller while
     *                          this was reading from it
     *
     * @throws      IOException may be thrown by <code>in</code>
     */
    public SeekableXZInputStream(SeekableInputStream in, int memoryLimit)
            throws IOException {
        this(in, memoryLimit, true);
    }

    /**
     * Creates a new seekable XZ decomporessor with an optional
     * memory usage limit and ability to disable verification
     * of integrity checks.
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
     * @param       in          seekable input stream containing one or more
     *                          XZ Streams; the whole input stream is used
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
     *                          XZ data is corrupt or truncated
     *
     * @throws      UnsupportedOptionsException
     *                          XZ headers seem valid but they specify
     *                          options not supported by this implementation
     *
     * @throws      MemoryLimitException
     *                          decoded XZ Indexes would need more memory
     *                          than allowed by the memory usage limit
     *
     * @throws      EOFException
     *                          less than 6 bytes of input was available
     *                          from <code>in</code>, or (unlikely) the size
     *                          of the underlying stream got smaller while
     *                          this was reading from it
     *
     * @throws      IOException may be thrown by <code>in</code>
     *
     * @since 1.6
     */
    public SeekableXZInputStream(SeekableInputStream in, int memoryLimit,
                                 boolean verifyCheck)
            throws IOException {
        this.verifyCheck = verifyCheck;
        this.in = in;
        DataInputStream inData = new DataInputStream(in);

        // Check the magic bytes in the beginning of the file.
        {
            in.seek(0);
            byte[] buf = new byte[XZ.HEADER_MAGIC.length];
            inData.readFully(buf);
            if (!Arrays.equals(buf, XZ.HEADER_MAGIC))
                throw new XZFormatException();
        }

        // Get the file size and verify that it is a multiple of 4 bytes.
        long pos = in.length();
        if ((pos & 3) != 0)
            throw new CorruptedInputException(
                    "XZ file size is not a multiple of 4 bytes");

        // Parse the headers starting from the end of the file.
        byte[] buf = new byte[DecoderUtil.STREAM_HEADER_SIZE];
        long streamPadding = 0;

        while (pos > 0) {
            if (pos < DecoderUtil.STREAM_HEADER_SIZE)
                throw new CorruptedInputException();

            // Read the potential Stream Footer.
            in.seek(pos - DecoderUtil.STREAM_HEADER_SIZE);
            inData.readFully(buf);

            // Skip Stream Padding four bytes at a time.
            // Skipping more at once would be faster,
            // but usually there isn't much Stream Padding.
            if (buf[8] == 0x00 && buf[9] == 0x00 && buf[10] == 0x00
                    && buf[11] == 0x00) {
                streamPadding += 4;
                pos -= 4;
                continue;
            }

            // It's not Stream Padding. Update pos.
            pos -= DecoderUtil.STREAM_HEADER_SIZE;

            // Decode the Stream Footer and check if Backward Size
            // looks reasonable.
            StreamFlags streamFooter = DecoderUtil.decodeStreamFooter(buf);
            if (streamFooter.backwardSize >= pos)
                throw new CorruptedInputException(
                        "Backward Size in XZ Stream Footer is too big");

            // Check that the Check ID is supported. Store it in case this
            // is the first Stream in the file.
            check = Check.getInstance(streamFooter.checkType);

            // Remember which Check IDs have been seen.
            checkTypes |= 1 << streamFooter.checkType;

            // Seek to the beginning of the Index.
            in.seek(pos - streamFooter.backwardSize);

            // Decode the Index field.
            IndexDecoder index;
            try {
                index = new IndexDecoder(in, streamFooter, streamPadding,
                                         memoryLimit);
            } catch (MemoryLimitException e) {
                // IndexDecoder doesn't know how much memory we had
                // already needed so we need to recreate the exception.
                assert memoryLimit >= 0;
                throw new MemoryLimitException(
                        e.getMemoryNeeded() + indexMemoryUsage,
                        memoryLimit + indexMemoryUsage);
            }

            // Update the memory usage and limit counters.
            indexMemoryUsage += index.getMemoryUsage();
            if (memoryLimit >= 0) {
                memoryLimit -= index.getMemoryUsage();
                assert memoryLimit >= 0;
            }

            // Remember the uncompressed size of the largest Block.
            if (largestBlockSize < index.getLargestBlockSize())
                largestBlockSize = index.getLargestBlockSize();

            // Calculate the offset to the beginning of this XZ Stream and
            // check that it looks sane.
            long off = index.getStreamSize() - DecoderUtil.STREAM_HEADER_SIZE;
            if (pos < off)
                throw new CorruptedInputException("XZ Index indicates "
                        + "too big compressed size for the XZ Stream");

            // Seek to the beginning of this Stream.
            pos -= off;
            in.seek(pos);

            // Decode the Stream Header.
            inData.readFully(buf);
            StreamFlags streamHeader = DecoderUtil.decodeStreamHeader(buf);

            // Verify that the Stream Header matches the Stream Footer.
            if (!DecoderUtil.areStreamFlagsEqual(streamHeader, streamFooter))
                throw new CorruptedInputException(
                        "XZ Stream Footer does not match Stream Header");

            // Update the total uncompressed size of the file and check that
            // it doesn't overflow.
            uncompressedSize += index.getUncompressedSize();
            if (uncompressedSize < 0)
                throw new UnsupportedOptionsException("XZ file is too big");

            // Update the Block count and check that it fits into an int.
            blockCount += index.getRecordCount();
            if (blockCount < 0)
                throw new UnsupportedOptionsException(
                        "XZ file has over " + Integer.MAX_VALUE + " Blocks");

            // Add this Stream to the list of Streams.
            streams.add(index);

            // Reset to be ready to parse the next Stream.
            streamPadding = 0;
        }

        assert pos == 0;

        // Save it now that indexMemoryUsage has been substracted from it.
        this.memoryLimit = memoryLimit;

        // Store the relative offsets of the Streams. This way we don't
        // need to recalculate them in this class when seeking; the
        // IndexDecoder instances will handle them.
        IndexDecoder prev = (IndexDecoder)streams.get(streams.size() - 1);
        for (int i = streams.size() - 2; i >= 0; --i) {
            IndexDecoder cur = (IndexDecoder)streams.get(i);
            cur.setOffsets(prev);
            prev = cur;
        }

        // Initialize curBlockInfo to point to the first Stream.
        // The blockNumber will be left to -1 so that .hasNext()
        // and .setNext() work to get the first Block when starting
        // to decompress from the beginning of the file.
        IndexDecoder first = (IndexDecoder)streams.get(streams.size() - 1);
        curBlockInfo = new BlockInfo(first);

        // queriedBlockInfo needs to be allocated too. The Stream used for
        // initialization doesn't matter though.
        queriedBlockInfo = new BlockInfo(first);
    }

    /**
     * Gets the types of integrity checks used in the .xz file.
     * Multiple checks are possible only if there are multiple
     * concatenated XZ Streams.
     * <p>
     * The returned value has a bit set for every check type that is present.
     * For example, if CRC64 and SHA-256 were used, the return value is
     * <code>(1&nbsp;&lt;&lt;&nbsp;XZ.CHECK_CRC64)
     * | (1&nbsp;&lt;&lt;&nbsp;XZ.CHECK_SHA256)</code>.
     */
    public int getCheckTypes() {
        return checkTypes;
    }

    /**
     * Gets the amount of memory in kibibytes (KiB) used by
     * the data structures needed to locate the XZ Blocks.
     * This is usually useless information but since it is calculated
     * for memory usage limit anyway, it is nice to make it available to too.
     */
    public int getIndexMemoryUsage() {
        return indexMemoryUsage;
    }

    /**
     * Gets the uncompressed size of the largest XZ Block in bytes.
     * This can be useful if you want to check that the file doesn't
     * have huge XZ Blocks which could make seeking to arbitrary offsets
     * very slow. Note that huge Blocks don't automatically mean that
     * seeking would be slow, for example, seeking to the beginning of
     * any Block is always fast.
     */
    public long getLargestBlockSize() {
        return largestBlockSize;
    }

    /**
     * Gets the number of Streams in the .xz file.
     *
     * @since 1.3
     */
    public int getStreamCount() {
        return streams.size();
    }

    /**
     * Gets the number of Blocks in the .xz file.
     *
     * @since 1.3
     */
    public int getBlockCount() {
        return blockCount;
    }

    /**
     * Gets the uncompressed start position of the given Block.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *          <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>.
     *
     * @since 1.3
     */
    public long getBlockPos(int blockNumber) {
        locateBlockByNumber(queriedBlockInfo, blockNumber);
        return queriedBlockInfo.uncompressedOffset;
    }

    /**
     * Gets the uncompressed size of the given Block.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *          <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>.
     *
     * @since 1.3
     */
    public long getBlockSize(int blockNumber) {
        locateBlockByNumber(queriedBlockInfo, blockNumber);
        return queriedBlockInfo.uncompressedSize;
    }

    /**
     * Gets the position where the given compressed Block starts in
     * the underlying .xz file.
     * This information is rarely useful to the users of this class.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *          <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>.
     *
     * @since 1.3
     */
    public long getBlockCompPos(int blockNumber) {
        locateBlockByNumber(queriedBlockInfo, blockNumber);
        return queriedBlockInfo.compressedOffset;
    }

    /**
     * Gets the compressed size of the given Block.
     * This together with the uncompressed size can be used to calculate
     * the compression ratio of the specific Block.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *          <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>.
     *
     * @since 1.3
     */
    public long getBlockCompSize(int blockNumber) {
        locateBlockByNumber(queriedBlockInfo, blockNumber);
        return (queriedBlockInfo.unpaddedSize + 3) & ~3;
    }

    /**
     * Gets integrity check type (Check ID) of the given Block.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *          <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>.
     *
     * @see #getCheckTypes()
     *
     * @since 1.3
     */
    public int getBlockCheckType(int blockNumber) {
        locateBlockByNumber(queriedBlockInfo, blockNumber);
        return queriedBlockInfo.getCheckType();
    }

    /**
     * Gets the number of the Block that contains the byte at the given
     * uncompressed position.
     *
     * @throws  IndexOutOfBoundsException if
     *          <code>pos&nbsp;&lt;&nbsp;0</code> or
     *          <code>pos&nbsp;&gt;=&nbsp;length()</code>.
     *
     * @since 1.3
     */
    public int getBlockNumber(long pos) {
        locateBlockByPos(queriedBlockInfo, pos);
        return queriedBlockInfo.blockNumber;
    }

    /**
     * Decompresses the next byte from this input stream.
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
     *   <li>An error is detected after at least one but less than
     *       <code>len</code> bytes have already been successfully
     *       decompressed. The next call with non-zero <code>len</code>
     *       will immediately throw the pending exception.</li>
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

        int size = 0;

        try {
            if (seekNeeded)
                seek();

            if (endReached)
                return -1;

            while (len > 0) {
                if (blockDecoder == null) {
                    seek();
                    if (endReached)
                        break;
                }

                int ret = blockDecoder.read(buf, off, len);

                if (ret > 0) {
                    curPos += ret;
                    size += ret;
                    off += ret;
                    len -= ret;
                } else if (ret == -1) {
                    blockDecoder = null;
                }
            }
        } catch (IOException e) {
            // We know that the file isn't simply truncated because we could
            // parse the Indexes in the constructor. So convert EOFException
            // to CorruptedInputException.
            if (e instanceof EOFException)
                e = new CorruptedInputException();

            exception = e;
            if (size == 0)
                throw e;
        }

        return size;
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

        if (endReached || seekNeeded || blockDecoder == null)
            return 0;

        return blockDecoder.available();
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

    /**
     * Gets the uncompressed size of this input stream. If there are multiple
     * XZ Streams, the total uncompressed size of all XZ Streams is returned.
     */
    public long length() {
        return uncompressedSize;
    }

    /**
     * Gets the current uncompressed position in this input stream.
     *
     * @throws      XZIOException if the stream has been closed
     */
    public long position() throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        return seekNeeded ? seekPos : curPos;
    }

    /**
     * Seeks to the specified absolute uncompressed position in the stream.
     * This only stores the new position, so this function itself is always
     * very fast. The actual seek is done when <code>read</code> is called
     * to read at least one byte.
     * <p>
     * Seeking past the end of the stream is possible. In that case
     * <code>read</code> will return <code>-1</code> to indicate
     * the end of the stream.
     *
     * @param       pos         new uncompressed read position
     *
     * @throws      XZIOException
     *                          if <code>pos</code> is negative, or
     *                          if stream has been closed
     */
    public void seek(long pos) throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (pos < 0)
            throw new XZIOException("Negative seek position: " + pos);

        seekPos = pos;
        seekNeeded = true;
    }

    /**
     * Seeks to the beginning of the given XZ Block.
     *
     * @throws      XZIOException
     *              if <code>blockNumber&nbsp;&lt;&nbsp;0</code> or
     *              <code>blockNumber&nbsp;&gt;=&nbsp;getBlockCount()</code>,
     *              or if stream has been closed
     *
     * @since 1.3
     */
    public void seekToBlock(int blockNumber) throws IOException {
        if (in == null)
            throw new XZIOException("Stream closed");

        if (blockNumber < 0 || blockNumber >= blockCount)
            throw new XZIOException("Invalid XZ Block number: " + blockNumber);

        // This is a bit silly implementation. Here we locate the uncompressed
        // offset of the specified Block, then when doing the actual seek in
        // seek(), we need to find the Block number based on seekPos.
        seekPos = getBlockPos(blockNumber);
        seekNeeded = true;
    }

    /**
     * Does the actual seeking. This is also called when <code>read</code>
     * needs a new Block to decode.
     */
    private void seek() throws IOException {
        // If seek(long) wasn't called, we simply need to get the next Block
        // from the same Stream. If there are no more Blocks in this Stream,
        // then we behave as if seek(long) had been called.
        if (!seekNeeded) {
            if (curBlockInfo.hasNext()) {
                curBlockInfo.setNext();
                initBlockDecoder();
                return;
            }

            seekPos = curPos;
        }

        seekNeeded = false;

        // Check if we are seeking to or past the end of the file.
        if (seekPos >= uncompressedSize) {
            curPos = seekPos;
            blockDecoder = null;
            endReached = true;
            return;
        }

        endReached = false;

        // Locate the Block that contains the uncompressed target position.
        locateBlockByPos(curBlockInfo, seekPos);

        // Seek in the underlying stream and create a new Block decoder
        // only if really needed. We can skip it if the current position
        // is already in the correct Block and the target position hasn't
        // been decompressed yet.
        //
        // NOTE: If curPos points to the beginning of this Block, it's
        // because it was left there after decompressing an earlier Block.
        // In that case, decoding of the current Block hasn't been started
        // yet. (Decoding of a Block won't be started until at least one
        // byte will also be read from it.)
        if (!(curPos > curBlockInfo.uncompressedOffset && curPos <= seekPos)) {
            // Seek to the beginning of the Block.
            in.seek(curBlockInfo.compressedOffset);

            // Since it is possible that this Block is from a different
            // Stream than the previous Block, initialize a new Check.
            check = Check.getInstance(curBlockInfo.getCheckType());

            // Create a new Block decoder.
            initBlockDecoder();
            curPos = curBlockInfo.uncompressedOffset;
        }

        // If the target wasn't at a Block boundary, decompress and throw
        // away data to reach the target position.
        if (seekPos > curPos) {
            // NOTE: The "if" below is there just in case. In this situation,
            // blockDecoder.skip will always skip the requested amount
            // or throw an exception.
            long skipAmount = seekPos - curPos;
            if (blockDecoder.skip(skipAmount) != skipAmount)
                throw new CorruptedInputException();

            curPos = seekPos;
        }
    }

    /**
     * Locates the Block that contains the given uncompressed position.
     */
    private void locateBlockByPos(BlockInfo info, long pos) {
        if (pos < 0 || pos >= uncompressedSize)
            throw new IndexOutOfBoundsException(
                    "Invalid uncompressed position: " + pos);

        // Locate the Stream that contains the target position.
        IndexDecoder index;
        for (int i = 0; ; ++i) {
            index = (IndexDecoder)streams.get(i);
            if (index.hasUncompressedOffset(pos))
                break;
        }

        // Locate the Block from the Stream that contains the target position.
        index.locateBlock(info, pos);

        assert (info.compressedOffset & 3) == 0;
        assert info.uncompressedSize > 0;
        assert pos >= info.uncompressedOffset;
        assert pos < info.uncompressedOffset + info.uncompressedSize;
    }

    /**
     * Locates the given Block and stores information about it
     * to <code>info</code>.
     */
    private void locateBlockByNumber(BlockInfo info, int blockNumber) {
        // Validate.
        if (blockNumber < 0 || blockNumber >= blockCount)
            throw new IndexOutOfBoundsException(
                    "Invalid XZ Block number: " + blockNumber);

        // Skip the search if info already points to the correct Block.
        if (info.blockNumber == blockNumber)
            return;

        // Search the Stream that contains the given Block and then
        // search the Block from that Stream.
        for (int i = 0; ; ++i) {
            IndexDecoder index = (IndexDecoder)streams.get(i);
            if (index.hasRecord(blockNumber)) {
                index.setBlockInfo(info, blockNumber);
                return;
            }
        }
    }

    /**
     * Initializes a new BlockInputStream. This is a helper function for
     * <code>seek()</code>.
     */
    private void initBlockDecoder() throws IOException {
        try {
            // Set it to null first so that GC can collect it if memory
            // runs tight when initializing a new BlockInputStream.
            blockDecoder = null;
            blockDecoder = new BlockInputStream(
                    in, check, verifyCheck, memoryLimit,
                    curBlockInfo.unpaddedSize, curBlockInfo.uncompressedSize);
        } catch (MemoryLimitException e) {
            // BlockInputStream doesn't know how much memory we had
            // already needed so we need to recreate the exception.
            assert memoryLimit >= 0;
            throw new MemoryLimitException(
                    e.getMemoryNeeded() + indexMemoryUsage,
                    memoryLimit + indexMemoryUsage);
        } catch (IndexIndicatorException e) {
            // It cannot be Index so the file must be corrupt.
            throw new CorruptedInputException();
        }
    }
}
