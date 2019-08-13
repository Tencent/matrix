/*
 * IndexBase
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.index;

import org.tukaani.xz.common.Util;
import org.tukaani.xz.XZIOException;

abstract class IndexBase {
    private final XZIOException invalidIndexException;
    long blocksSum = 0;
    long uncompressedSum = 0;
    long indexListSize = 0;
    long recordCount = 0;

    IndexBase(XZIOException invalidIndexException) {
        this.invalidIndexException = invalidIndexException;
    }

    private long getUnpaddedIndexSize() {
        // Index Indicator + Number of Records + List of Records + CRC32
        return 1 + Util.getVLISize(recordCount) + indexListSize + 4;
    }

    public long getIndexSize() {
        return (getUnpaddedIndexSize() + 3) & ~3;
    }

    public long getStreamSize() {
        return Util.STREAM_HEADER_SIZE + blocksSum + getIndexSize()
                + Util.STREAM_HEADER_SIZE;
    }

    int getIndexPaddingSize() {
        return (int)((4 - getUnpaddedIndexSize()) & 3);
    }

    void add(long unpaddedSize, long uncompressedSize) throws XZIOException {
        blocksSum += (unpaddedSize + 3) & ~3;
        uncompressedSum += uncompressedSize;
        indexListSize += Util.getVLISize(unpaddedSize)
                         + Util.getVLISize(uncompressedSize);
        ++recordCount;

        if (blocksSum < 0 || uncompressedSum < 0
                || getIndexSize() > Util.BACKWARD_SIZE_MAX
                || getStreamSize() < 0)
            throw invalidIndexException;
    }
}
