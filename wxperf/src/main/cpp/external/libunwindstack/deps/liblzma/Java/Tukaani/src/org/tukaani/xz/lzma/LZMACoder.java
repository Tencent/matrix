/*
 * LZMACoder
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lzma;

import org.tukaani.xz.rangecoder.RangeCoder;

abstract class LZMACoder {
    static final int POS_STATES_MAX = 1 << 4;

    static final int MATCH_LEN_MIN = 2;
    static final int MATCH_LEN_MAX = MATCH_LEN_MIN + LengthCoder.LOW_SYMBOLS
                                     + LengthCoder.MID_SYMBOLS
                                     + LengthCoder.HIGH_SYMBOLS - 1;

    static final int DIST_STATES = 4;
    static final int DIST_SLOTS = 1 << 6;
    static final int DIST_MODEL_START = 4;
    static final int DIST_MODEL_END = 14;
    static final int FULL_DISTANCES = 1 << (DIST_MODEL_END / 2);

    static final int ALIGN_BITS = 4;
    static final int ALIGN_SIZE = 1 << ALIGN_BITS;
    static final int ALIGN_MASK = ALIGN_SIZE - 1;

    static final int REPS = 4;

    final int posMask;

    final int[] reps = new int[REPS];
    final State state = new State();

    final short[][] isMatch = new short[State.STATES][POS_STATES_MAX];
    final short[] isRep = new short[State.STATES];
    final short[] isRep0 = new short[State.STATES];
    final short[] isRep1 = new short[State.STATES];
    final short[] isRep2 = new short[State.STATES];
    final short[][] isRep0Long = new short[State.STATES][POS_STATES_MAX];
    final short[][] distSlots = new short[DIST_STATES][DIST_SLOTS];
    final short[][] distSpecial = { new short[2], new short[2],
                                    new short[4], new short[4],
                                    new short[8], new short[8],
                                    new short[16], new short[16],
                                    new short[32], new short[32] };
    final short[] distAlign = new short[ALIGN_SIZE];

    static final int getDistState(int len) {
        return len < DIST_STATES + MATCH_LEN_MIN
               ? len - MATCH_LEN_MIN
               : DIST_STATES - 1;
    }

    LZMACoder(int pb) {
        posMask = (1 << pb) - 1;
    }

    void reset() {
        reps[0] = 0;
        reps[1] = 0;
        reps[2] = 0;
        reps[3] = 0;
        state.reset();

        for (int i = 0; i < isMatch.length; ++i)
            RangeCoder.initProbs(isMatch[i]);

        RangeCoder.initProbs(isRep);
        RangeCoder.initProbs(isRep0);
        RangeCoder.initProbs(isRep1);
        RangeCoder.initProbs(isRep2);

        for (int i = 0; i < isRep0Long.length; ++i)
            RangeCoder.initProbs(isRep0Long[i]);

        for (int i = 0; i < distSlots.length; ++i)
            RangeCoder.initProbs(distSlots[i]);

        for (int i = 0; i < distSpecial.length; ++i)
            RangeCoder.initProbs(distSpecial[i]);

        RangeCoder.initProbs(distAlign);
    }


    abstract class LiteralCoder {
        private final int lc;
        private final int literalPosMask;

        LiteralCoder(int lc, int lp) {
            this.lc = lc;
            this.literalPosMask = (1 << lp) - 1;
        }

        final int getSubcoderIndex(int prevByte, int pos) {
            int low = prevByte >> (8 - lc);
            int high = (pos & literalPosMask) << lc;
            return low + high;
        }


        abstract class LiteralSubcoder {
            final short[] probs = new short[0x300];

            void reset() {
                RangeCoder.initProbs(probs);
            }
        }
    }


    abstract class LengthCoder {
        static final int LOW_SYMBOLS = 1 << 3;
        static final int MID_SYMBOLS = 1 << 3;
        static final int HIGH_SYMBOLS = 1 << 8;

        final short[] choice = new short[2];
        final short[][] low = new short[POS_STATES_MAX][LOW_SYMBOLS];
        final short[][] mid = new short[POS_STATES_MAX][MID_SYMBOLS];
        final short[] high = new short[HIGH_SYMBOLS];

        void reset() {
            RangeCoder.initProbs(choice);

            for (int i = 0; i < low.length; ++i)
                RangeCoder.initProbs(low[i]);

            for (int i = 0; i < low.length; ++i)
                RangeCoder.initProbs(mid[i]);

            RangeCoder.initProbs(high);
        }
    }
}
