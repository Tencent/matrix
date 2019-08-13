/*
 * State
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lzma;

final class State {
    static final int STATES = 12;

    private static final int LIT_STATES = 7;

    private static final int LIT_LIT = 0;
    private static final int MATCH_LIT_LIT = 1;
    private static final int REP_LIT_LIT = 2;
    private static final int SHORTREP_LIT_LIT = 3;
    private static final int MATCH_LIT = 4;
    private static final int REP_LIT = 5;
    private static final int SHORTREP_LIT = 6;
    private static final int LIT_MATCH = 7;
    private static final int LIT_LONGREP = 8;
    private static final int LIT_SHORTREP = 9;
    private static final int NONLIT_MATCH = 10;
    private static final int NONLIT_REP = 11;

    private int state;

    State() {}

    State(State other) {
        state = other.state;
    }

    void reset() {
        state = LIT_LIT;
    }

    int get() {
        return state;
    }

    void set(State other) {
        state = other.state;
    }

    void updateLiteral() {
        if (state <= SHORTREP_LIT_LIT)
            state = LIT_LIT;
        else if (state <= LIT_SHORTREP)
            state -= 3;
        else
            state -= 6;
    }

    void updateMatch() {
        state = state < LIT_STATES ? LIT_MATCH : NONLIT_MATCH;
    }

    void updateLongRep() {
        state = state < LIT_STATES ? LIT_LONGREP : NONLIT_REP;
    }

    void updateShortRep() {
        state = state < LIT_STATES ? LIT_SHORTREP : NONLIT_REP;
    }

    boolean isLiteral() {
        return state < LIT_STATES;
    }
}
