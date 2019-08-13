/*
 * Binary Tree match finder with 2-, 3-, and 4-byte hashing
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package external.libunwindstack.deps.liblzma.Java.Tukaani.src.org.tukaani.xz.lz;

final class BT4 extends LZEncoder {
    private final Hash234 hash;
    private final int[] tree;
    private final Matches matches;
    private final int depthLimit;

    private final int cyclicSize;
    private int cyclicPos = -1;
    private int lzPos;

    static int getMemoryUsage(int dictSize) {
        return Hash234.getMemoryUsage(dictSize) + dictSize / (1024 / 8) + 10;
    }

    BT4(int dictSize, int beforeSizeMin, int readAheadMax,
            int niceLen, int matchLenMax, int depthLimit) {
        super(dictSize, beforeSizeMin, readAheadMax, niceLen, matchLenMax);

        cyclicSize = dictSize + 1;
        lzPos = cyclicSize;

        hash = new Hash234(dictSize);
        tree = new int[cyclicSize * 2];

        // Substracting 1 because the shortest match that this match
        // finder can find is 2 bytes, so there's no need to reserve
        // space for one-byte matches.
        matches = new Matches(niceLen - 1);

        this.depthLimit = depthLimit > 0 ? depthLimit : 16 + niceLen / 2;
    }

    private int movePos() {
        int avail = movePos(niceLen, 4);

        if (avail != 0) {
            if (++lzPos == Integer.MAX_VALUE) {
                int normalizationOffset = Integer.MAX_VALUE - cyclicSize;
                hash.normalize(normalizationOffset);
                normalize(tree, normalizationOffset);
                lzPos -= normalizationOffset;
            }

            if (++cyclicPos == cyclicSize)
                cyclicPos = 0;
        }

        return avail;
    }

    public Matches getMatches() {
        matches.count = 0;

        int matchLenLimit = matchLenMax;
        int niceLenLimit = niceLen;
        int avail = movePos();

        if (avail < matchLenLimit) {
            if (avail == 0)
                return matches;

            matchLenLimit = avail;
            if (niceLenLimit > avail)
                niceLenLimit = avail;
        }

        hash.calcHashes(buf, readPos);
        int delta2 = lzPos - hash.getHash2Pos();
        int delta3 = lzPos - hash.getHash3Pos();
        int currentMatch = hash.getHash4Pos();
        hash.updateTables(lzPos);

        int lenBest = 0;

        // See if the hash from the first two bytes found a match.
        // The hashing algorithm guarantees that if the first byte
        // matches, also the second byte does, so there's no need to
        // test the second byte.
        if (delta2 < cyclicSize && buf[readPos - delta2] == buf[readPos]) {
            lenBest = 2;
            matches.len[0] = 2;
            matches.dist[0] = delta2 - 1;
            matches.count = 1;
        }

        // See if the hash from the first three bytes found a match that
        // is different from the match possibly found by the two-byte hash.
        // Also here the hashing algorithm guarantees that if the first byte
        // matches, also the next two bytes do.
        if (delta2 != delta3 && delta3 < cyclicSize
                && buf[readPos - delta3] == buf[readPos]) {
            lenBest = 3;
            matches.dist[matches.count++] = delta3 - 1;
            delta2 = delta3;
        }

        // If a match was found, see how long it is.
        if (matches.count > 0) {
            while (lenBest < matchLenLimit && buf[readPos + lenBest - delta2]
                                              == buf[readPos + lenBest])
                ++lenBest;

            matches.len[matches.count - 1] = lenBest;

            // Return if it is long enough (niceLen or reached the end of
            // the dictionary).
            if (lenBest >= niceLenLimit) {
                skip(niceLenLimit, currentMatch);
                return matches;
            }
        }

        // Long enough match wasn't found so easily. Look for better matches
        // from the binary tree.
        if (lenBest < 3)
            lenBest = 3;

        int depth = depthLimit;

        int ptr0 = (cyclicPos << 1) + 1;
        int ptr1 = cyclicPos << 1;
        int len0 = 0;
        int len1 = 0;

        while (true) {
            int delta = lzPos - currentMatch;

            // Return if the search depth limit has been reached or
            // if the distance of the potential match exceeds the
            // dictionary size.
            if (depth-- == 0 || delta >= cyclicSize) {
                tree[ptr0] = 0;
                tree[ptr1] = 0;
                return matches;
            }

            int pair = (cyclicPos - delta
                        + (delta > cyclicPos ? cyclicSize : 0)) << 1;
            int len = Math.min(len0, len1);

            if (buf[readPos + len - delta] == buf[readPos + len]) {
                while (++len < matchLenLimit)
                    if (buf[readPos + len - delta] != buf[readPos + len])
                        break;

                if (len > lenBest) {
                    lenBest = len;
                    matches.len[matches.count] = len;
                    matches.dist[matches.count] = delta - 1;
                    ++matches.count;

                    if (len >= niceLenLimit) {
                        tree[ptr1] = tree[pair];
                        tree[ptr0] = tree[pair + 1];
                        return matches;
                    }
                }
            }

            if ((buf[readPos + len - delta] & 0xFF)
                    < (buf[readPos + len] & 0xFF)) {
                tree[ptr1] = currentMatch;
                ptr1 = pair + 1;
                currentMatch = tree[ptr1];
                len1 = len;
            } else {
                tree[ptr0] = currentMatch;
                ptr0 = pair;
                currentMatch = tree[ptr0];
                len0 = len;
            }
        }
    }

    private void skip(int niceLenLimit, int currentMatch) {
        int depth = depthLimit;

        int ptr0 = (cyclicPos << 1) + 1;
        int ptr1 = cyclicPos << 1;
        int len0 = 0;
        int len1 = 0;

        while (true) {
            int delta = lzPos - currentMatch;

            if (depth-- == 0 || delta >= cyclicSize) {
                tree[ptr0] = 0;
                tree[ptr1] = 0;
                return;
            }

            int pair = (cyclicPos - delta
                        + (delta > cyclicPos ? cyclicSize : 0)) << 1;
            int len = Math.min(len0, len1);

            if (buf[readPos + len - delta] == buf[readPos + len]) {
                // No need to look for longer matches than niceLenLimit
                // because we only are updating the tree, not returning
                // matches found to the caller.
                do {
                    if (++len == niceLenLimit) {
                        tree[ptr1] = tree[pair];
                        tree[ptr0] = tree[pair + 1];
                        return;
                    }
                } while (buf[readPos + len - delta] == buf[readPos + len]);
            }

            if ((buf[readPos + len - delta] & 0xFF)
                    < (buf[readPos + len] & 0xFF)) {
                tree[ptr1] = currentMatch;
                ptr1 = pair + 1;
                currentMatch = tree[ptr1];
                len1 = len;
            } else {
                tree[ptr0] = currentMatch;
                ptr0 = pair;
                currentMatch = tree[ptr0];
                len0 = len;
            }
        }
    }

    public void skip(int len) {
        while (len-- > 0) {
            int niceLenLimit = niceLen;
            int avail = movePos();

            if (avail < niceLenLimit) {
                if (avail == 0)
                    continue;

                niceLenLimit = avail;
            }

            hash.calcHashes(buf, readPos);
            int currentMatch = hash.getHash4Pos();
            hash.updateTables(lzPos);

            skip(niceLenLimit, currentMatch);
        }
    }
}
