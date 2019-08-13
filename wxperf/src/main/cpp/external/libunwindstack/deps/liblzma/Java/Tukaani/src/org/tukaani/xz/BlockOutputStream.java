/*
 * BlockOutputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import org.tukaani.xz.common.EncoderUtil;
import org.tukaani.xz.check.Check;

class BlockOutputStream extends FinishableOutputStream {
    private final OutputStream out;
    private final CountingOutputStream outCounted;
    private FinishableOutputStream filterChain;
    private final Check check;

    private final int headerSize;
    private final long compressedSizeLimit;
    private long uncompressedSize = 0;

    private final byte[] tempBuf = new byte[1];

    public BlockOutputStream(OutputStream out, FilterEncoder[] filters,
                             Check check) throws IOException {
        this.out = out;
        this.check = check;

        // Initialize the filter chain.
        outCounted = new CountingOutputStream(out);
        filterChain = outCounted;
        for (int i = filters.length - 1; i >= 0; --i)
            filterChain = filters[i].getOutputStream(filterChain);

        // Prepare to encode the Block Header field.
        ByteArrayOutputStream bufStream = new ByteArrayOutputStream();

        // Write a dummy Block Header Size field. The real value is written
        // once everything else except CRC32 has been written.
        bufStream.write(0x00);

        // Write Block Flags. Storing Compressed Size or Uncompressed Size
        // isn't supported for now.
        bufStream.write(filters.length - 1);

        // List of Filter Flags
        for (int i = 0; i < filters.length; ++i) {
            EncoderUtil.encodeVLI(bufStream, filters[i].getFilterID());
            byte[] filterProps = filters[i].getFilterProps();
            EncoderUtil.encodeVLI(bufStream, filterProps.length);
            bufStream.write(filterProps);
        }

        // Header Padding
        while ((bufStream.size() & 3) != 0)
            bufStream.write(0x00);

        byte[] buf = bufStream.toByteArray();

        // Total size of the Block Header: Take the size of the CRC32 field
        // into account.
        headerSize = buf.length + 4;

        // This is just a sanity check.
        if (headerSize > EncoderUtil.BLOCK_HEADER_SIZE_MAX)
            throw new UnsupportedOptionsException();

        // Block Header Size
        buf[0] = (byte)(buf.length / 4);

        // Write the Block Header field to the output stream.
        out.write(buf);
        EncoderUtil.writeCRC32(out, buf);

        // Calculate the maximum allowed size of the Compressed Data field.
        // It is hard to exceed it so this is mostly to be pedantic.
        compressedSizeLimit = (EncoderUtil.VLI_MAX & ~3)
                              - headerSize - check.getSize();
    }

    public void write(int b) throws IOException {
        tempBuf[0] = (byte)b;
        write(tempBuf, 0, 1);
    }

    public void write(byte[] buf, int off, int len) throws IOException {
        filterChain.write(buf, off, len);
        check.update(buf, off, len);
        uncompressedSize += len;
        validate();
    }

    public void flush() throws IOException {
        filterChain.flush();
        validate();
    }

    public void finish() throws IOException {
        // Finish the Compressed Data field.
        filterChain.finish();
        validate();

        // Block Padding
        for (long i = outCounted.getSize(); (i & 3) != 0; ++i)
            out.write(0x00);

        // Check
        out.write(check.finish());
    }

    private void validate() throws IOException {
        long compressedSize = outCounted.getSize();

        // It is very hard to trigger this exception.
        // This is just to be pedantic.
        if (compressedSize < 0 || compressedSize > compressedSizeLimit
                || uncompressedSize < 0)
            throw new XZIOException("XZ Stream has grown too big");
    }

    public long getUnpaddedSize() {
        return headerSize + outCounted.getSize() + check.getSize();
    }

    public long getUncompressedSize() {
        return uncompressedSize;
    }
}
