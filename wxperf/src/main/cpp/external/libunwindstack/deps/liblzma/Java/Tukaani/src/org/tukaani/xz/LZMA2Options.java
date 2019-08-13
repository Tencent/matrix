/*
 * LZMA2Options
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz;

import java.io.InputStream;
import java.io.IOException;
import org.tukaani.xz.lz.LZEncoder;
import org.tukaani.xz.lzma.LZMAEncoder;

/**
 * LZMA2 compression options.
 * <p>
 * While this allows setting the LZMA2 compression options in detail,
 * often you only need <code>LZMA2Options()</code> or
 * <code>LZMA2Options(int)</code>.
 */
public class LZMA2Options extends FilterOptions {
    /**
     * Minimum valid compression preset level is 0.
     */
    public static final int PRESET_MIN = 0;

    /**
     * Maximum valid compression preset level is 9.
     */
    public static final int PRESET_MAX = 9;

    /**
     * Default compression preset level is 6.
     */
    public static final int PRESET_DEFAULT = 6;

    /**
     * Minimum dictionary size is 4 KiB.
     */
    public static final int DICT_SIZE_MIN = 4096;

    /**
     * Maximum dictionary size for compression is 768 MiB.
     * <p>
     * The decompressor supports bigger dictionaries, up to almost 2 GiB.
     * With HC4 the encoder would support dictionaries bigger than 768 MiB.
     * The 768 MiB limit comes from the current implementation of BT4 where
     * we would otherwise hit the limits of signed ints in array indexing.
     * <p>
     * If you really need bigger dictionary for decompression,
     * use {@link LZMA2InputStream} directly.
     */
    public static final int DICT_SIZE_MAX = 768 << 20;

    /**
     * The default dictionary size is 8 MiB.
     */
    public static final int DICT_SIZE_DEFAULT = 8 << 20;

    /**
     * Maximum value for lc + lp is 4.
     */
    public static final int LC_LP_MAX = 4;

    /**
     * The default number of literal context bits is 3.
     */
    public static final int LC_DEFAULT = 3;

    /**
     * The default number of literal position bits is 0.
     */
    public static final int LP_DEFAULT = 0;

    /**
     * Maximum value for pb is 4.
     */
    public static final int PB_MAX = 4;

    /**
     * The default number of position bits is 2.
     */
    public static final int PB_DEFAULT = 2;

    /**
     * Compression mode: uncompressed.
     * The data is wrapped into a LZMA2 stream without compression.
     */
    public static final int MODE_UNCOMPRESSED = 0;

    /**
     * Compression mode: fast.
     * This is usually combined with a hash chain match finder.
     */
    public static final int MODE_FAST = LZMAEncoder.MODE_FAST;

    /**
     * Compression mode: normal.
     * This is usually combined with a binary tree match finder.
     */
    public static final int MODE_NORMAL = LZMAEncoder.MODE_NORMAL;

    /**
     * Minimum value for <code>niceLen</code> is 8.
     */
    public static final int NICE_LEN_MIN = 8;

    /**
     * Maximum value for <code>niceLen</code> is 273.
     */
    public static final int NICE_LEN_MAX = 273;

    /**
     * Match finder: Hash Chain 2-3-4
     */
    public static final int MF_HC4 = LZEncoder.MF_HC4;

    /**
     * Match finder: Binary tree 2-3-4
     */
    public static final int MF_BT4 = LZEncoder.MF_BT4;

    private static final int[] presetToDictSize = {
            1 << 18, 1 << 20, 1 << 21, 1 << 22, 1 << 22,
            1 << 23, 1 << 23, 1 << 24, 1 << 25, 1 << 26 };

    private static final int[] presetToDepthLimit = { 4, 8, 24, 48 };

    private int dictSize;
    private byte[] presetDict = null;
    private int lc;
    private int lp;
    private int pb;
    private int mode;
    private int niceLen;
    private int mf;
    private int depthLimit;

    /**
     * Creates new LZMA2 options and sets them to the default values.
     * This is equivalent to <code>LZMA2Options(PRESET_DEFAULT)</code>.
     */
    public LZMA2Options() {
        try {
            setPreset(PRESET_DEFAULT);
        } catch (UnsupportedOptionsException e) {
            assert false;
            throw new RuntimeException();
        }
    }

    /**
     * Creates new LZMA2 options and sets them to the given preset.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>preset</code> is not supported
     */
    public LZMA2Options(int preset) throws UnsupportedOptionsException {
        setPreset(preset);
    }

    /**
     * Creates new LZMA2 options and sets them to the given custom values.
     *
     * @throws      UnsupportedOptionsException
     *                          unsupported options were specified
     */
    public LZMA2Options(int dictSize, int lc, int lp, int pb, int mode,
                        int niceLen, int mf, int depthLimit)
            throws UnsupportedOptionsException {
        setDictSize(dictSize);
        setLcLp(lc, lp);
        setPb(pb);
        setMode(mode);
        setNiceLen(niceLen);
        setMatchFinder(mf);
        setDepthLimit(depthLimit);
    }

    /**
     * Sets the compression options to the given preset.
     * <p>
     * The presets 0-3 are fast presets with medium compression.
     * The presets 4-6 are fairly slow presets with high compression.
     * The default preset (<code>PRESET_DEFAULT</code>) is 6.
     * <p>
     * The presets 7-9 are like the preset 6 but use bigger dictionaries
     * and have higher compressor and decompressor memory requirements.
     * Unless the uncompressed size of the file exceeds 8&nbsp;MiB,
     * 16&nbsp;MiB, or 32&nbsp;MiB, it is waste of memory to use the
     * presets 7, 8, or 9, respectively.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>preset</code> is not supported
     */
    public void setPreset(int preset) throws UnsupportedOptionsException {
        if (preset < 0 || preset > 9)
            throw new UnsupportedOptionsException(
                    "Unsupported preset: " + preset);

        lc = LC_DEFAULT;
        lp = LP_DEFAULT;
        pb = PB_DEFAULT;
        dictSize = presetToDictSize[preset];

        if (preset <= 3) {
            mode = MODE_FAST;
            mf = MF_HC4;
            niceLen = preset <= 1 ? 128 : NICE_LEN_MAX;
            depthLimit = presetToDepthLimit[preset];
        } else {
            mode = MODE_NORMAL;
            mf = MF_BT4;
            niceLen = (preset == 4) ? 16 : (preset == 5) ? 32 : 64;
            depthLimit = 0;
        }
    }

    /**
     * Sets the dictionary size in bytes.
     * <p>
     * The dictionary (or history buffer) holds the most recently seen
     * uncompressed data. Bigger dictionary usually means better compression.
     * However, using a dictioanary bigger than the size of the uncompressed
     * data is waste of memory.
     * <p>
     * Any value in the range [DICT_SIZE_MIN, DICT_SIZE_MAX] is valid,
     * but sizes of 2^n and 2^n&nbsp;+&nbsp;2^(n-1) bytes are somewhat
     * recommended.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>dictSize</code> is not supported
     */
    public void setDictSize(int dictSize) throws UnsupportedOptionsException {
        if (dictSize < DICT_SIZE_MIN)
            throw new UnsupportedOptionsException(
                    "LZMA2 dictionary size must be at least 4 KiB: "
                    + dictSize + " B");

        if (dictSize > DICT_SIZE_MAX)
            throw new UnsupportedOptionsException(
                    "LZMA2 dictionary size must not exceed "
                    + (DICT_SIZE_MAX >> 20) + " MiB: " + dictSize + " B");

        this.dictSize = dictSize;
    }

    /**
     * Gets the dictionary size in bytes.
     */
    public int getDictSize() {
        return dictSize;
    }

    /**
     * Sets a preset dictionary. Use null to disable the use of
     * a preset dictionary. By default there is no preset dictionary.
     * <p>
     * <b>The .xz format doesn't support a preset dictionary for now.
     * Do not set a preset dictionary unless you use raw LZMA2.</b>
     * <p>
     * Preset dictionary can be useful when compressing many similar,
     * relatively small chunks of data independently from each other.
     * A preset dictionary should contain typical strings that occur in
     * the files being compressed. The most probable strings should be
     * near the end of the preset dictionary. The preset dictionary used
     * for compression is also needed for decompression.
     */
    public void setPresetDict(byte[] presetDict) {
        this.presetDict = presetDict;
    }

    /**
     * Gets the preset dictionary.
     */
    public byte[] getPresetDict() {
        return presetDict;
    }

    /**
     * Sets the number of literal context bits and literal position bits.
     * <p>
     * The sum of <code>lc</code> and <code>lp</code> is limited to 4.
     * Trying to exceed it will throw an exception. This function lets
     * you change both at the same time.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>lc</code> and <code>lp</code>
     *                          are invalid
     */
    public void setLcLp(int lc, int lp) throws UnsupportedOptionsException {
        if (lc < 0 || lp < 0 || lc > LC_LP_MAX || lp > LC_LP_MAX
                || lc + lp > LC_LP_MAX)
            throw new UnsupportedOptionsException(
                    "lc + lp must not exceed " + LC_LP_MAX + ": "
                    + lc + " + " + lp);

        this.lc = lc;
        this.lp = lp;
    }

    /**
     * Sets the number of literal context bits.
     * <p>
     * All bytes that cannot be encoded as matches are encoded as literals.
     * That is, literals are simply 8-bit bytes that are encoded one at
     * a time.
     * <p>
     * The literal coding makes an assumption that the highest <code>lc</code>
     * bits of the previous uncompressed byte correlate with the next byte.
     * For example, in typical English text, an upper-case letter is often
     * followed by a lower-case letter, and a lower-case letter is usually
     * followed by another lower-case letter. In the US-ASCII character set,
     * the highest three bits are 010 for upper-case letters and 011 for
     * lower-case letters. When <code>lc</code> is at least 3, the literal
     * coding can take advantage of this property in the  uncompressed data.
     * <p>
     * The default value (3) is usually good. If you want maximum compression,
     * try <code>setLc(4)</code>. Sometimes it helps a little, and sometimes it
     * makes compression worse. If it makes it worse, test for example
     * <code>setLc(2)</code> too.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>lc</code> is invalid, or the sum
     *                          of <code>lc</code> and <code>lp</code>
     *                          exceed LC_LP_MAX
     */
    public void setLc(int lc) throws UnsupportedOptionsException {
        setLcLp(lc, lp);
    }

    /**
     * Sets the number of literal position bits.
     * <p>
     * This affets what kind of alignment in the uncompressed data is
     * assumed when encoding literals. See {@link #setPb(int) setPb} for
     * more information about alignment.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>lp</code> is invalid, or the sum
     *                          of <code>lc</code> and <code>lp</code>
     *                          exceed LC_LP_MAX
     */
    public void setLp(int lp) throws UnsupportedOptionsException {
        setLcLp(lc, lp);
    }

    /**
     * Gets the number of literal context bits.
     */
    public int getLc() {
        return lc;
    }

    /**
     * Gets the number of literal position bits.
     */
    public int getLp() {
        return lp;
    }

    /**
     * Sets the number of position bits.
     * <p>
     * This affects what kind of alignment in the uncompressed data is
     * assumed in general. The default (2) means four-byte alignment
     * (2^<code>pb</code> = 2^2 = 4), which is often a good choice when
     * there's no better guess.
     * <p>
     * When the alignment is known, setting the number of position bits
     * accordingly may reduce the file size a little. For example with text
     * files having one-byte alignment (US-ASCII, ISO-8859-*, UTF-8), using
     * <code>setPb(0)</code> can improve compression slightly. For UTF-16
     * text, <code>setPb(1)</code> is a good choice. If the alignment is
     * an odd number like 3 bytes, <code>setPb(0)</code> might be the best
     * choice.
     * <p>
     * Even though the assumed alignment can be adjusted with
     * <code>setPb</code> and <code>setLp</code>, LZMA2 still slightly favors
     * 16-byte alignment. It might be worth taking into account when designing
     * file formats that are likely to be often compressed with LZMA2.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>pb</code> is invalid
     */
    public void setPb(int pb) throws UnsupportedOptionsException {
        if (pb < 0 || pb > PB_MAX)
            throw new UnsupportedOptionsException(
                    "pb must not exceed " + PB_MAX + ": " + pb);

        this.pb = pb;
    }

    /**
     * Gets the number of position bits.
     */
    public int getPb() {
        return pb;
    }

    /**
     * Sets the compression mode.
     * <p>
     * This specifies the method to analyze the data produced by
     * a match finder. The default is <code>MODE_FAST</code> for presets
     * 0-3 and <code>MODE_NORMAL</code> for presets 4-9.
     * <p>
     * Usually <code>MODE_FAST</code> is used with Hash Chain match finders
     * and <code>MODE_NORMAL</code> with Binary Tree match finders. This is
     * also what the presets do.
     * <p>
     * The special mode <code>MODE_UNCOMPRESSED</code> doesn't try to
     * compress the data at all (and doesn't use a match finder) and will
     * simply wrap it in uncompressed LZMA2 chunks.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>mode</code> is not supported
     */
    public void setMode(int mode) throws UnsupportedOptionsException {
        if (mode < MODE_UNCOMPRESSED || mode > MODE_NORMAL)
            throw new UnsupportedOptionsException(
                    "Unsupported compression mode: " + mode);

        this.mode = mode;
    }

    /**
     * Gets the compression mode.
     */
    public int getMode() {
        return mode;
    }

    /**
     * Sets the nice length of matches.
     * Once a match of at least <code>niceLen</code> bytes is found,
     * the algorithm stops looking for better matches. Higher values tend
     * to give better compression at the expense of speed. The default
     * depends on the preset.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>niceLen</code> is invalid
     */
    public void setNiceLen(int niceLen) throws UnsupportedOptionsException {
        if (niceLen < NICE_LEN_MIN)
            throw new UnsupportedOptionsException(
                    "Minimum nice length of matches is "
                    + NICE_LEN_MIN + " bytes: " + niceLen);

        if (niceLen > NICE_LEN_MAX)
            throw new UnsupportedOptionsException(
                    "Maximum nice length of matches is " + NICE_LEN_MAX
                    + ": " + niceLen);

        this.niceLen = niceLen;
    }

    /**
     * Gets the nice length of matches.
     */
    public int getNiceLen() {
        return niceLen;
    }

    /**
     * Sets the match finder type.
     * <p>
     * Match finder has a major effect on compression speed, memory usage,
     * and compression ratio. Usually Hash Chain match finders are faster
     * than Binary Tree match finders. The default depends on the preset:
     * 0-3 use <code>MF_HC4</code> and 4-9 use <code>MF_BT4</code>.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>mf</code> is not supported
     */
    public void setMatchFinder(int mf) throws UnsupportedOptionsException {
        if (mf != MF_HC4 && mf != MF_BT4)
            throw new UnsupportedOptionsException(
                    "Unsupported match finder: " + mf);

        this.mf = mf;
    }

    /**
     * Gets the match finder type.
     */
    public int getMatchFinder() {
        return mf;
    }

    /**
     * Sets the match finder search depth limit.
     * <p>
     * The default is a special value of <code>0</code> which indicates that
     * the depth limit should be automatically calculated by the selected
     * match finder from the nice length of matches.
     * <p>
     * Reasonable depth limit for Hash Chain match finders is 4-100 and
     * 16-1000 for Binary Tree match finders. Using very high values can
     * make the compressor extremely slow with some files. Avoid settings
     * higher than 1000 unless you are prepared to interrupt the compression
     * in case it is taking far too long.
     *
     * @throws      UnsupportedOptionsException
     *                          <code>depthLimit</code> is invalid
     */
    public void setDepthLimit(int depthLimit)
            throws UnsupportedOptionsException {
        if (depthLimit < 0)
            throw new UnsupportedOptionsException(
                    "Depth limit cannot be negative: " + depthLimit);

        this.depthLimit = depthLimit;
    }

    /**
     * Gets the match finder search depth limit.
     */
    public int getDepthLimit() {
        return depthLimit;
    }

    public int getEncoderMemoryUsage() {
        return (mode == MODE_UNCOMPRESSED)
               ? UncompressedLZMA2OutputStream.getMemoryUsage()
               : LZMA2OutputStream.getMemoryUsage(this);
    }

    public FinishableOutputStream getOutputStream(FinishableOutputStream out) {
        if (mode == MODE_UNCOMPRESSED)
            return new UncompressedLZMA2OutputStream(out);

        return new LZMA2OutputStream(out, this);
    }

    /**
     * Gets how much memory the LZMA2 decoder will need to decompress the data
     * that was encoded with these options and stored in a .xz file.
     * <p>
     * The returned value may bigger than the value returned by a direct call
     * to {@link LZMA2InputStream#getMemoryUsage(int)} if the dictionary size
     * is not 2^n or 2^n&nbsp;+&nbsp;2^(n-1) bytes. This is because the .xz
     * headers store the dictionary size in such a format and other values
     * are rounded up to the next such value. Such rounding is harmess except
     * it might waste some memory if an unsual dictionary size is used.
     * <p>
     * If you use raw LZMA2 streams and unusual dictioanary size, call
     * {@link LZMA2InputStream#getMemoryUsage} directly to get raw decoder
     * memory requirements.
     */
    public int getDecoderMemoryUsage() {
        // Round the dictionary size up to the next 2^n or 2^n + 2^(n-1).
        int d = dictSize - 1;
        d |= d >>> 2;
        d |= d >>> 3;
        d |= d >>> 4;
        d |= d >>> 8;
        d |= d >>> 16;
        return LZMA2InputStream.getMemoryUsage(d + 1);
    }

    public InputStream getInputStream(InputStream in) throws IOException {
        return new LZMA2InputStream(in, dictSize);
    }

    FilterEncoder getFilterEncoder() {
        return new LZMA2Encoder(this);
    }

    public Object clone() {
        try {
            return super.clone();
        } catch (CloneNotSupportedException e) {
            assert false;
            throw new RuntimeException();
        }
    }
}
