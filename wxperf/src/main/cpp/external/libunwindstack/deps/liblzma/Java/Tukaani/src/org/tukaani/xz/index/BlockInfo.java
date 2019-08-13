/*
 * BlockInfo
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.index;

import org.tukaani.xz.common.StreamFlags;

public class BlockInfo {
    public int blockNumber = -1;
    public long compressedOffset = -1;
    public long uncompressedOffset = -1;
    public long unpaddedSize = -1;
    public long uncompressedSize = -1;

    IndexDecoder index;

    public BlockInfo(IndexDecoder indexOfFirstStream) {
        index = indexOfFirstStream;
    }

    public int getCheckType() {
        return index.getStreamFlags().checkType;
    }

    public boolean hasNext() {
        return index.hasRecord(blockNumber + 1);
    }

    public void setNext() {
        index.setBlockInfo(this, blockNumber + 1);
    }
}
