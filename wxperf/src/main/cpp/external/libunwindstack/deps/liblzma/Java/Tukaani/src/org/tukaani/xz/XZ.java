/*
 * XZ
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

/**
 * XZ constants.
 */
public class XZ {
    /**
     * XZ Header Magic Bytes begin a XZ file.
     * This can be useful to detect XZ compressed data.
     */
    public static final byte[] HEADER_MAGIC = {
            (byte)0xFD, '7', 'z', 'X', 'Z', '\0' };

    /**
     * XZ Footer Magic Bytes are the last bytes of a XZ Stream.
     */
    public static final byte[] FOOTER_MAGIC = { 'Y', 'Z' };

    /**
     * Integrity check ID indicating that no integrity check is calculated.
     * <p>
     * Omitting the integrity check is strongly discouraged except when
     * the integrity of the data will be verified by other means anyway,
     * and calculating the check twice would be useless.
     */
    public static final int CHECK_NONE = 0;

    /**
     * Integrity check ID for CRC32.
     */
    public static final int CHECK_CRC32 = 1;

    /**
     * Integrity check ID for CRC64.
     */
    public static final int CHECK_CRC64 = 4;

    /**
     * Integrity check ID for SHA-256.
     */
    public static final int CHECK_SHA256 = 10;

    private XZ() {}
}
