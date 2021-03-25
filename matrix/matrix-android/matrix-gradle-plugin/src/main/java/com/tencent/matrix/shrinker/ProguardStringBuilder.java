package com.tencent.matrix.shrinker;

import java.util.Arrays;
import java.util.List;

public class ProguardStringBuilder {

    private static char[] alphaArray = new char[26];
    private static char[] numberArray = new char[10];

    private final static List<String> WIN_INVALID_FILE_NAME = Arrays.asList(
            "aux", "nul", "prn", "nul", "con",
            "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
            "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    );


    private int alphaIndex;
    private int numberIndex;
    private String prefix = "";

    public ProguardStringBuilder() {
        int i = 0;
        for (char alpha = 'a'; alpha <= 'z'; alpha++) {
            alphaArray[i++]  = alpha;
        }
        i = 0;
        for (char num = '0'; num <= '9'; num++) {
            numberArray[i++] = num;
        }
    }

    private String getPrefix(int len) {
        if (len == 0) {
            return "" + alphaArray[0];
        } else if (len == 1) {
            char curChar = prefix.charAt(0);
            if (curChar < alphaArray[alphaArray.length - 1]) {
                return "" + (char) (curChar + 1);
            } else {
                return "" + alphaArray[0] + alphaArray[0];
            }
        } else {
            char lastChar = prefix.charAt(len - 1);
            if (lastChar == alphaArray[alphaArray.length - 1]) {
                return prefix.substring(0, len - 1) + numberArray[0];
            } else if (lastChar == numberArray[numberArray.length - 1]) {
                return getPrefix(len - 1) + alphaArray[0];
            } else {
                return prefix.substring(0, len - 1) + (char) (lastChar + 1);
            }
        }
    }

    private void nextTurn() {
        alphaIndex = 0;
        numberIndex = 0;
        prefix = getPrefix(prefix.length());
    }

    public String generateNextProguard() {
        String result = "";
        if (prefix.equals("")) {
            if (alphaIndex <= alphaArray.length - 1) {
                result = String.valueOf(alphaArray[alphaIndex++]);
            } else {
                nextTurn();
                result = prefix + alphaArray[alphaIndex++];
            }
        } else {
            if (alphaIndex <= alphaArray.length - 1) {
                result = prefix + alphaArray[alphaIndex++];
            } else if (numberIndex <= numberArray.length - 1) {
                result = prefix + numberArray[numberIndex++];
            } else {
                nextTurn();
                result = prefix + alphaArray[alphaIndex++];
            }
        }
        //System.out.println(prefix + "," + result)
        return result;
    }

    public String generateNextProguardFileName() {
        String result = generateNextProguard();
        while (WIN_INVALID_FILE_NAME.contains(result.toLowerCase())) {
            result = generateNextProguard();
        }
        return result;
    }

    public void reset() {
        alphaIndex = 0;
        numberIndex = 0;
        prefix = "";
    }

}
