/*
 * XZFormatException
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

/**
 * Thrown when the input data is not in the XZ format.
 */
public class XZFormatException extends XZIOException {
    private static final long serialVersionUID = 3L;

    /**
     * Creates a new exception with the default error detail message.
     */
    public XZFormatException() {
        super("Input is not in the XZ format");
    }
}
