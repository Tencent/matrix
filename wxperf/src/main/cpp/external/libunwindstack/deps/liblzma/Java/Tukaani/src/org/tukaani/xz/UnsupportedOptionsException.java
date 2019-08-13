/*
 * UnsupportedOptionsException
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

/**
 * Thrown when compression options not supported by this implementation
 * are detected. Some other implementation might support those options.
 */
public class UnsupportedOptionsException extends XZIOException {
    private static final long serialVersionUID = 3L;

    /**
     * Creates a new UnsupportedOptionsException with null
     * as its error detail message.
     */
    public UnsupportedOptionsException() {}

    /**
     * Creates a new UnsupportedOptionsException with the given
     * error detail message.
     *
     * @param       s           error detail message
     */
    public UnsupportedOptionsException(String s) {
        super(s);
    }
}
