/*
 * IndexHash
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.index;

import java.io.InputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.zip.CheckedInputStream;
import org.tukaani.xz.common.DecoderUtil;
import org.tukaani.xz.XZIOException;
import org.tukaani.xz.CorruptedInputException;

public class IndexHash extends IndexBase {
    private org.tukaani.xz.check.Check hash;

    public IndexHash() {
        super(new CorruptedInputException());

        try {
            hash = new org.tukaani.xz.check.SHA256();
        } catch (java.security.NoSuchAlgorithmException e) {
            hash = new org.tukaani.xz.check.CRC32();
        }
    }

    public void add(long unpaddedSize, long uncompressedSize)
            throws XZIOException {
        super.add(unpaddedSize, uncompressedSize);

        ByteBuffer buf = ByteBuffer.allocate(2 * 8);
        buf.putLong(unpaddedSize);
        buf.putLong(uncompressedSize);
        hash.update(buf.array());
    }

    public void validate(InputStream in) throws IOException {
        // Index Indicator (0x00) has already been read by BlockInputStream
        // so add 0x00 to the CRC32 here.
        java.util.zip.CRC32 crc32 = new java.util.zip.CRC32();
        crc32.update('\0');
        CheckedInputStream inChecked = new CheckedInputStream(in, crc32);

        // Get and validate the Number of Records field.
        long storedRecordCount = DecoderUtil.decodeVLI(inChecked);
        if (storedRecordCount != recordCount)
            throw new CorruptedInputException("XZ Index is corrupt");

        // Decode and hash the Index field and compare it to
        // the hash value calculated from the decoded Blocks.
        IndexHash stored = new IndexHash();
        for (long i = 0; i < recordCount; ++i) {
            long unpaddedSize = DecoderUtil.decodeVLI(inChecked);
            long uncompressedSize = DecoderUtil.decodeVLI(inChecked);

            try {
                stored.add(unpaddedSize, uncompressedSize);
            } catch (XZIOException e) {
                throw new CorruptedInputException("XZ Index is corrupt");
            }

            if (stored.blocksSum > blocksSum
                    || stored.uncompressedSum > uncompressedSum
                    || stored.indexListSize > indexListSize)
                throw new CorruptedInputException("XZ Index is corrupt");
        }

        if (stored.blocksSum != blocksSum
                || stored.uncompressedSum != uncompressedSum
                || stored.indexListSize != indexListSize
                || !Arrays.equals(stored.hash.finish(), hash.finish()))
            throw new CorruptedInputException("XZ Index is corrupt");

        // Index Padding
        DataInputStream inData = new DataInputStream(inChecked);
        for (int i = getIndexPaddingSize(); i > 0; --i)
            if (inData.readUnsignedByte() != 0x00)
                throw new CorruptedInputException("XZ Index is corrupt");

        // CRC32
        long value = crc32.getValue();
        for (int i = 0; i < 4; ++i)
            if (((value >>> (i * 8)) & 0xFF) != inData.readUnsignedByte())
                throw new CorruptedInputException("XZ Index is corrupt");
    }
}
