/*
 * LZMAEncoder
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lzma;

import org.tukaani.xz.lz.LZEncoder;
import org.tukaani.xz.lz.Matches;
import org.tukaani.xz.rangecoder.RangeEncoder;

public abstract class LZMAEncoder extends LZMACoder {
    public static final int MODE_FAST = 1;
    public static final int MODE_NORMAL = 2;

    /**
     * LZMA2 chunk is considered full when its uncompressed size exceeds
     * <code>LZMA2_UNCOMPRESSED_LIMIT</code>.
     * <p>
     * A compressed LZMA2 chunk can hold 2 MiB of uncompressed data.
     * A single LZMA symbol may indicate up to MATCH_LEN_MAX bytes
     * of data, so the LZMA2 chunk is considered full when there is
     * less space than MATCH_LEN_MAX bytes.
     */
    private static final int LZMA2_UNCOMPRESSED_LIMIT
            = (2 << 20) - MATCH_LEN_MAX;

    /**
     * LZMA2 chunk is considered full when its compressed size exceeds
     * <code>LZMA2_COMPRESSED_LIMIT</code>.
     * <p>
     * The maximum compressed size of a LZMA2 chunk is 64 KiB.
     * A single LZMA symbol might use 20 bytes of space even though
     * it usually takes just one byte or so. Two more bytes are needed
     * for LZMA2 uncompressed chunks (see LZMA2OutputStream.writeChunk).
     * Leave a little safety margin and use 26 bytes.
     */
    private static final int LZMA2_COMPRESSED_LIMIT = (64 << 10) - 26;

    private static final int DIST_PRICE_UPDATE_INTERVAL = FULL_DISTANCES;
    private static final int ALIGN_PRICE_UPDATE_INTERVAL = ALIGN_SIZE;

    private final RangeEncoder rc;
    final LZEncoder lz;
    final LiteralEncoder literalEncoder;
    final LengthEncoder matchLenEncoder;
    final LengthEncoder repLenEncoder;
    final int niceLen;

    private int distPriceCount = 0;
    private int alignPriceCount = 0;

    private final int distSlotPricesSize;
    private final int[][] distSlotPrices;
    private final int[][] fullDistPrices
            = new int[DIST_STATES][FULL_DISTANCES];
    private final int[] alignPrices = new int[ALIGN_SIZE];

    int back = 0;
    int readAhead = -1;
    private int uncompressedSize = 0;

    public static int getMemoryUsage(int mode, int dictSize,
                                     int extraSizeBefore, int mf) {
        int m = 80;

        switch (mode) {
            case MODE_FAST:
                m += LZMAEncoderFast.getMemoryUsage(
                        dictSize, extraSizeBefore, mf);
                break;

            case MODE_NORMAL:
                m += LZMAEncoderNormal.getMemoryUsage(
                        dictSize, extraSizeBefore, mf);
                break;

            default:
                throw new IllegalArgumentException();
        }

        return m;
    }

    public static LZMAEncoder getInstance(
                RangeEncoder rc, int lc, int lp, int pb, int mode,
                int dictSize, int extraSizeBefore,
                int niceLen, int mf, int depthLimit) {
        switch (mode) {
            case MODE_FAST:
                return new LZMAEncoderFast(rc, lc, lp, pb,
                                           dictSize, extraSizeBefore,
                                           niceLen, mf, depthLimit);

            case MODE_NORMAL:
                return new LZMAEncoderNormal(rc, lc, lp, pb,
                                             dictSize, extraSizeBefore,
                                             niceLen, mf, depthLimit);
        }

        throw new IllegalArgumentException();
    }

    /**
     * Gets an integer [0, 63] matching the highest two bits of an integer.
     * This is like bit scan reverse (BSR) on x86 except that this also
     * cares about the second highest bit.
     */
    public static int getDistSlot(int dist) {
        if (dist <= DIST_MODEL_START)
            return dist;

        int n = dist;
        int i = 31;

        if ((n & 0xFFFF0000) == 0) {
            n <<= 16;
            i = 15;
        }

        if ((n & 0xFF000000) == 0) {
            n <<= 8;
            i -= 8;
        }

        if ((n & 0xF0000000) == 0) {
            n <<= 4;
            i -= 4;
        }

        if ((n & 0xC0000000) == 0) {
            n <<= 2;
            i -= 2;
        }

        if ((n & 0x80000000) == 0)
            --i;

        return (i << 1) + ((dist >>> (i - 1)) & 1);
    }

    /**
     * Gets the next LZMA symbol.
     * <p>
     * There are three types of symbols: literal (a single byte),
     * repeated match, and normal match. The symbol is indicated
     * by the return value and by the variable <code>back</code>.
     * <p>
     * Literal: <code>back == -1</code> and return value is <code>1</code>.
     * The literal itself needs to be read from <code>lz</code> separately.
     * <p>
     * Repeated match: <code>back</code> is in the range [0, 3] and
     * the return value is the length of the repeated match.
     * <p>
     * Normal match: <code>back - REPS<code> (<code>back - 4</code>)
     * is the distance of the match and the return value is the length
     * of the match.
     */
    abstract int getNextSymbol();

    LZMAEncoder(RangeEncoder rc, LZEncoder lz,
                int lc, int lp, int pb, int dictSize, int niceLen) {
        super(pb);
        this.rc = rc;
        this.lz = lz;
        this.niceLen = niceLen;

        literalEncoder = new LiteralEncoder(lc, lp);
        matchLenEncoder = new LengthEncoder(pb, niceLen);
        repLenEncoder = new LengthEncoder(pb, niceLen);

        distSlotPricesSize = getDistSlot(dictSize - 1) + 1;
        distSlotPrices = new int[DIST_STATES][distSlotPricesSize];

        reset();
    }

    public LZEncoder getLZEncoder() {
        return lz;
    }

    public void reset() {
        super.reset();
        literalEncoder.reset();
        matchLenEncoder.reset();
        repLenEncoder.reset();
        distPriceCount = 0;
        alignPriceCount = 0;

        uncompressedSize += readAhead + 1;
        readAhead = -1;
    }

    public int getUncompressedSize() {
        return uncompressedSize;
    }

    public void resetUncompressedSize() {
        uncompressedSize = 0;
    }

    /**
     * Compresses for LZMA2.
     *
     * @return      true if the LZMA2 chunk became full, false otherwise
     */
    public boolean encodeForLZMA2() {
        if (!lz.isStarted() && !encodeInit())
            return false;

        while (uncompressedSize <= LZMA2_UNCOMPRESSED_LIMIT
                && rc.getPendingSize() <= LZMA2_COMPRESSED_LIMIT)
            if (!encodeSymbol())
                return false;

        return true;
    }

    private boolean encodeInit() {
        assert readAhead == -1;
        if (!lz.hasEnoughData(0))
            return false;

        // The first symbol must be a literal unless using
        // a preset dictionary. This code isn't run if using
        // a preset dictionary.
        skip(1);
        rc.encodeBit(isMatch[state.get()], 0, 0);
        literalEncoder.encodeInit();

        --readAhead;
        assert readAhead == -1;

        ++uncompressedSize;
        assert uncompressedSize == 1;

        return true;
    }

    private boolean encodeSymbol() {
        if (!lz.hasEnoughData(readAhead + 1))
            return false;

        int len = getNextSymbol();

        assert readAhead >= 0;
        int posState = (lz.getPos() - readAhead) & posMask;

        if (back == -1) {
            // Literal i.e. eight-bit byte
            assert len == 1;
            rc.encodeBit(isMatch[state.get()], posState, 0);
            literalEncoder.encode();
        } else {
            // Some type of match
            rc.encodeBit(isMatch[state.get()], posState, 1);
            if (back < REPS) {
                // Repeated match i.e. the same distance
                // has been used earlier.
                assert lz.getMatchLen(-readAhead, reps[back], len) == len;
                rc.encodeBit(isRep, state.get(), 1);
                encodeRepMatch(back, len, posState);
            } else {
                // Normal match
                assert lz.getMatchLen(-readAhead, back - REPS, len) == len;
                rc.encodeBit(isRep, state.get(), 0);
                encodeMatch(back - REPS, len, posState);
            }
        }

        readAhead -= len;
        uncompressedSize += len;

        return true;
    }

    private void encodeMatch(int dist, int len, int posState) {
        state.updateMatch();
        matchLenEncoder.encode(len, posState);

        int distSlot = getDistSlot(dist);
        rc.encodeBitTree(distSlots[getDistState(len)], distSlot);

        if (distSlot >= DIST_MODEL_START) {
            int footerBits = (distSlot >>> 1) - 1;
            int base = (2 | (distSlot & 1)) << footerBits;
            int distReduced = dist - base;

            if (distSlot < DIST_MODEL_END) {
                rc.encodeReverseBitTree(
                        distSpecial[distSlot - DIST_MODEL_START],
                        distReduced);
            } else {
                rc.encodeDirectBits(distReduced >>> ALIGN_BITS,
                                    footerBits - ALIGN_BITS);
                rc.encodeReverseBitTree(distAlign, distReduced & ALIGN_MASK);
                --alignPriceCount;
            }
        }

        reps[3] = reps[2];
        reps[2] = reps[1];
        reps[1] = reps[0];
        reps[0] = dist;

        --distPriceCount;
    }

    private void encodeRepMatch(int rep, int len, int posState) {
        if (rep == 0) {
            rc.encodeBit(isRep0, state.get(), 0);
            rc.encodeBit(isRep0Long[state.get()], posState, len == 1 ? 0 : 1);
        } else {
            int dist = reps[rep];
            rc.encodeBit(isRep0, state.get(), 1);

            if (rep == 1) {
                rc.encodeBit(isRep1, state.get(), 0);
            } else {
                rc.encodeBit(isRep1, state.get(), 1);
                rc.encodeBit(isRep2, state.get(), rep - 2);

                if (rep == 3)
                    reps[3] = reps[2];

                reps[2] = reps[1];
            }

            reps[1] = reps[0];
            reps[0] = dist;
        }

        if (len == 1) {
            state.updateShortRep();
        } else {
            repLenEncoder.encode(len, posState);
            state.updateLongRep();
        }
    }

    Matches getMatches() {
        ++readAhead;
        Matches matches = lz.getMatches();
        assert lz.verifyMatches(matches);
        return matches;
    }

    void skip(int len) {
        readAhead += len;
        lz.skip(len);
    }

    int getAnyMatchPrice(State state, int posState) {
        return RangeEncoder.getBitPrice(isMatch[state.get()][posState], 1);
    }

    int getNormalMatchPrice(int anyMatchPrice, State state) {
        return anyMatchPrice
               + RangeEncoder.getBitPrice(isRep[state.get()], 0);
    }

    int getAnyRepPrice(int anyMatchPrice, State state) {
        return anyMatchPrice
               + RangeEncoder.getBitPrice(isRep[state.get()], 1);
    }

    int getShortRepPrice(int anyRepPrice, State state, int posState) {
        return anyRepPrice
               + RangeEncoder.getBitPrice(isRep0[state.get()], 0)
               + RangeEncoder.getBitPrice(isRep0Long[state.get()][posState],
                                          0);
    }

    int getLongRepPrice(int anyRepPrice, int rep, State state, int posState) {
        int price = anyRepPrice;

        if (rep == 0) {
            price += RangeEncoder.getBitPrice(isRep0[state.get()], 0)
                     + RangeEncoder.getBitPrice(
                       isRep0Long[state.get()][posState], 1);
        } else {
            price += RangeEncoder.getBitPrice(isRep0[state.get()], 1);

            if (rep == 1)
                price += RangeEncoder.getBitPrice(isRep1[state.get()], 0);
            else
                price += RangeEncoder.getBitPrice(isRep1[state.get()], 1)
                         + RangeEncoder.getBitPrice(isRep2[state.get()],
                                                    rep - 2);
        }

        return price;
    }

    int getLongRepAndLenPrice(int rep, int len, State state, int posState) {
        int anyMatchPrice = getAnyMatchPrice(state, posState);
        int anyRepPrice = getAnyRepPrice(anyMatchPrice, state);
        int longRepPrice = getLongRepPrice(anyRepPrice, rep, state, posState);
        return longRepPrice + repLenEncoder.getPrice(len, posState);
    }

    int getMatchAndLenPrice(int normalMatchPrice,
                            int dist, int len, int posState) {
        int price = normalMatchPrice
                    + matchLenEncoder.getPrice(len, posState);
        int distState = getDistState(len);

        if (dist < FULL_DISTANCES) {
            price += fullDistPrices[distState][dist];
        } else {
            // Note that distSlotPrices includes also
            // the price of direct bits.
            int distSlot = getDistSlot(dist);
            price += distSlotPrices[distState][distSlot]
                     + alignPrices[dist & ALIGN_MASK];
        }

        return price;
    }

    private void updateDistPrices() {
        distPriceCount = DIST_PRICE_UPDATE_INTERVAL;

        for (int distState = 0; distState < DIST_STATES; ++distState) {
            for (int distSlot = 0; distSlot < distSlotPricesSize; ++distSlot)
                distSlotPrices[distState][distSlot]
                        = RangeEncoder.getBitTreePrice(
                          distSlots[distState], distSlot);

            for (int distSlot = DIST_MODEL_END; distSlot < distSlotPricesSize;
                    ++distSlot) {
                int count = (distSlot >>> 1) - 1 - ALIGN_BITS;
                distSlotPrices[distState][distSlot]
                        += RangeEncoder.getDirectBitsPrice(count);
            }

            for (int dist = 0; dist < DIST_MODEL_START; ++dist)
                fullDistPrices[distState][dist]
                        = distSlotPrices[distState][dist];
        }

        int dist = DIST_MODEL_START;
        for (int distSlot = DIST_MODEL_START; distSlot < DIST_MODEL_END;
                ++distSlot) {
            int footerBits = (distSlot >>> 1) - 1;
            int base = (2 | (distSlot & 1)) << footerBits;

            int limit = distSpecial[distSlot - DIST_MODEL_START].length;
            for (int i = 0; i < limit; ++i) {
                int distReduced = dist - base;
                int price = RangeEncoder.getReverseBitTreePrice(
                        distSpecial[distSlot - DIST_MODEL_START],
                        distReduced);

                for (int distState = 0; distState < DIST_STATES; ++distState)
                    fullDistPrices[distState][dist]
                            = distSlotPrices[distState][distSlot] + price;

                ++dist;
            }
        }

        assert dist == FULL_DISTANCES;
    }

    private void updateAlignPrices() {
        alignPriceCount = ALIGN_PRICE_UPDATE_INTERVAL;

        for (int i = 0; i < ALIGN_SIZE; ++i)
            alignPrices[i] = RangeEncoder.getReverseBitTreePrice(distAlign,
                                                                 i);
    }

    /**
     * Updates the lookup tables used for calculating match distance
     * and length prices. The updating is skipped for performance reasons
     * if the tables haven't changed much since the previous update.
     */
    void updatePrices() {
        if (distPriceCount <= 0)
            updateDistPrices();

        if (alignPriceCount <= 0)
            updateAlignPrices();

        matchLenEncoder.updatePrices();
        repLenEncoder.updatePrices();
    }


    class LiteralEncoder extends LiteralCoder {
        private final LiteralSubencoder[] subencoders;

        LiteralEncoder(int lc, int lp) {
            super(lc, lp);

            subencoders = new LiteralSubencoder[1 << (lc + lp)];
            for (int i = 0; i < subencoders.length; ++i)
                subencoders[i] = new LiteralSubencoder();
        }

        void reset() {
            for (int i = 0; i < subencoders.length; ++i)
                subencoders[i].reset();
        }

        void encodeInit() {
            // When encoding the first byte of the stream, there is
            // no previous byte in the dictionary so the encode function
            // wouldn't work.
            assert readAhead >= 0;
            subencoders[0].encode();
        }

        void encode() {
            assert readAhead >= 0;
            int i = getSubcoderIndex(lz.getByte(1 + readAhead),
                                     lz.getPos() - readAhead);
            subencoders[i].encode();
        }

        int getPrice(int curByte, int matchByte,
                     int prevByte, int pos, State state) {
            int price = RangeEncoder.getBitPrice(
                    isMatch[state.get()][pos & posMask], 0);

            int i = getSubcoderIndex(prevByte, pos);
            price += state.isLiteral()
                   ? subencoders[i].getNormalPrice(curByte)
                   : subencoders[i].getMatchedPrice(curByte, matchByte);

            return price;
        }

        private class LiteralSubencoder extends LiteralSubcoder {
            void encode() {
                int symbol = lz.getByte(readAhead) | 0x100;

                if (state.isLiteral()) {
                    int subencoderIndex;
                    int bit;

                    do {
                        subencoderIndex = symbol >>> 8;
                        bit = (symbol >>> 7) & 1;
                        rc.encodeBit(probs, subencoderIndex, bit);
                        symbol <<= 1;
                    } while (symbol < 0x10000);

                } else {
                    int matchByte = lz.getByte(reps[0] + 1 + readAhead);
                    int offset = 0x100;
                    int subencoderIndex;
                    int matchBit;
                    int bit;

                    do {
                        matchByte <<= 1;
                        matchBit = matchByte & offset;
                        subencoderIndex = offset + matchBit + (symbol >>> 8);
                        bit = (symbol >>> 7) & 1;
                        rc.encodeBit(probs, subencoderIndex, bit);
                        symbol <<= 1;
                        offset &= ~(matchByte ^ symbol);
                    } while (symbol < 0x10000);
                }

                state.updateLiteral();
            }

            int getNormalPrice(int symbol) {
                int price = 0;
                int subencoderIndex;
                int bit;

                symbol |= 0x100;

                do {
                    subencoderIndex = symbol >>> 8;
                    bit = (symbol >>> 7) & 1;
                    price += RangeEncoder.getBitPrice(probs[subencoderIndex],
                                                      bit);
                    symbol <<= 1;
                } while (symbol < (0x100 << 8));

                return price;
            }

            int getMatchedPrice(int symbol, int matchByte) {
                int price = 0;
                int offset = 0x100;
                int subencoderIndex;
                int matchBit;
                int bit;

                symbol |= 0x100;

                do {
                    matchByte <<= 1;
                    matchBit = matchByte & offset;
                    subencoderIndex = offset + matchBit + (symbol >>> 8);
                    bit = (symbol >>> 7) & 1;
                    price += RangeEncoder.getBitPrice(probs[subencoderIndex],
                                                      bit);
                    symbol <<= 1;
                    offset &= ~(matchByte ^ symbol);
                } while (symbol < (0x100 << 8));

                return price;
            }
        }
    }


    class LengthEncoder extends LengthCoder {
        /**
         * The prices are updated after at least
         * <code>PRICE_UPDATE_INTERVAL</code> many lengths
         * have been encoded with the same posState.
         */
        private static final int PRICE_UPDATE_INTERVAL = 32; // FIXME?

        private final int[] counters;
        private final int[][] prices;

        LengthEncoder(int pb, int niceLen) {
            int posStates = 1 << pb;
            counters = new int[posStates];

            // Always allocate at least LOW_SYMBOLS + MID_SYMBOLS because
            // it makes updatePrices slightly simpler. The prices aren't
            // usually needed anyway if niceLen < 18.
            int lenSymbols = Math.max(niceLen - MATCH_LEN_MIN + 1,
                                      LOW_SYMBOLS + MID_SYMBOLS);
            prices = new int[posStates][lenSymbols];
        }

        void reset() {
            super.reset();

            // Reset counters to zero to force price update before
            // the prices are needed.
            for (int i = 0; i < counters.length; ++i)
                counters[i] = 0;
        }

        void encode(int len, int posState) {
            len -= MATCH_LEN_MIN;

            if (len < LOW_SYMBOLS) {
                rc.encodeBit(choice, 0, 0);
                rc.encodeBitTree(low[posState], len);
            } else {
                rc.encodeBit(choice, 0, 1);
                len -= LOW_SYMBOLS;

                if (len < MID_SYMBOLS) {
                    rc.encodeBit(choice, 1, 0);
                    rc.encodeBitTree(mid[posState], len);
                } else {
                    rc.encodeBit(choice, 1, 1);
                    rc.encodeBitTree(high, len - MID_SYMBOLS);
                }
            }

            --counters[posState];
        }

        int getPrice(int len, int posState) {
            return prices[posState][len - MATCH_LEN_MIN];
        }

        void updatePrices() {
            for (int posState = 0; posState < counters.length; ++posState) {
                if (counters[posState] <= 0) {
                    counters[posState] = PRICE_UPDATE_INTERVAL;
                    updatePrices(posState);
                }
            }
        }

        private void updatePrices(int posState) {
            int choice0Price = RangeEncoder.getBitPrice(choice[0], 0);

            int i = 0;
            for (; i < LOW_SYMBOLS; ++i)
                prices[posState][i] = choice0Price
                        + RangeEncoder.getBitTreePrice(low[posState], i);

            choice0Price = RangeEncoder.getBitPrice(choice[0], 1);
            int choice1Price = RangeEncoder.getBitPrice(choice[1], 0);

            for (; i < LOW_SYMBOLS + MID_SYMBOLS; ++i)
                prices[posState][i] = choice0Price + choice1Price
                         + RangeEncoder.getBitTreePrice(mid[posState],
                                                        i - LOW_SYMBOLS);

            choice1Price = RangeEncoder.getBitPrice(choice[1], 1);

            for (; i < prices[posState].length; ++i)
                prices[posState][i] = choice0Price + choice1Price
                         + RangeEncoder.getBitTreePrice(high, i - LOW_SYMBOLS
                                                              - MID_SYMBOLS);
        }
    }
}
