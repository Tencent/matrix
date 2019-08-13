/*
 * LZMAEncoderNormal
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

final class LZMAEncoderNormal extends LZMAEncoder {
    private static final int OPTS = 4096;

    private static final int EXTRA_SIZE_BEFORE = OPTS;
    private static final int EXTRA_SIZE_AFTER = OPTS;

    private final Optimum[] opts = new Optimum[OPTS];
    private int optCur = 0;
    private int optEnd = 0;

    private Matches matches;

    // These are fields solely to avoid allocating the objects again and
    // again on each function call.
    private final int[] repLens = new int[REPS];
    private final State nextState = new State();

    static int getMemoryUsage(int dictSize, int extraSizeBefore, int mf) {
        return LZEncoder.getMemoryUsage(dictSize,
                   Math.max(extraSizeBefore, EXTRA_SIZE_BEFORE),
                   EXTRA_SIZE_AFTER, MATCH_LEN_MAX, mf)
               + OPTS * 64 / 1024;
    }

    LZMAEncoderNormal(RangeEncoder rc, int lc, int lp, int pb,
                             int dictSize, int extraSizeBefore,
                             int niceLen, int mf, int depthLimit) {
        super(rc, LZEncoder.getInstance(dictSize,
                                        Math.max(extraSizeBefore,
                                                 EXTRA_SIZE_BEFORE),
                                        EXTRA_SIZE_AFTER,
                                        niceLen, MATCH_LEN_MAX,
                                        mf, depthLimit),
              lc, lp, pb, dictSize, niceLen);

        for (int i = 0; i < OPTS; ++i)
            opts[i] = new Optimum();
    }

    public void reset() {
        optCur = 0;
        optEnd = 0;
        super.reset();
    }

    /**
     * Converts the opts array from backward indexes to forward indexes.
     * Then it will be simple to get the next symbol from the array
     * in later calls to <code>getNextSymbol()</code>.
     */
    private int convertOpts() {
        optEnd = optCur;

        int optPrev = opts[optCur].optPrev;

        do {
            Optimum opt = opts[optCur];

            if (opt.prev1IsLiteral) {
                opts[optPrev].optPrev = optCur;
                opts[optPrev].backPrev = -1;
                optCur = optPrev--;

                if (opt.hasPrev2) {
                    opts[optPrev].optPrev = optPrev + 1;
                    opts[optPrev].backPrev = opt.backPrev2;
                    optCur = optPrev;
                    optPrev = opt.optPrev2;
                }
            }

            int temp = opts[optPrev].optPrev;
            opts[optPrev].optPrev = optCur;
            optCur = optPrev;
            optPrev = temp;
        } while (optCur > 0);

        optCur = opts[0].optPrev;
        back = opts[optCur].backPrev;
        return optCur;
    }

    int getNextSymbol() {
        // If there are pending symbols from an earlier call to this
        // function, return those symbols first.
        if (optCur < optEnd) {
            int len = opts[optCur].optPrev - optCur;
            optCur = opts[optCur].optPrev;
            back = opts[optCur].backPrev;
            return len;
        }

        assert optCur == optEnd;
        optCur = 0;
        optEnd = 0;
        back = -1;

        if (readAhead == -1)
            matches = getMatches();

        // Get the number of bytes available in the dictionary, but
        // not more than the maximum match length. If there aren't
        // enough bytes remaining to encode a match at all, return
        // immediately to encode this byte as a literal.
        int avail = Math.min(lz.getAvail(), MATCH_LEN_MAX);
        if (avail < MATCH_LEN_MIN)
            return 1;

        // Get the lengths of repeated matches.
        int repBest = 0;
        for (int rep = 0; rep < REPS; ++rep) {
            repLens[rep] = lz.getMatchLen(reps[rep], avail);

            if (repLens[rep] < MATCH_LEN_MIN) {
                repLens[rep] = 0;
                continue;
            }

            if (repLens[rep] > repLens[repBest])
                repBest = rep;
        }

        // Return if the best repeated match is at least niceLen bytes long.
        if (repLens[repBest] >= niceLen) {
            back = repBest;
            skip(repLens[repBest] - 1);
            return repLens[repBest];
        }

        // Initialize mainLen and mainDist to the longest match found
        // by the match finder.
        int mainLen = 0;
        int mainDist = 0;
        if (matches.count > 0) {
            mainLen = matches.len[matches.count - 1];
            mainDist = matches.dist[matches.count - 1];

            // Return if it is at least niceLen bytes long.
            if (mainLen >= niceLen) {
                back = mainDist + REPS;
                skip(mainLen - 1);
                return mainLen;
            }
        }

        int curByte = lz.getByte(0);
        int matchByte = lz.getByte(reps[0] + 1);

        // If the match finder found no matches and this byte cannot be
        // encoded as a repeated match (short or long), we must be return
        // to have the byte encoded as a literal.
        if (mainLen < MATCH_LEN_MIN && curByte != matchByte
                && repLens[repBest] < MATCH_LEN_MIN)
            return 1;


        int pos = lz.getPos();
        int posState = pos & posMask;

        // Calculate the price of encoding the current byte as a literal.
        {
            int prevByte = lz.getByte(1);
            int literalPrice = literalEncoder.getPrice(curByte, matchByte,
                                                       prevByte, pos, state);
            opts[1].set1(literalPrice, 0, -1);
        }

        int anyMatchPrice = getAnyMatchPrice(state, posState);
        int anyRepPrice = getAnyRepPrice(anyMatchPrice, state);

        // If it is possible to encode this byte as a short rep, see if
        // it is cheaper than encoding it as a literal.
        if (matchByte == curByte) {
            int shortRepPrice = getShortRepPrice(anyRepPrice,
                                                 state, posState);
            if (shortRepPrice < opts[1].price)
                opts[1].set1(shortRepPrice, 0, 0);
        }

        // Return if there is neither normal nor long repeated match. Use
        // a short match instead of a literal if is is possible and cheaper.
        optEnd = Math.max(mainLen, repLens[repBest]);
        if (optEnd < MATCH_LEN_MIN) {
            assert optEnd == 0 : optEnd;
            back = opts[1].backPrev;
            return 1;
        }


        // Update the lookup tables for distances and lengths before using
        // those price calculation functions. (The price function above
        // don't need these tables.)
        updatePrices();

        // Initialize the state and reps of this position in opts[].
        // updateOptStateAndReps() will need these to get the new
        // state and reps for the next byte.
        opts[0].state.set(state);
        System.arraycopy(reps, 0, opts[0].reps, 0, REPS);

        // Initialize the prices for latter opts that will be used below.
        for (int i = optEnd; i >= MATCH_LEN_MIN; --i)
            opts[i].reset();

        // Calculate the prices of repeated matches of all lengths.
        for (int rep = 0; rep < REPS; ++rep) {
            int repLen = repLens[rep];
            if (repLen < MATCH_LEN_MIN)
                continue;

            int longRepPrice = getLongRepPrice(anyRepPrice, rep,
                                               state, posState);
            do {
                int price = longRepPrice + repLenEncoder.getPrice(repLen,
                                                                  posState);
                if (price < opts[repLen].price)
                    opts[repLen].set1(price, 0, rep);
            } while (--repLen >= MATCH_LEN_MIN);
        }

        // Calculate the prices of normal matches that are longer than rep0.
        {
            int len = Math.max(repLens[0] + 1, MATCH_LEN_MIN);
            if (len <= mainLen) {
                int normalMatchPrice = getNormalMatchPrice(anyMatchPrice,
                                                           state);

                // Set i to the index of the shortest match that is
                // at least len bytes long.
                int i = 0;
                while (len > matches.len[i])
                    ++i;

                while (true) {
                    int dist = matches.dist[i];
                    int price = getMatchAndLenPrice(normalMatchPrice,
                                                    dist, len, posState);
                    if (price < opts[len].price)
                        opts[len].set1(price, 0, dist + REPS);

                    if (len == matches.len[i])
                        if (++i == matches.count)
                            break;

                    ++len;
                }
            }
        }


        avail = Math.min(lz.getAvail(), OPTS - 1);

        // Get matches for later bytes and optimize the use of LZMA symbols
        // by calculating the prices and picking the cheapest symbol
        // combinations.
        while (++optCur < optEnd) {
            matches = getMatches();
            if (matches.count > 0
                    && matches.len[matches.count - 1] >= niceLen)
                break;

            --avail;
            ++pos;
            posState = pos & posMask;

            updateOptStateAndReps();
            anyMatchPrice = opts[optCur].price
                            + getAnyMatchPrice(opts[optCur].state, posState);
            anyRepPrice = getAnyRepPrice(anyMatchPrice, opts[optCur].state);

            calc1BytePrices(pos, posState, avail, anyRepPrice);

            if (avail >= MATCH_LEN_MIN) {
                int startLen = calcLongRepPrices(pos, posState,
                                                 avail, anyRepPrice);
                if (matches.count > 0)
                    calcNormalMatchPrices(pos, posState, avail,
                                          anyMatchPrice, startLen);
            }
        }

        return convertOpts();
    }

    /**
     * Updates the state and reps for the current byte in the opts array.
     */
    private void updateOptStateAndReps() {
        int optPrev = opts[optCur].optPrev;
        assert optPrev < optCur;

        if (opts[optCur].prev1IsLiteral) {
            --optPrev;

            if (opts[optCur].hasPrev2) {
                opts[optCur].state.set(opts[opts[optCur].optPrev2].state);
                if (opts[optCur].backPrev2 < REPS)
                    opts[optCur].state.updateLongRep();
                else
                    opts[optCur].state.updateMatch();
            } else {
                opts[optCur].state.set(opts[optPrev].state);
            }

            opts[optCur].state.updateLiteral();
        } else {
            opts[optCur].state.set(opts[optPrev].state);
        }

        if (optPrev == optCur - 1) {
            // Must be either a short rep or a literal.
            assert opts[optCur].backPrev == 0 || opts[optCur].backPrev == -1;

            if (opts[optCur].backPrev == 0)
                opts[optCur].state.updateShortRep();
            else
                opts[optCur].state.updateLiteral();

            System.arraycopy(opts[optPrev].reps, 0,
                             opts[optCur].reps, 0, REPS);
        } else {
            int back;
            if (opts[optCur].prev1IsLiteral && opts[optCur].hasPrev2) {
                optPrev = opts[optCur].optPrev2;
                back = opts[optCur].backPrev2;
                opts[optCur].state.updateLongRep();
            } else {
                back = opts[optCur].backPrev;
                if (back < REPS)
                    opts[optCur].state.updateLongRep();
                else
                    opts[optCur].state.updateMatch();
            }

            if (back < REPS) {
                opts[optCur].reps[0] = opts[optPrev].reps[back];

                int rep;
                for (rep = 1; rep <= back; ++rep)
                    opts[optCur].reps[rep] = opts[optPrev].reps[rep - 1];

                for (; rep < REPS; ++rep)
                    opts[optCur].reps[rep] = opts[optPrev].reps[rep];
            } else {
                opts[optCur].reps[0] = back - REPS;
                System.arraycopy(opts[optPrev].reps, 0,
                                 opts[optCur].reps, 1, REPS - 1);
            }
        }
    }

    /**
     * Calculates prices of a literal, a short rep, and literal + rep0.
     */
    private void calc1BytePrices(int pos, int posState,
                                 int avail, int anyRepPrice) {
        // This will be set to true if using a literal or a short rep.
        boolean nextIsByte = false;

        int curByte = lz.getByte(0);
        int matchByte = lz.getByte(opts[optCur].reps[0] + 1);

        // Try a literal.
        int literalPrice = opts[optCur].price
                + literalEncoder.getPrice(curByte, matchByte, lz.getByte(1),
                                          pos, opts[optCur].state);
        if (literalPrice < opts[optCur + 1].price) {
            opts[optCur + 1].set1(literalPrice, optCur, -1);
            nextIsByte = true;
        }

        // Try a short rep.
        if (matchByte == curByte && (opts[optCur + 1].optPrev == optCur
                                      || opts[optCur + 1].backPrev != 0)) {
            int shortRepPrice = getShortRepPrice(anyRepPrice,
                                                 opts[optCur].state,
                                                 posState);
            if (shortRepPrice <= opts[optCur + 1].price) {
                opts[optCur + 1].set1(shortRepPrice, optCur, 0);
                nextIsByte = true;
            }
        }

        // If neither a literal nor a short rep was the cheapest choice,
        // try literal + long rep0.
        if (!nextIsByte && matchByte != curByte && avail > MATCH_LEN_MIN) {
            int lenLimit = Math.min(niceLen, avail - 1);
            int len = lz.getMatchLen(1, opts[optCur].reps[0], lenLimit);

            if (len >= MATCH_LEN_MIN) {
                nextState.set(opts[optCur].state);
                nextState.updateLiteral();
                int nextPosState = (pos + 1) & posMask;
                int price = literalPrice
                            + getLongRepAndLenPrice(0, len,
                                                    nextState, nextPosState);

                int i = optCur + 1 + len;
                while (optEnd < i)
                    opts[++optEnd].reset();

                if (price < opts[i].price)
                    opts[i].set2(price, optCur, 0);
            }
        }
    }

    /**
     * Calculates prices of long rep and long rep + literal + rep0.
     */
    private int calcLongRepPrices(int pos, int posState,
                                  int avail, int anyRepPrice) {
        int startLen = MATCH_LEN_MIN;
        int lenLimit = Math.min(avail, niceLen);

        for (int rep = 0; rep < REPS; ++rep) {
            int len = lz.getMatchLen(opts[optCur].reps[rep], lenLimit);
            if (len < MATCH_LEN_MIN)
                continue;

            while (optEnd < optCur + len)
                opts[++optEnd].reset();

            int longRepPrice = getLongRepPrice(anyRepPrice, rep,
                                               opts[optCur].state, posState);

            for (int i = len; i >= MATCH_LEN_MIN; --i) {
                int price = longRepPrice
                            + repLenEncoder.getPrice(i, posState);
                if (price < opts[optCur + i].price)
                    opts[optCur + i].set1(price, optCur, rep);
            }

            if (rep == 0)
                startLen = len + 1;

            int len2Limit = Math.min(niceLen, avail - len - 1);
            int len2 = lz.getMatchLen(len + 1, opts[optCur].reps[rep],
                                      len2Limit);

            if (len2 >= MATCH_LEN_MIN) {
                // Rep
                int price = longRepPrice
                            + repLenEncoder.getPrice(len, posState);
                nextState.set(opts[optCur].state);
                nextState.updateLongRep();

                // Literal
                int curByte = lz.getByte(len, 0);
                int matchByte = lz.getByte(0); // lz.getByte(len, len)
                int prevByte = lz.getByte(len, 1);
                price += literalEncoder.getPrice(curByte, matchByte, prevByte,
                                                 pos + len, nextState);
                nextState.updateLiteral();

                // Rep0
                int nextPosState = (pos + len + 1) & posMask;
                price += getLongRepAndLenPrice(0, len2,
                                               nextState, nextPosState);

                int i = optCur + len + 1 + len2;
                while (optEnd < i)
                    opts[++optEnd].reset();

                if (price < opts[i].price)
                    opts[i].set3(price, optCur, rep, len, 0);
            }
        }

        return startLen;
    }

    /**
     * Calculates prices of a normal match and normal match + literal + rep0.
     */
    private void calcNormalMatchPrices(int pos, int posState, int avail,
                                       int anyMatchPrice, int startLen) {
        // If the longest match is so long that it would not fit into
        // the opts array, shorten the matches.
        if (matches.len[matches.count - 1] > avail) {
            matches.count = 0;
            while (matches.len[matches.count] < avail)
                ++matches.count;

            matches.len[matches.count++] = avail;
        }

        if (matches.len[matches.count - 1] < startLen)
            return;

        while (optEnd < optCur + matches.len[matches.count - 1])
            opts[++optEnd].reset();

        int normalMatchPrice = getNormalMatchPrice(anyMatchPrice,
                                                   opts[optCur].state);

        int match = 0;
        while (startLen > matches.len[match])
            ++match;

        for (int len = startLen; ; ++len) {
            int dist = matches.dist[match];

            // Calculate the price of a match of len bytes from the nearest
            // possible distance.
            int matchAndLenPrice = getMatchAndLenPrice(normalMatchPrice,
                                                       dist, len, posState);
            if (matchAndLenPrice < opts[optCur + len].price)
                opts[optCur + len].set1(matchAndLenPrice,
                                        optCur, dist + REPS);

            if (len != matches.len[match])
                continue;

            // Try match + literal + rep0. First get the length of the rep0.
            int len2Limit = Math.min(niceLen, avail - len - 1);
            int len2 = lz.getMatchLen(len + 1, dist, len2Limit);

            if (len2 >= MATCH_LEN_MIN) {
                nextState.set(opts[optCur].state);
                nextState.updateMatch();

                // Literal
                int curByte = lz.getByte(len, 0);
                int matchByte = lz.getByte(0); // lz.getByte(len, len)
                int prevByte = lz.getByte(len, 1);
                int price = matchAndLenPrice
                        + literalEncoder.getPrice(curByte, matchByte,
                                                  prevByte, pos + len,
                                                  nextState);
                nextState.updateLiteral();

                // Rep0
                int nextPosState = (pos + len + 1) & posMask;
                price += getLongRepAndLenPrice(0, len2,
                                               nextState, nextPosState);

                int i = optCur + len + 1 + len2;
                while (optEnd < i)
                    opts[++optEnd].reset();

                if (price < opts[i].price)
                    opts[i].set3(price, optCur, dist + REPS, len, 0);
            }

            if (++match == matches.count)
                break;
        }
    }
}
