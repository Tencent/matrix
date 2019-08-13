/*
 * CountingInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.FilterInputStream;
import java.io.InputStream;
import java.io.IOException;

/**
 * Counts the number of bytes read from an input stream.
 */
class CountingInputStream extends FilterInputStream {
    private long size = 0;

    public CountingInputStream(InputStream in) {
        super(in);
    }

    public int read() throws IOException {
        int ret = in.read();
        if (ret != -1 && size >= 0)
            ++size;

        return ret;
    }

    public int read(byte[] b, int off, int len) throws IOException {
        int ret = in.read(b, off, len);
        if (ret > 0 && size >= 0)
            size += ret;

        return ret;
    }

    public long getSize() {
        return size;
    }
}
