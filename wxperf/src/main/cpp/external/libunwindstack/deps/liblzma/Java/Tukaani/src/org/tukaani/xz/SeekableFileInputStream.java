/*
 * SeekableFileInputStream
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.io.FileNotFoundException;

/**
 * Wraps a {@link RandomAccessFile RandomAccessFile}
 * in a SeekableInputStream.
 */
public class SeekableFileInputStream extends SeekableInputStream {
    /**
     * The RandomAccessFile that has been wrapped
     * into a SeekableFileInputStream.
     */
    protected RandomAccessFile randomAccessFile;

    /**
     * Creates a new seekable input stream that reads from the specified file.
     */
    public SeekableFileInputStream(File file) throws FileNotFoundException {
        randomAccessFile = new RandomAccessFile(file, "r");
    }

    /**
     * Creates a new seekable input stream that reads from a file with
     * the specified name.
     */
    public SeekableFileInputStream(String name) throws FileNotFoundException {
        randomAccessFile = new RandomAccessFile(name, "r");
    }

    /**
     * Creates a new seekable input stream from an existing
     * <code>RandomAccessFile</code> object.
     */
    public SeekableFileInputStream(RandomAccessFile randomAccessFile) {
        this.randomAccessFile = randomAccessFile;
    }

    /**
     * Calls {@link RandomAccessFile#read() randomAccessFile.read()}.
     */
    public int read() throws IOException {
        return randomAccessFile.read();
    }

    /**
     * Calls {@link RandomAccessFile#read(byte[]) randomAccessFile.read(buf)}.
     */
    public int read(byte[] buf) throws IOException {
        return randomAccessFile.read(buf);
    }

    /**
     * Calls
     * {@link RandomAccessFile#read(byte[],int,int)
     *        randomAccessFile.read(buf, off, len)}.
     */
    public int read(byte[] buf, int off, int len) throws IOException {
        return randomAccessFile.read(buf, off, len);
    }

    /**
     * Calls {@link RandomAccessFile#close() randomAccessFile.close()}.
     */
    public void close() throws IOException {
        randomAccessFile.close();
    }

    /**
     * Calls {@link RandomAccessFile#length() randomAccessFile.length()}.
     */
    public long length() throws IOException {
        return randomAccessFile.length();
    }

    /**
     * Calls {@link RandomAccessFile#getFilePointer()
                    randomAccessFile.getFilePointer()}.
     */
    public long position() throws IOException {
        return randomAccessFile.getFilePointer();
    }

    /**
     * Calls {@link RandomAccessFile#seek(long) randomAccessFile.seek(long)}.
     */
    public void seek(long pos) throws IOException {
        randomAccessFile.seek(pos);
    }
}
