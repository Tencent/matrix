/*
 * IndexDecoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.index;

import java.io.IOException;
import java.io.EOFException;
import java.util.zip.CheckedInputStream;
import org.tukaani.xz.common.DecoderUtil;
import org.tukaani.xz.common.StreamFlags;
import org.tukaani.xz.SeekableInputStream;
import org.tukaani.xz.CorruptedInputException;
import org.tukaani.xz.MemoryLimitException;
import org.tukaani.xz.UnsupportedOptionsException;

public class IndexDecoder extends IndexBase {
    private final StreamFlags streamFlags;
    private final long streamPadding;
    private final int memoryUsage;

    // Unpadded Size and Uncompressed Size fields
    private final long[] unpadded;
    private final long[] uncompressed;

    // Uncompressed size of the largest Block. It is used by
    // SeekableXZInputStream to find out the largest Block of the .xz file.
    private long largestBlockSize = 0;

    // Offsets relative to the beginning of the .xz file. These are all zero
    // for the first Stream in the file.
    private int recordOffset = 0;
    private long compressedOffset = 0;
    private long uncompressedOffset = 0;

    public IndexDecoder(SeekableInputStream in, StreamFlags streamFooterFlags,
                        long streamPadding, int memoryLimit)
            throws IOException {
        super(new CorruptedInputException("XZ Index is corrupt"));
        this.streamFlags = streamFooterFlags;
        this.streamPadding = streamPadding;

        // If endPos is exceeded before the CRC32 field has been decoded,
        // the Index is corrupt.
        long endPos = in.position() + streamFooterFlags.backwardSize - 4;

        java.util.zip.CRC32 crc32 = new java.util.zip.CRC32();
        CheckedInputStream inChecked = new CheckedInputStream(in, crc32);

        // Index Indicator
        if (inChecked.read() != 0x00)
            throw new CorruptedInputException("XZ Index is corrupt");

        try {
            // Number of Records
            long count = DecoderUtil.decodeVLI(inChecked);

            // Catch Record counts that obviously too high to be valid.
            // This test isn't exact because it ignores Index Indicator,
            // Number of Records, and CRC32 fields, but this is good enough
            // to catch the most obvious problems.
            if (count >= streamFooterFlags.backwardSize / 2)
                throw new CorruptedInputException("XZ Index is corrupt");

            // If the Record count doesn't fit into an int, we cannot
            // allocate the arrays to hold the Records.
            if (count > Integer.MAX_VALUE)
                throw new UnsupportedOptionsException("XZ Index has over "
                        + Integer.MAX_VALUE + " Records");

            // Calculate approximate memory requirements and check the
            // memory usage limit.
            memoryUsage = 1 + (int)((16L * count + 1023) / 1024);
            if (memoryLimit >= 0 && memoryUsage > memoryLimit)
                throw new MemoryLimitException(memoryUsage, memoryLimit);

            // Allocate the arrays for the Records.
            unpadded = new long[(int)count];
            uncompressed = new long[(int)count];
            int record = 0;

            // Decode the Records.
            for (int i = (int)count; i > 0; --i) {
                // Get the next Record.
                long unpaddedSize = DecoderUtil.decodeVLI(inChecked);
                long uncompressedSize = DecoderUtil.decodeVLI(inChecked);

                // Check that the input position stays sane. Since this is
                // checked only once per loop iteration instead of for
                // every input byte read, it's still possible that
                // EOFException gets thrown with corrupt input.
                if (in.position() > endPos)
                    throw new CorruptedInputException("XZ Index is corrupt");

                // Add the new Record.
                unpadded[record] = blocksSum + unpaddedSize;
                uncompressed[record] = uncompressedSum + uncompressedSize;
                ++record;
                super.add(unpaddedSize, uncompressedSize);
                assert record == recordCount;

                // Remember the uncompressed size of the largest Block.
                if (largestBlockSize < uncompressedSize)
                    largestBlockSize = uncompressedSize;
            }
        } catch (EOFException e) {
            // EOFException is caught just in case a corrupt input causes
            // DecoderUtil.decodeVLI to read too much at once.
            throw new CorruptedInputException("XZ Index is corrupt");
        }

        // Validate that the size of the Index field matches
        // Backward Size.
        int indexPaddingSize = getIndexPaddingSize();
        if (in.position() + indexPaddingSize != endPos)
            throw new CorruptedInputException("XZ Index is corrupt");

        // Index Padding
        while (indexPaddingSize-- > 0)
            if (inChecked.read() != 0x00)
                throw new CorruptedInputException("XZ Index is corrupt");

        // CRC32
        long value = crc32.getValue();
        for (int i = 0; i < 4; ++i)
            if (((value >>> (i * 8)) & 0xFF) != in.read())
                throw new CorruptedInputException("XZ Index is corrupt");
    }

    public void setOffsets(IndexDecoder prev) {
        // NOTE: SeekableXZInputStream checks that the total number of Blocks
        // in concatenated Streams fits into an int.
        recordOffset = prev.recordOffset + (int)prev.recordCount;
        compressedOffset = prev.compressedOffset
                           + prev.getStreamSize() + prev.streamPadding;
        assert (compressedOffset & 3) == 0;
        uncompressedOffset = prev.uncompressedOffset + prev.uncompressedSum;
    }

    public int getMemoryUsage() {
        return memoryUsage;
    }

    public StreamFlags getStreamFlags() {
        return streamFlags;
    }

    public int getRecordCount() {
        // It was already checked in the constructor that it fits into an int.
        // Otherwise we couldn't have allocated the arrays.
        return (int)recordCount;
    }

    public long getUncompressedSize() {
        return uncompressedSum;
    }

    public long getLargestBlockSize() {
        return largestBlockSize;
    }

    public boolean hasUncompressedOffset(long pos) {
        return pos >= uncompressedOffset
               && pos < uncompressedOffset + uncompressedSum;
    }

    public boolean hasRecord(int blockNumber) {
        return blockNumber >= recordOffset
               && blockNumber < recordOffset + recordCount;
    }

    public void locateBlock(BlockInfo info, long target) {
        assert target >= uncompressedOffset;
        target -= uncompressedOffset;
        assert target < uncompressedSum;

        int left = 0;
        int right = unpadded.length - 1;

        while (left < right) {
            int i = left + (right - left) / 2;

            if (uncompressed[i] <= target)
                left = i + 1;
            else
                right = i;
        }

        setBlockInfo(info, recordOffset + left);
    }

    public void setBlockInfo(BlockInfo info, int blockNumber) {
        // The caller has checked that the given Block number is inside
        // this Index.
        assert blockNumber >= recordOffset;
        assert blockNumber - recordOffset < recordCount;

        info.index = this;
        info.blockNumber = blockNumber;

        int pos = blockNumber - recordOffset;

        if (pos == 0) {
            info.compressedOffset = 0;
            info.uncompressedOffset = 0;
        } else {
            info.compressedOffset = (unpadded[pos - 1] + 3) & ~3;
            info.uncompressedOffset = uncompressed[pos - 1];
        }

        info.unpaddedSize = unpadded[pos] - info.compressedOffset;
        info.uncompressedSize = uncompressed[pos] - info.uncompressedOffset;

        info.compressedOffset += compressedOffset
                                 + DecoderUtil.STREAM_HEADER_SIZE;
        info.uncompressedOffset += uncompressedOffset;
    }
}
