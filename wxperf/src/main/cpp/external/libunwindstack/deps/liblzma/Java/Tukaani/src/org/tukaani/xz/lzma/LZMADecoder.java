/*
 * LZMADecoder
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lzma;

import java.io.IOException;
import org.tukaani.xz.lz.LZDecoder;
import org.tukaani.xz.rangecoder.RangeDecoder;

public final class LZMADecoder extends LZMACoder {
    private final LZDecoder lz;
    private final RangeDecoder rc;
    private final LiteralDecoder literalDecoder;
    private final LengthDecoder matchLenDecoder = new LengthDecoder();
    private final LengthDecoder repLenDecoder = new LengthDecoder();

    public LZMADecoder(LZDecoder lz, RangeDecoder rc, int lc, int lp, int pb) {
        super(pb);
        this.lz = lz;
        this.rc = rc;
        this.literalDecoder = new LiteralDecoder(lc, lp);
        reset();
    }

    public void reset() {
        super.reset();
        literalDecoder.reset();
        matchLenDecoder.reset();
        repLenDecoder.reset();
    }

    /**
     * Returns true if LZMA end marker was detected. It is encoded as
     * the maximum match distance which with signed ints becomes -1. This
     * function is needed only for LZMA1. LZMA2 doesn't use the end marker
     * in the LZMA layer.
     */
    public boolean endMarkerDetected() {
        return reps[0] == -1;
    }

    public void decode() throws IOException {
        lz.repeatPending();

        while (lz.hasSpace()) {
            int posState = lz.getPos() & posMask;

            if (rc.decodeBit(isMatch[state.get()], posState) == 0) {
                literalDecoder.decode();
            } else {
                int len = rc.decodeBit(isRep, state.get()) == 0
                          ? decodeMatch(posState)
                          : decodeRepMatch(posState);

                // NOTE: With LZMA1 streams that have the end marker,
                // this will throw CorruptedInputException. LZMAInputStream
                // handles it specially.
                lz.repeat(reps[0], len);
            }
        }

        rc.normalize();
    }

    private int decodeMatch(int posState) throws IOException {
        state.updateMatch();

        reps[3] = reps[2];
        reps[2] = reps[1];
        reps[1] = reps[0];

        int len = matchLenDecoder.decode(posState);
        int distSlot = rc.decodeBitTree(distSlots[getDistState(len)]);

        if (distSlot < DIST_MODEL_START) {
            reps[0] = distSlot;
        } else {
            int limit = (distSlot >> 1) - 1;
            reps[0] = (2 | (distSlot & 1)) << limit;

            if (distSlot < DIST_MODEL_END) {
                reps[0] |= rc.decodeReverseBitTree(
                        distSpecial[distSlot - DIST_MODEL_START]);
            } else {
                reps[0] |= rc.decodeDirectBits(limit - ALIGN_BITS)
                           << ALIGN_BITS;
                reps[0] |= rc.decodeReverseBitTree(distAlign);
            }
        }

        return len;
    }

    private int decodeRepMatch(int posState) throws IOException {
        if (rc.decodeBit(isRep0, state.get()) == 0) {
            if (rc.decodeBit(isRep0Long[state.get()], posState) == 0) {
                state.updateShortRep();
                return 1;
            }
        } else {
            int tmp;

            if (rc.decodeBit(isRep1, state.get()) == 0) {
                tmp = reps[1];
            } else {
                if (rc.decodeBit(isRep2, state.get()) == 0) {
                    tmp = reps[2];
                } else {
                    tmp = reps[3];
                    reps[3] = reps[2];
                }

                reps[2] = reps[1];
            }

            reps[1] = reps[0];
            reps[0] = tmp;
        }

        state.updateLongRep();

        return repLenDecoder.decode(posState);
    }


    private class LiteralDecoder extends LiteralCoder {
        private final LiteralSubdecoder[] subdecoders;

        LiteralDecoder(int lc, int lp) {
            super(lc, lp);

            subdecoders = new LiteralSubdecoder[1 << (lc + lp)];
            for (int i = 0; i < subdecoders.length; ++i)
                subdecoders[i] = new LiteralSubdecoder();
        }

        void reset() {
            for (int i = 0; i < subdecoders.length; ++i)
                subdecoders[i].reset();
        }

        void decode() throws IOException {
            int i = getSubcoderIndex(lz.getByte(0), lz.getPos());
            subdecoders[i].decode();
        }


        private class LiteralSubdecoder extends LiteralSubcoder {
            void decode() throws IOException {
                int symbol = 1;

                if (state.isLiteral()) {
                    do {
                        symbol = (symbol << 1) | rc.decodeBit(probs, symbol);
                    } while (symbol < 0x100);

                } else {
                    int matchByte = lz.getByte(reps[0]);
                    int offset = 0x100;
                    int matchBit;
                    int bit;

                    do {
                        matchByte <<= 1;
                        matchBit = matchByte & offset;
                        bit = rc.decodeBit(probs, offset + matchBit + symbol);
                        symbol = (symbol << 1) | bit;
                        offset &= (0 - bit) ^ ~matchBit;
                    } while (symbol < 0x100);
                }

                lz.putByte((byte)symbol);
                state.updateLiteral();
            }
        }
    }


    private class LengthDecoder extends LengthCoder {
        int decode(int posState) throws IOException {
            if (rc.decodeBit(choice, 0) == 0)
                return rc.decodeBitTree(low[posState]) + MATCH_LEN_MIN;

            if (rc.decodeBit(choice, 1) == 0)
                return rc.decodeBitTree(mid[posState])
                       + MATCH_LEN_MIN + LOW_SYMBOLS;

            return rc.decodeBitTree(high)
                   + MATCH_LEN_MIN + LOW_SYMBOLS + MID_SYMBOLS;
        }
    }
}
