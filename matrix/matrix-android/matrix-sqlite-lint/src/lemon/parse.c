/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is include which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#line 54 "parse.y"

#include "sqliteInt.h"
#include "parse.h"

/*
** An instance of this structure holds information about the
** LIMIT clause of a SELECT statement.
*/
struct LimitVal {
  Expr *pLimit;    /* The LIMIT expression.  NULL if there is no limit */
  Expr *pOffset;   /* The OFFSET expression.  NULL if there is none */
};

/*
** An instance of this structure is used to store the LIKE,
** GLOB, NOT LIKE, and NOT GLOB operators.
*/
struct LikeOp {
  Token eOperator;  /* "like" or "glob" or "regexp" */
  int not;         /* True if the NOT keyword is present */
};

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

/*
** An instance of this structure holds the ATTACH key and the key type.
*/
struct AttachKey { int type;  Token key; };

#line 48 "parse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/*
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands.
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    sqlite3ParserTOKENTYPE     is the data type used for minor tokens given
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 230
#define YYACTIONTYPE unsigned short int
#define sqlite3ParserTOKENTYPE Token
typedef union {
  sqlite3ParserTOKENTYPE yy0;
  ValuesList* yy37;
  SrcList* yy41;
  struct LimitVal yy76;
  struct LikeOp yy78;
  Token yy82;
  ExprList* yy162;
  Select* yy203;
  IdList* yy306;
  struct {int value; int mask;} yy331;
  Expr* yy396;
  int yy444;
  int yy459;
} YYMINORTYPE;
#define YYSTACKDEPTH 100
#define sqlite3ParserARG_SDECL Parse *pParse;
#define sqlite3ParserARG_PDECL ,Parse *pParse
#define sqlite3ParserARG_FETCH Parse *pParse = yypParser->pParse
#define sqlite3ParserARG_STORE yypParser->pParse = pParse
#define YYNSTATE 466
#define YYNRULE 254
#define YYERRORSYMBOL 136
#define YYERRSYMDT yy459
#define YYFALLBACK 1
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* Next are that tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
static const YYACTIONTYPE yy_action[] = {
 /*     0 */   222,  360,  160,  141,  322,  352,   65,   64,   64,   64,
 /*    10 */    64,  426,   66,   66,   66,   66,   71,   71,   72,   72,
 /*    20 */    72,   73,   69,   66,   66,   66,   66,   71,   71,   72,
 /*    30 */    72,   72,   73,  365,   66,   66,   66,   66,   71,   71,
 /*    40 */    72,   72,   72,   73,   71,   71,   72,   72,   72,   73,
 /*    50 */    55,   77,  230,  373,  376,  370,  370,   65,   64,   64,
 /*    60 */    64,   64,  410,   66,   66,   66,   66,   71,   71,   72,
 /*    70 */    72,   72,   73,  222,  329,  234,  352,   48,  380,   65,
 /*    80 */    15,  290,   95,  374,  437,  171,  234,  162,  366,  330,
 /*    90 */   317,  286,   19,   70,  387,  164,  171,  127,   73,  289,
 /*   100 */   278,  305,   20,  121,  161,  331,  365,  327,  368,  369,
 /*   110 */    75,  342,    3,  142,  352,  120,  447,  374,  328,  342,
 /*   120 */    37,  408,  352,   55,   77,  230,  373,  376,  370,  370,
 /*   130 */    65,   64,   64,   64,   64,  367,   66,   66,   66,   66,
 /*   140 */    71,   71,   72,   72,   72,   73,  222,  466,  343,   90,
 /*   150 */   304,  276,   65,  135,  323,  188,  274,  196,  266,  133,
 /*   160 */   319,   50,  301,   54,  172,  323,  188,  274,  196,  266,
 /*   170 */   133,  289,   17,  225,  289,  172,  384,  150,  446,  365,
 /*   180 */   251,  255,  259,  432,  268,  352,  378,  378,  434,  260,
 /*   190 */   115,  342,   39,  352,  342,   34,   55,   77,  230,  373,
 /*   200 */   376,  370,  370,   65,   64,   64,   64,   64,  412,   66,
 /*   210 */    66,   66,   66,   71,   71,   72,   72,   72,   73,  194,
 /*   220 */   299,  289,  222,  181,  198,  197,  128,  268,   65,  378,
 /*   230 */   378,  320,  321,  150,  243,  176,  251,  255,  259,  227,
 /*   240 */   403,  342,   37,    1,  150,  260,  289,  251,  255,  259,
 /*   250 */   289,   72,   72,   72,   73,  365,  260,  721,  104,  302,
 /*   260 */   438,  343,    2,  319,  399,  250,  342,   32,  463,  364,
 /*   270 */   342,   37,   55,   77,  230,  373,  376,  370,  370,   65,
 /*   280 */    64,   64,   64,   64,  224,   66,   66,   66,   66,   71,
 /*   290 */    71,   72,   72,   72,   73,  222,  433,  289,  384,  229,
 /*   300 */   346,   65,  129,  405,  437,  174,  180,  319,  358,  358,
 /*   310 */   448,  319,   19,  241,  258,  106,  462,  342,   23,  285,
 /*   320 */   460,   70,  289,  164,  162,  127,  254,  384,  365,  298,
 /*   330 */   348,  348,  348,  146,  320,  321,  399,  343,  268,  319,
 /*   340 */   378,  378,  342,   35,  140,   55,   77,  230,  373,  376,
 /*   350 */   370,  370,   65,   64,   64,   64,   64,  205,   66,   66,
 /*   360 */    66,   66,   71,   71,   72,   72,   72,   73,  343,  289,
 /*   370 */   379,  222,  341,   10,  264,  190,  340,   65,  320,  321,
 /*   380 */   145,  125,  320,  321,  240,  277,  242,  355,  356,  342,
 /*   390 */    37,  293,  418,  319,  289,  223,  206,  288,  289,  445,
 /*   400 */   161,  413,  396,  319,  365,  273,  395,  444,  140,  142,
 /*   410 */   320,  321,  319,   57,  342,   34,  419,  207,  342,   39,
 /*   420 */    99,   55,   77,  230,  373,  376,  370,  370,   65,   64,
 /*   430 */    64,   64,   64,  431,   66,   66,   66,   66,   71,   71,
 /*   440 */    72,   72,   72,   73,  343,  361,  383,  222,  394,  246,
 /*   450 */   156,  183,   70,   65,  164,  173,  127,  304,  276,  223,
 /*   460 */   237,  355,  356,  172,  320,  321,  226,  407,  221,  225,
 /*   470 */   289,  362,  436,  130,  320,  321,  268,  291,  378,  378,
 /*   480 */   365,  318,  429,  320,  321,  371,  139,  144,  149,  426,
 /*   490 */   342,   28,  362,  249,  130,  440,  143,   55,   77,  230,
 /*   500 */   373,  376,  370,  370,   65,   64,   64,   64,   64,  294,
 /*   510 */    66,   66,   66,   66,   71,   71,   72,   72,   72,   73,
 /*   520 */   222,  126,  289,  123,  156,  138,   65,  289,  353,  289,
 /*   530 */   296,  289,  235,  341,  289,  105,  289,  340,  289,  392,
 /*   540 */   100,  289,  342,   26,  394,  289,   18,  342,   33,  342,
 /*   550 */    30,  342,   27,  365,  342,   31,  342,   44,  342,   43,
 /*   560 */   337,  342,   29,  417,  216,  342,   22,  424,  338,  402,
 /*   570 */    55,   77,  230,  373,  376,  370,  370,   65,   64,   64,
 /*   580 */    64,   64,  409,   66,   66,   66,   66,   71,   71,   72,
 /*   590 */    72,   72,   73,  289,  209,  289,  222,  289,  213,  289,
 /*   600 */   233,  289,   65,  289,  424,  289,  393,  182,  424,  503,
 /*   610 */   289,  424,  289,  342,   21,  342,   94,  342,   93,  342,
 /*   620 */    42,  342,   46,  342,   81,  342,   79,  162,   56,  365,
 /*   630 */   342,   83,  342,   25,  117,  184,  416,  239,   51,  424,
 /*   640 */   201,  231,  325,  326,  167,  214,   55,   77,  230,  373,
 /*   650 */   376,  370,  370,   65,   64,   64,   64,   64,  289,   66,
 /*   660 */    66,   66,   66,   71,   71,   72,   72,   72,   73,  222,
 /*   670 */   289,  107,   96,  289,  178,   65,  289,  184,  342,   38,
 /*   680 */   289,  208,  214,  289,  292,  289,  214,  289,  415,  214,
 /*   690 */   342,   82,  289,  342,   16,  303,  342,   84,    2,   99,
 /*   700 */   342,   91,  365,  342,   92,  342,   24,  342,   41,  141,
 /*   710 */   250,  352,  342,   40,  152,  250,  179,  195,  399,   55,
 /*   720 */    77,  230,  373,  376,  370,  370,   65,   64,   64,   64,
 /*   730 */    64,  289,   66,   66,   66,   66,   71,   71,   72,   72,
 /*   740 */    72,   73,  222,  382,  289,  263,  184,  334,   65,  343,
 /*   750 */   435,  342,   45,  137,  457,  461,  335,  202,  162,  430,
 /*   760 */   403,   15,  422,   99,  342,   36,  458,  414,  401,  122,
 /*   770 */   398,  187,   76,  131,  282,  365,  155,  184,  184,  168,
 /*   780 */    99,  420,  352,   47,   76,  267,  131,  134,  343,  136,
 /*   790 */   125,    6,  191,   77,  230,  373,  376,  370,  370,   65,
 /*   800 */    64,   64,   64,   64,  343,   66,   66,   66,   66,   71,
 /*   810 */    71,   72,   72,   72,   73,  222,  452,  185,  347,  238,
 /*   820 */   421,   65,  343,  439,  385,  199,  375,  212,  391,  423,
 /*   830 */   357,  413,  312,  359,  193,  116,  425,  270,  257,  253,
 /*   840 */   215,   59,  388,  389,  427,  345,  428,  228,  365,  175,
 /*   850 */     8,  169,  390,  455,  344,   56,  194,  299,   98,  157,
 /*   860 */   181,  198,  197,    9,  103,  177,  284,  230,  373,  376,
 /*   870 */   370,  370,   65,   64,   64,   64,   64,    4,   66,   66,
 /*   880 */    66,   66,   71,   71,   72,   72,   72,   73,  308,  310,
 /*   890 */   333,  315,   63,  261,  271,  151,  453,  232,  108,    4,
 /*   900 */   295,   11,  111,  287,  244,  463,  324,  265,  404,   68,
 /*   910 */   281,  189,  449,  159,   63,  261,  279,  147,  336,  232,
 /*   920 */   307,  339,  451,  311,  269,   87,  219,  400,  283,  245,
 /*   930 */   200,  170,  281,   52,   53,  203,  109,  443,  442,  300,
 /*   940 */   166,  364,  441,  218,  204,  332,  456,  165,  210,  256,
 /*   950 */   236,  258,  106,  462,   49,  386,  285,  158,   85,   60,
 /*   960 */    61,  162,   80,  364,  354,   14,   97,   63,  248,  247,
 /*   970 */     5,  217,  346,  505,  389,  504,  363,   62,  252,   89,
 /*   980 */   397,   60,   61,  406,  211,   86,  314,  102,  162,   63,
 /*   990 */   248,  247,  381,   58,  346,  262,  114,   88,  182,  153,
 /*  1000 */   110,   67,  348,  348,  348,  349,  350,  351,   12,  154,
 /*  1010 */   113,  377,  192,   13,  316,  372,  465,  163,  364,  275,
 /*  1020 */   101,  280,  464,  450,  348,  348,  348,  349,  350,  351,
 /*  1030 */    12,    4,  459,   74,  124,  138,  118,  119,  220,  186,
 /*  1040 */   272,  306,  132,   78,  148,  433,   63,  261,  411,  346,
 /*  1050 */   112,  232,  454,    4,  309,  297,    7,  485,  313,  485,
 /*  1060 */    15,  485,  485,  485,  281,  485,  485,  485,   63,  261,
 /*  1070 */   485,  485,  485,  232,  485,  485,  485,  485,  485,  348,
 /*  1080 */   348,  348,  485,  485,  485,  485,  281,  485,  485,  485,
 /*  1090 */   485,  485,  485,  485,  485,  364,  485,  485,  485,  485,
 /*  1100 */   485,  485,  485,  485,  485,  485,  485,  485,  485,  485,
 /*  1110 */   485,  485,  485,   60,   61,  485,  485,  364,  485,  485,
 /*  1120 */   485,   63,  248,  247,  485,  485,  346,  485,  485,  485,
 /*  1130 */   485,  485,  485,  485,  485,   60,   61,  485,  485,  485,
 /*  1140 */   485,  485,  485,   63,  248,  247,  485,  485,  346,  485,
 /*  1150 */   485,  485,  485,  485,  485,  485,  348,  348,  348,  349,
 /*  1160 */   350,  351,   12,  485,  485,  485,  485,  485,  485,  485,
 /*  1170 */   485,  485,  485,  485,  485,  485,  485,  485,  348,  348,
 /*  1180 */   348,  349,  350,  351,   12,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    16,   83,   84,   19,  167,   21,   22,   74,   75,   76,
 /*    10 */    77,  174,   79,   80,   81,   82,   83,   84,   85,   86,
 /*    20 */    87,   88,   78,   79,   80,   81,   82,   83,   84,   85,
 /*    30 */    86,   87,   88,   49,   79,   80,   81,   82,   83,   84,
 /*    40 */    85,   86,   87,   88,   83,   84,   85,   86,   87,   88,
 /*    50 */    66,   67,   68,   69,   70,   71,   72,   73,   74,   75,
 /*    60 */    76,   77,    2,   79,   80,   81,   82,   83,   84,   85,
 /*    70 */    86,   87,   88,   16,   12,   16,   92,   20,    2,   22,
 /*    80 */    20,   56,   23,   92,  148,   26,   16,  112,   49,   27,
 /*    90 */   154,  148,  156,  217,  218,  219,   26,  221,   88,  148,
 /*   100 */   140,  141,    1,   20,  144,   43,   49,   45,   69,   70,
 /*   110 */    53,  168,  169,  153,   21,  179,  180,  126,   56,  168,
 /*   120 */   169,    2,   21,   66,   67,   68,   69,   70,   71,   72,
 /*   130 */    73,   74,   75,   76,   77,   96,   79,   80,   81,   82,
 /*   140 */    83,   84,   85,   86,   87,   88,   16,    0,  188,   19,
 /*   150 */     3,    4,   22,   94,   95,   96,   97,   98,   99,  100,
 /*   160 */    21,  133,  211,  135,  105,   95,   96,   97,   98,   99,
 /*   170 */   100,  148,    1,  213,  148,  105,  225,   94,  180,   49,
 /*   180 */    97,   98,   99,  168,  108,   92,  110,  111,  168,  106,
 /*   190 */    19,  168,  169,   92,  168,  169,   66,   67,   68,   69,
 /*   200 */    70,   71,   72,   73,   74,   75,   76,   77,    2,   79,
 /*   210 */    80,   81,   82,   83,   84,   85,   86,   87,   88,    7,
 /*   220 */     8,  148,   16,   11,   12,   13,  153,  108,   22,  110,
 /*   230 */   111,   92,   93,   94,  208,  209,   97,   98,   99,  216,
 /*   240 */   148,  168,  169,    1,   94,  106,  148,   97,   98,   99,
 /*   250 */   148,   85,   86,   87,   88,   49,  106,  137,  138,  139,
 /*   260 */   180,  188,  142,   21,  160,  148,  168,  169,   56,   65,
 /*   270 */   168,  169,   66,   67,   68,   69,   70,   71,   72,   73,
 /*   280 */    74,   75,   76,   77,  211,   79,   80,   81,   82,   83,
 /*   290 */    84,   85,   86,   87,   88,   16,   92,  148,  225,  207,
 /*   300 */    96,   22,  153,  199,  148,  189,  150,   21,  126,  127,
 /*   310 */   154,   21,  156,  211,  102,  103,  104,  168,  169,  107,
 /*   320 */     2,  217,  148,  219,  112,  221,  148,  225,   49,  212,
 /*   330 */   126,  127,  128,  153,   92,   93,  160,  188,  108,   21,
 /*   340 */   110,  111,  168,  169,   50,   66,   67,   68,   69,   70,
 /*   350 */    71,   72,   73,   74,   75,   76,   77,    2,   79,   80,
 /*   360 */    81,   82,   83,   84,   85,   86,   87,   88,  188,  148,
 /*   370 */     2,   16,  109,    1,   16,  199,  113,   22,   92,   93,
 /*   380 */   200,  201,   92,   93,   25,  205,  163,  164,  165,  168,
 /*   390 */   169,  148,   33,   21,  148,  101,  222,  141,  148,  174,
 /*   400 */   144,  176,  116,   21,   49,   16,  116,  182,   50,  153,
 /*   410 */    92,   93,   21,  134,  168,  169,   57,  148,  168,  169,
 /*   420 */    20,   66,   67,   68,   69,   70,   71,   72,   73,   74,
 /*   430 */    75,   76,   77,    2,   79,   80,   81,   82,   83,   84,
 /*   440 */    85,   86,   87,   88,  188,  160,  225,   16,   20,   24,
 /*   450 */   227,   26,  217,   22,  219,  209,  221,    3,    4,  101,
 /*   460 */   163,  164,  165,  105,   92,   93,  216,   85,  157,  213,
 /*   470 */   148,  160,  161,  162,   92,   93,  108,   66,  110,  111,
 /*   480 */    49,  166,  167,   92,   93,   96,  102,  103,  104,  174,
 /*   490 */   168,  169,  160,  161,  162,   95,    1,   66,   67,   68,
 /*   500 */    69,   70,   71,   72,   73,   74,   75,   76,   77,   85,
 /*   510 */    79,   80,   81,   82,   83,   84,   85,   86,   87,   88,
 /*   520 */    16,   93,  148,  148,  227,   20,   22,  148,    2,  148,
 /*   530 */    21,  148,   23,  109,  148,  124,  148,  113,  148,   20,
 /*   540 */    25,  148,  168,  169,  116,  148,   20,  168,  169,  168,
 /*   550 */   169,  168,  169,   49,  168,  169,  168,  169,  168,  169,
 /*   560 */    30,  168,  169,   23,  148,  168,  169,  148,   38,  148,
 /*   570 */    66,   67,   68,   69,   70,   71,   72,   73,   74,   75,
 /*   580 */    76,   77,    2,   79,   80,   81,   82,   83,   84,   85,
 /*   590 */    86,   87,   88,  148,  148,  148,   16,  148,   96,  148,
 /*   600 */   181,  148,   22,  148,  148,  148,  202,  105,  148,  114,
 /*   610 */   148,  148,  148,  168,  169,  168,  169,  168,  169,  168,
 /*   620 */   169,  168,  169,  168,  169,  168,  169,  112,  123,   49,
 /*   630 */   168,  169,  168,  169,  115,  148,   96,  181,  134,  148,
 /*   640 */   125,  181,  184,  185,  181,  226,   66,   67,   68,   69,
 /*   650 */    70,   71,   72,   73,   74,   75,   76,   77,  148,   79,
 /*   660 */    80,   81,   82,   83,   84,   85,   86,   87,   88,   16,
 /*   670 */   148,   25,  148,  148,  187,   22,  148,  148,  168,  169,
 /*   680 */   148,    2,  226,  148,  148,  148,  226,  148,  177,  226,
 /*   690 */   168,  169,  148,  168,  169,  139,  168,  169,  142,   20,
 /*   700 */   168,  169,   49,  168,  169,  168,  169,  168,  169,   19,
 /*   710 */   148,   21,  168,  169,  153,  148,  187,  226,  160,   66,
 /*   720 */    67,   68,   69,   70,   71,   72,   73,   74,   75,   76,
 /*   730 */    77,  148,   79,   80,   81,   82,   83,   84,   85,   86,
 /*   740 */    87,   88,   16,    2,  148,    2,  148,   37,   22,  188,
 /*   750 */   159,  168,  169,  153,    2,    2,   46,  199,  112,  168,
 /*   760 */   148,   20,    2,   20,  168,  169,    2,  148,    2,  153,
 /*   770 */   148,  125,   20,   20,  212,   49,  191,  148,  148,  212,
 /*   780 */    20,  177,   92,  198,   20,  187,   20,  153,  188,  200,
 /*   790 */   201,  190,  148,   67,   68,   69,   70,   71,   72,   73,
 /*   800 */    74,   75,   76,   77,  188,   79,   80,   81,   82,   83,
 /*   810 */    84,   85,   86,   87,   88,   16,  187,  187,  148,  207,
 /*   820 */   177,   22,  188,  148,  148,  148,  220,  192,  171,  171,
 /*   830 */   228,  176,  145,  228,  171,  190,  171,  220,  224,  175,
 /*   840 */   193,  122,  171,   21,  159,  197,  178,   47,   49,   26,
 /*   850 */     1,  210,  159,  197,  188,  123,    7,    8,    1,  210,
 /*   860 */    11,   12,   13,   20,  214,  193,  151,   68,   69,   70,
 /*   870 */    71,   72,   73,   74,   75,   76,   77,    1,   79,   80,
 /*   880 */    81,   82,   83,   84,   85,   86,   87,   88,   10,  143,
 /*   890 */   178,   22,   16,   17,  155,  114,  149,   21,  214,    1,
 /*   900 */    15,    1,  186,  215,  101,   56,  170,  117,  204,  101,
 /*   910 */    34,  203,  149,  210,   16,   17,   15,  147,  170,   21,
 /*   920 */   143,  170,   18,  143,  146,    1,  172,  204,  215,  118,
 /*   930 */   203,  196,   34,  121,  133,  195,  149,  170,  170,  120,
 /*   940 */   223,   65,  170,  172,  194,  170,  197,  210,  193,  155,
 /*   950 */   152,  102,  103,  104,    1,  149,  107,  173,  173,   83,
 /*   960 */    84,  112,  158,   65,    2,   20,   35,   91,   92,   93,
 /*   970 */   119,   25,   96,  114,   21,  114,    2,   22,   51,    1,
 /*   980 */   116,   83,   84,   85,    2,    1,   21,  115,  112,   91,
 /*   990 */    92,   93,   41,    1,   96,   51,    1,    1,  105,  100,
 /*  1000 */     1,    1,  126,  127,  128,  129,  130,  131,  132,   67,
 /*  1010 */    25,  109,   14,    1,   21,   96,    6,   51,   65,    5,
 /*  1020 */     2,   16,    3,   17,  126,  127,  128,  129,  130,  131,
 /*  1030 */   132,    1,    2,    1,  114,   20,   83,   84,    2,   14,
 /*  1040 */    20,    2,  114,   22,  114,   92,   16,   17,    2,   96,
 /*  1050 */   124,   21,   17,    1,    9,   22,  119,  229,   21,  229,
 /*  1060 */    20,  229,  229,  229,   34,  229,  229,  229,   16,   17,
 /*  1070 */   229,  229,  229,   21,  229,  229,  229,  229,  229,  126,
 /*  1080 */   127,  128,  229,  229,  229,  229,   34,  229,  229,  229,
 /*  1090 */   229,  229,  229,  229,  229,   65,  229,  229,  229,  229,
 /*  1100 */   229,  229,  229,  229,  229,  229,  229,  229,  229,  229,
 /*  1110 */   229,  229,  229,   83,   84,  229,  229,   65,  229,  229,
 /*  1120 */   229,   91,   92,   93,  229,  229,   96,  229,  229,  229,
 /*  1130 */   229,  229,  229,  229,  229,   83,   84,  229,  229,  229,
 /*  1140 */   229,  229,  229,   91,   92,   93,  229,  229,   96,  229,
 /*  1150 */   229,  229,  229,  229,  229,  229,  126,  127,  128,  129,
 /*  1160 */   130,  131,  132,  229,  229,  229,  229,  229,  229,  229,
 /*  1170 */   229,  229,  229,  229,  229,  229,  229,  229,  126,  127,
 /*  1180 */   128,  129,  130,  131,  132,
};
#define YY_SHIFT_USE_DFLT (-83)
#define YY_SHIFT_MAX 301
static const short yy_shift_ofst[] = {
 /*     0 */   454,  876,  849,  -16,  876, 1052, 1052, 1052,  212,  139,
 /*    10 */   -25, 1030, 1052, 1052, 1052, 1052,  -56,  391,  -82,   93,
 /*    20 */   -82,   57,  355,  580,  130,  206,  279,  504,  431,  653,
 /*    30 */   653,  653,  653,  653,  653,  653,  653,  653,  653,  653,
 /*    40 */   653,  653,  653,  653,  726,  799,  799,  898, 1052, 1052,
 /*    50 */  1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052,
 /*    60 */  1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052,
 /*    70 */  1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052, 1052,  -67,
 /*    80 */    59,  -45,  -45,  -39,  166,  358,  391,  391,  391,  391,
 /*    90 */    93,   10,  -83,  -83,  -83,  953,   70,   62,  318,  391,
 /*   100 */   391,  690,  391,  515,  147,  391,  391,  391,  646,  690,
 /*   110 */   391,  391,  391,  391,  -25,  -25,  -83,  -83,  204,  204,
 /*   120 */    83,  150,   76,  286,  382,  372,  290,  242,  368,  119,
 /*   130 */   101,  391,  391,  391,  230,  391,  428,  230,  391,  359,
 /*   140 */   391,  391,  230,  424,  359,  428,  230,  391,  391,  359,
 /*   150 */   391,  391,  230,  391,   -9,  519,  182,  530,  294,  530,
 /*   160 */   182,  171,  263,  530,   -9,  530,   28,  400,  505,  530,
 /*   170 */   719,  822,  800,  823,  -25,  822,  823,  719,  732,  857,
 /*   180 */   843,  878,  800,  869,  781,  857,  885,  900,  803,  790,
 /*   190 */   808,  781,  901,  803,  878,  823,  803,  904,  878,  924,
 /*   200 */   790,  900,  808,  811,  812,  803,  801,  781,  803,  924,
 /*   210 */   819,  803,  732,  803,  823,  719,  781,  869,  -83,  -83,
 /*   220 */   -83,  -83,   39,  384,  741,  411,  752,  764,  710,  753,
 /*   230 */   389,  743,  495,  679,  502,  425,  509,  526,  766,  760,
 /*   240 */   540,   60,  962,  945,  931,  851,  946,  859,  861,  974,
 /*   250 */   955,  927,  978,  982,  864,  984,  965,  951,  872,  992,
 /*   260 */   944,  995,  996,  899,  893,  999, 1000,  985,  902,  998,
 /*   270 */   942,  993, 1012,  919,  966, 1010, 1014, 1018, 1019, 1005,
 /*   280 */  1006, 1032, 1015, 1020, 1036, 1025,  920, 1020, 1039,  928,
 /*   290 */   926,   25, 1021,  930, 1046, 1035, 1033, 1037, 1015, 1045,
 /*   300 */   937, 1040,
};
#define YY_REDUCE_USE_DFLT (-164)
#define YY_REDUCE_MAX 221
static const short yy_reduce_ofst[] = {
 /*     0 */   120,   73,  -40,  104,  149,   26,  102,  -49,  256,  -64,
 /*    10 */   180,   23,  174,  250,  246,  221, -124,  156,  223,  311,
 /*    20 */   297,  235,  235,  235,  235,  235,  235,  235,  235,  235,
 /*    30 */   235,  235,  235,  235,  235,  235,  235,  235,  235,  235,
 /*    40 */   235,  235,  235,  235,  235,  235,  235,  -57,   98,  322,
 /*    50 */   374,  379,  381,  383,  386,  388,  390,  393,  397,  445,
 /*    60 */   447,  449,  451,  453,  455,  457,  462,  464,  510,  522,
 /*    70 */   525,  528,  532,  535,  537,  539,  544,  583,  596,  235,
 /*    80 */   315,  235,  235,  235,  235,  225,  419,  456,  460,  463,
 /*    90 */   332,  235,  235,  235,  235,  591, -163,  458,   92,  491,
 /*   100 */   117,  176,  487,  561,  556,  529,  598,  562,  600,  558,
 /*   110 */   612,  629,  630,  567,  616,  634,  585,  589,   15,   20,
 /*   120 */    -2,   80,  116,  178,  243,  269,  375,  416,  116,  116,
 /*   130 */   285,  421,  243,  446,  116,  524,  404,  116,  536,  511,
 /*   140 */   619,  622,  116,  601,  604,  404,  116,  644,  670,  643,
 /*   150 */   675,  676,  116,  677,  606,  635,  602,  657,  655,  658,
 /*   160 */   605,  687,  645,  663,  617,  665,  614,  664,  647,  671,
 /*   170 */   648,  685,  668,  641,  666,  693,  649,  656,  672,  650,
 /*   180 */   715,  746,  712,  739,  747,  684,  716,  688,  736,  704,
 /*   190 */   708,  763,  770,  748,  777,  703,  751,  778,  780,  754,
 /*   200 */   723,  713,  727,  735,  740,  767,  717,  787,  768,  771,
 /*   210 */   750,  772,  755,  775,  737,  749,  806,  794,  784,  785,
 /*   220 */   798,  804,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   473,  706,  720,  587,  720,  720,  706,  706,  720,  720,
 /*    10 */   591,  720,  702,  720,  720,  706,  676,  720,  716,  506,
 /*    20 */   716,  622,  720,  720,  720,  720,  720,  720,  720,  697,
 /*    30 */   620,  699,  624,  698,  611,  701,  629,  705,  603,  643,
 /*    40 */   642,  623,  630,  627,  666,  665,  682,  720,  720,  720,
 /*    50 */   720,  720,  720,  720,  720,  720,  720,  720,  720,  720,
 /*    60 */   720,  720,  720,  720,  720,  720,  720,  720,  720,  720,
 /*    70 */   720,  720,  720,  720,  720,  720,  720,  720,  720,  668,
 /*    80 */   499,  667,  675,  669,  670,  560,  720,  720,  720,  720,
 /*    90 */   720,  671,  672,  683,  684,  720,  720,  720,  720,  720,
 /*   100 */   720,  587,  720,  720,  473,  720,  720,  720,  720,  587,
 /*   110 */   720,  720,  720,  720,  720,  720,  581,  591,  720,  720,
 /*   120 */   551,  720,  720,  720,  720,  720,  720,  720,  720,  720,
 /*   130 */   508,  720,  720,  720,  489,  720,  589,  636,  720,  720,
 /*   140 */   720,  720,  570,  579,  720,  595,  594,  720,  720,  720,
 /*   150 */   720,  720,  633,  720,  720,  588,  720,  614,  530,  614,
 /*   160 */   720,  720,  579,  614,  720,  614,  700,  533,  626,  614,
 /*   170 */   621,  720,  547,  615,  720,  720,  615,  621,  626,  644,
 /*   180 */   550,  478,  547,  497,  596,  644,  569,  720,  562,  606,
 /*   190 */   604,  596,  484,  562,  478,  615,  562,  487,  478,  707,
 /*   200 */   606,  720,  604,  607,  619,  562,  720,  596,  562,  707,
 /*   210 */   617,  562,  626,  562,  615,  621,  596,  497,  535,  535,
 /*   220 */   492,  516,  720,  720,  720,  720,  720,  720,  720,  720,
 /*   230 */   720,  720,  652,  720,  720,  720,  488,  720,  720,  720,
 /*   240 */   720,  720,  720,  608,  720,  720,  720,  653,  657,  720,
 /*   250 */   720,  720,  720,  720,  720,  720,  720,  720,  720,  720,
 /*   260 */   720,  720,  720,  720,  720,  720,  720,  720,  573,  720,
 /*   270 */   720,  720,  720,  720,  720,  720,  474,  720,  720,  720,
 /*   280 */   720,  720,  635,  631,  720,  720,  720,  634,  720,  720,
 /*   290 */   720,  720,  720,  654,  720,  720,  493,  720,  632,  720,
 /*   300 */   720,  618,  468,  467,  471,  469,  470,  476,  479,  477,
 /*   310 */   480,  481,  482,  494,  495,  498,  496,  490,  515,  503,
 /*   320 */   504,  505,  517,  524,  525,  563,  564,  565,  566,  717,
 /*   330 */   718,  719,  526,  545,  548,  549,  527,  612,  613,  528,
 /*   340 */   577,  578,  649,  571,  572,  576,  651,  655,  656,  658,
 /*   350 */   659,  660,  502,  509,  510,  513,  514,  712,  714,  713,
 /*   360 */   715,  512,  511,  661,  664,  673,  674,  680,  686,  690,
 /*   370 */   678,  679,  681,  685,  687,  688,  689,  574,  575,  693,
 /*   380 */   695,  696,  691,  703,  704,  597,  694,  677,  609,  501,
 /*   390 */   616,  610,  580,  590,  599,  600,  601,  602,  585,  586,
 /*   400 */   592,  605,  647,  648,  593,  582,  583,  584,  692,  650,
 /*   410 */   662,  663,  529,  536,  537,  538,  541,  542,  543,  544,
 /*   420 */   539,  540,  708,  709,  711,  710,  531,  532,  546,  518,
 /*   430 */   519,  520,  521,  657,  522,  523,  507,  500,  552,  555,
 /*   440 */   534,  556,  557,  558,  559,  561,  553,  554,  491,  483,
 /*   450 */   485,  486,  567,  598,  568,  625,  628,  639,  640,  641,
 /*   460 */   645,  646,  637,  638,  472,  475,
};
#define YY_SZ_ACTTAB (sizeof(yy_action)/sizeof(yy_action[0]))

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
**
**      %fallback ID X Y Z.
**
** appears in the grammer, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*       SEMI => nothing */
   21,  /*    EXPLAIN => ID */
   21,  /*      QUERY => ID */
   21,  /*       PLAN => ID */
   21,  /*      BEGIN => ID */
    0,  /*      START => nothing */
    0,  /* TRANSACTION => nothing */
    0,  /*       WORK => nothing */
    0,  /*     COMMIT => nothing */
    0,  /*   ROLLBACK => nothing */
    0,  /*     CREATE => nothing */
    0,  /*      TABLE => nothing */
   21,  /*         IF => ID */
    0,  /*        NOT => nothing */
    0,  /*     EXISTS => nothing */
   21,  /*       TEMP => ID */
    0,  /*         AS => nothing */
    0,  /*      COMMA => nothing */
    0,  /*         ID => nothing */
    0,  /*         EQ => nothing */
    0,  /*    DEFAULT => nothing */
    0,  /*    CHARSET => nothing */
    0,  /*        SET => nothing */
    0,  /*    COLLATE => nothing */
   21,  /*      ABORT => ID */
   21,  /*      AFTER => ID */
   21,  /*    ANALYZE => ID */
   21,  /*        ASC => ID */
   21,  /*     ATTACH => ID */
   21,  /*     BEFORE => ID */
   21,  /*    CASCADE => ID */
   21,  /*       CAST => ID */
   21,  /*   CONFLICT => ID */
   21,  /*   DATABASE => ID */
   21,  /*   DEFERRED => ID */
   21,  /*       DESC => ID */
   21,  /*     DETACH => ID */
   21,  /*       EACH => ID */
   21,  /*        END => ID */
   21,  /*  EXCLUSIVE => ID */
   21,  /*       FAIL => ID */
   21,  /*        FOR => ID */
   21,  /*     IGNORE => ID */
   21,  /*  IMMEDIATE => ID */
   21,  /*  INITIALLY => ID */
   21,  /*    INSTEAD => ID */
   21,  /*    LIKE_KW => ID */
   21,  /*      MATCH => ID */
   21,  /*        KEY => ID */
   21,  /*         OF => ID */
   21,  /*     OFFSET => ID */
   21,  /*     PRAGMA => ID */
   21,  /*      RAISE => ID */
   21,  /*    REPLACE => ID */
   21,  /*   RESTRICT => ID */
   21,  /*        ROW => ID */
   21,  /*  STATEMENT => ID */
   21,  /*    TRIGGER => ID */
   21,  /*     VACUUM => ID */
   21,  /*       VIEW => ID */
   21,  /*    REINDEX => ID */
   21,  /*     RENAME => ID */
   21,  /*   CTIME_KW => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*         IS => nothing */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
    0,  /*     ISNULL => nothing */
    0,  /*    NOTNULL => nothing */
    0,  /*         NE => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*     UMINUS => nothing */
    0,  /*      UPLUS => nothing */
    0,  /*     BITNOT => nothing */
    0,  /*     STRING => nothing */
    0,  /*    JOIN_KW => nothing */
    0,  /* CONSTRAINT => nothing */
    0,  /*   AUTOINCR => nothing */
    0,  /*       NULL => nothing */
    0,  /*    PRIMARY => nothing */
    0,  /*     UNIQUE => nothing */
    0,  /*      CHECK => nothing */
    0,  /* REFERENCES => nothing */
    0,  /*         ON => nothing */
    0,  /*     DELETE => nothing */
    0,  /*     UPDATE => nothing */
    0,  /*     INSERT => nothing */
    0,  /* DEFERRABLE => nothing */
    0,  /*    FOREIGN => nothing */
    0,  /*       DROP => nothing */
    0,  /*      UNION => nothing */
    0,  /*        ALL => nothing */
    0,  /*     EXCEPT => nothing */
    0,  /*  INTERSECT => nothing */
    0,  /*     SELECT => nothing */
    0,  /*   DISTINCT => nothing */
    0,  /*        DOT => nothing */
    0,  /*       FROM => nothing */
    0,  /*       JOIN => nothing */
    0,  /*      USING => nothing */
    0,  /*      ORDER => nothing */
    0,  /*         BY => nothing */
    0,  /*      GROUP => nothing */
    0,  /*     HAVING => nothing */
    0,  /*      LIMIT => nothing */
    0,  /*      WHERE => nothing */
    0,  /*       INTO => nothing */
    0,  /*     VALUES => nothing */
    0,  /*    INTEGER => nothing */
    0,  /*      FLOAT => nothing */
    0,  /*       BLOB => nothing */
    0,  /*   REGISTER => nothing */
    0,  /*   VARIABLE => nothing */
    0,  /*  VARIABLE1 => nothing */
    0,  /*       CASE => nothing */
    0,  /*       WHEN => nothing */
    0,  /*       THEN => nothing */
    0,  /*       ELSE => nothing */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  int stateno;       /* The state-number */
  int major;         /* The major token value.  This is the code
                     ** number for the token at this stack level */
  YYMINORTYPE minor; /* The user-supplied minor token value.  This
                     ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
  int yyerrcnt;                 /* Shifts left before out of the error */
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/*
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = {
  "$",             "LP",            "RP",            "SEMI",
  "EXPLAIN",       "QUERY",         "PLAN",          "BEGIN",
  "START",         "TRANSACTION",   "WORK",          "COMMIT",
  "ROLLBACK",      "CREATE",        "TABLE",         "IF",
  "NOT",           "EXISTS",        "TEMP",          "AS",
  "COMMA",         "ID",            "EQ",            "DEFAULT",
  "CHARSET",       "SET",           "COLLATE",       "ABORT",
  "AFTER",         "ANALYZE",       "ASC",           "ATTACH",
  "BEFORE",        "CASCADE",       "CAST",          "CONFLICT",
  "DATABASE",      "DEFERRED",      "DESC",          "DETACH",
  "EACH",          "END",           "EXCLUSIVE",     "FAIL",
  "FOR",           "IGNORE",        "IMMEDIATE",     "INITIALLY",
  "INSTEAD",       "LIKE_KW",       "MATCH",         "KEY",
  "OF",            "OFFSET",        "PRAGMA",        "RAISE",
  "REPLACE",       "RESTRICT",      "ROW",           "STATEMENT",
  "TRIGGER",       "VACUUM",        "VIEW",          "REINDEX",
  "RENAME",        "CTIME_KW",      "OR",            "AND",
  "IS",            "BETWEEN",       "IN",            "ISNULL",
  "NOTNULL",       "NE",            "GT",            "LE",
  "LT",            "GE",            "ESCAPE",        "BITAND",
  "BITOR",         "LSHIFT",        "RSHIFT",        "PLUS",
  "MINUS",         "STAR",          "SLASH",         "REM",
  "CONCAT",        "UMINUS",        "UPLUS",         "BITNOT",
  "STRING",        "JOIN_KW",       "CONSTRAINT",    "AUTOINCR",
  "NULL",          "PRIMARY",       "UNIQUE",        "CHECK",
  "REFERENCES",    "ON",            "DELETE",        "UPDATE",
  "INSERT",        "DEFERRABLE",    "FOREIGN",       "DROP",
  "UNION",         "ALL",           "EXCEPT",        "INTERSECT",
  "SELECT",        "DISTINCT",      "DOT",           "FROM",
  "JOIN",          "USING",         "ORDER",         "BY",
  "GROUP",         "HAVING",        "LIMIT",         "WHERE",
  "INTO",          "VALUES",        "INTEGER",       "FLOAT",
  "BLOB",          "REGISTER",      "VARIABLE",      "VARIABLE1",
  "CASE",          "WHEN",          "THEN",          "ELSE",
  "error",         "input",         "cmdlist",       "ecmd",
  "cmdx",          "cmd",           "explain",       "trans_opt",
  "create_table",  "create_table_args",  "temp",          "ifnotexists",
  "nm",            "dbnm",          "columnlist",    "conslist_opt",
  "table_opt",     "select",        "column",        "eq_or_null",
  "columnid",      "type",          "carglist",      "id",
  "ids",           "typetoken",     "typename",      "signed",
  "plus_num",      "minus_num",     "carg",          "ccons",
  "term",          "expr",          "onconf",        "sortorder",
  "idxlist_opt",   "refargs",       "defer_subclause",  "autoinc",
  "refarg",        "refact",        "init_deferred_pred_opt",  "conslist",
  "tcons",         "idxlist",       "defer_subclause_opt",  "orconf",
  "resolvetype",   "raisetype",     "ifexists",      "fullname",
  "oneselect",     "multiselect_op",  "distinct",      "selcollist",
  "from",          "where_opt",     "groupby_opt",   "having_opt",
  "orderby_opt",   "limit_opt",     "sclp",          "as",
  "seltablist",    "stl_prefix",    "joinop",        "on_opt",
  "using_opt",     "seltablist_paren",  "joinop2",       "inscollist",
  "sortlist",      "sortitem",      "collate",       "exprlist",
  "setlist",       "insert_cmd",    "inscollist_opt",  "valueslist",
  "itemlist",      "likeop",        "escape",        "between_op",
  "between_elem",  "in_op",         "case_operand",  "case_exprlist",
  "case_else",     "expritem",      "idxitem",       "plus_opt",
  "number",
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "input ::= cmdlist",
 /*   1 */ "cmdlist ::= cmdlist ecmd",
 /*   2 */ "cmdlist ::= ecmd",
 /*   3 */ "cmdx ::= cmd",
 /*   4 */ "cmdx ::= LP cmd RP",
 /*   5 */ "ecmd ::= SEMI",
 /*   6 */ "ecmd ::= explain cmdx SEMI",
 /*   7 */ "explain ::=",
 /*   8 */ "explain ::= EXPLAIN",
 /*   9 */ "explain ::= EXPLAIN QUERY PLAN",
 /*  10 */ "cmd ::= BEGIN trans_opt",
 /*  11 */ "cmd ::= START TRANSACTION",
 /*  12 */ "trans_opt ::=",
 /*  13 */ "trans_opt ::= WORK",
 /*  14 */ "cmd ::= COMMIT trans_opt",
 /*  15 */ "cmd ::= ROLLBACK trans_opt",
 /*  16 */ "cmd ::= create_table create_table_args",
 /*  17 */ "create_table ::= CREATE temp TABLE ifnotexists nm dbnm",
 /*  18 */ "ifnotexists ::=",
 /*  19 */ "ifnotexists ::= IF NOT EXISTS",
 /*  20 */ "temp ::= TEMP",
 /*  21 */ "temp ::=",
 /*  22 */ "create_table_args ::= LP columnlist conslist_opt RP table_opt",
 /*  23 */ "create_table_args ::= AS select",
 /*  24 */ "columnlist ::= columnlist COMMA column",
 /*  25 */ "columnlist ::= column",
 /*  26 */ "table_opt ::=",
 /*  27 */ "table_opt ::= table_opt ID",
 /*  28 */ "table_opt ::= table_opt ID EQ ID",
 /*  29 */ "table_opt ::= table_opt DEFAULT CHARSET SET eq_or_null ID",
 /*  30 */ "table_opt ::= table_opt DEFAULT COLLATE eq_or_null ID",
 /*  31 */ "eq_or_null ::=",
 /*  32 */ "eq_or_null ::= EQ",
 /*  33 */ "column ::= columnid type carglist",
 /*  34 */ "columnid ::= nm",
 /*  35 */ "id ::= ID",
 /*  36 */ "ids ::= ID|STRING",
 /*  37 */ "nm ::= ID",
 /*  38 */ "nm ::= STRING",
 /*  39 */ "nm ::= JOIN_KW",
 /*  40 */ "type ::=",
 /*  41 */ "type ::= typetoken",
 /*  42 */ "typetoken ::= typename",
 /*  43 */ "typetoken ::= typename LP signed RP",
 /*  44 */ "typetoken ::= typename LP signed COMMA signed RP",
 /*  45 */ "typename ::= ids",
 /*  46 */ "typename ::= typename ids",
 /*  47 */ "signed ::= plus_num",
 /*  48 */ "signed ::= minus_num",
 /*  49 */ "carglist ::= carglist carg",
 /*  50 */ "carglist ::=",
 /*  51 */ "carg ::= CONSTRAINT nm ccons",
 /*  52 */ "carg ::= ccons",
 /*  53 */ "carg ::= DEFAULT term",
 /*  54 */ "carg ::= DEFAULT LP expr RP",
 /*  55 */ "carg ::= DEFAULT PLUS term",
 /*  56 */ "carg ::= DEFAULT MINUS term",
 /*  57 */ "carg ::= DEFAULT id",
 /*  58 */ "ccons ::= AUTOINCR",
 /*  59 */ "ccons ::= NULL onconf",
 /*  60 */ "ccons ::= NOT NULL onconf",
 /*  61 */ "ccons ::= PRIMARY KEY sortorder onconf",
 /*  62 */ "ccons ::= UNIQUE onconf",
 /*  63 */ "ccons ::= CHECK LP expr RP",
 /*  64 */ "ccons ::= REFERENCES nm idxlist_opt refargs",
 /*  65 */ "ccons ::= defer_subclause",
 /*  66 */ "ccons ::= COLLATE id",
 /*  67 */ "autoinc ::=",
 /*  68 */ "autoinc ::= AUTOINCR",
 /*  69 */ "refargs ::=",
 /*  70 */ "refargs ::= refargs refarg",
 /*  71 */ "refarg ::= MATCH nm",
 /*  72 */ "refarg ::= ON DELETE refact",
 /*  73 */ "refarg ::= ON UPDATE refact",
 /*  74 */ "refarg ::= ON INSERT refact",
 /*  75 */ "refact ::= SET NULL",
 /*  76 */ "refact ::= SET DEFAULT",
 /*  77 */ "refact ::= CASCADE",
 /*  78 */ "refact ::= RESTRICT",
 /*  79 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  80 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  81 */ "init_deferred_pred_opt ::=",
 /*  82 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  83 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  84 */ "conslist_opt ::=",
 /*  85 */ "conslist_opt ::= COMMA conslist",
 /*  86 */ "conslist ::= conslist COMMA tcons",
 /*  87 */ "conslist ::= conslist tcons",
 /*  88 */ "conslist ::= tcons",
 /*  89 */ "tcons ::= CONSTRAINT nm",
 /*  90 */ "tcons ::= PRIMARY KEY LP idxlist autoinc RP onconf",
 /*  91 */ "tcons ::= UNIQUE LP idxlist RP onconf",
 /*  92 */ "tcons ::= CHECK LP expr RP onconf",
 /*  93 */ "tcons ::= FOREIGN KEY LP idxlist RP REFERENCES nm idxlist_opt refargs defer_subclause_opt",
 /*  94 */ "defer_subclause_opt ::=",
 /*  95 */ "defer_subclause_opt ::= defer_subclause",
 /*  96 */ "onconf ::=",
 /*  97 */ "onconf ::= ON CONFLICT resolvetype",
 /*  98 */ "resolvetype ::= raisetype",
 /*  99 */ "resolvetype ::= IGNORE",
 /* 100 */ "resolvetype ::= REPLACE",
 /* 101 */ "cmd ::= DROP TABLE ifexists fullname",
 /* 102 */ "ifexists ::= IF EXISTS",
 /* 103 */ "ifexists ::=",
 /* 104 */ "cmd ::= select",
 /* 105 */ "select ::= oneselect",
 /* 106 */ "select ::= select multiselect_op oneselect",
 /* 107 */ "multiselect_op ::= UNION",
 /* 108 */ "multiselect_op ::= UNION ALL",
 /* 109 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /* 110 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /* 111 */ "distinct ::= DISTINCT",
 /* 112 */ "distinct ::= ALL",
 /* 113 */ "distinct ::=",
 /* 114 */ "sclp ::= selcollist COMMA",
 /* 115 */ "sclp ::=",
 /* 116 */ "selcollist ::= sclp expr as",
 /* 117 */ "selcollist ::= sclp STAR",
 /* 118 */ "selcollist ::= sclp nm DOT STAR",
 /* 119 */ "as ::= AS nm",
 /* 120 */ "as ::= ids",
 /* 121 */ "as ::=",
 /* 122 */ "from ::=",
 /* 123 */ "from ::= FROM seltablist",
 /* 124 */ "stl_prefix ::= seltablist joinop",
 /* 125 */ "stl_prefix ::=",
 /* 126 */ "seltablist ::= stl_prefix nm dbnm as on_opt using_opt",
 /* 127 */ "seltablist ::= stl_prefix LP seltablist_paren RP as on_opt using_opt",
 /* 128 */ "seltablist_paren ::= select",
 /* 129 */ "seltablist_paren ::= seltablist",
 /* 130 */ "dbnm ::=",
 /* 131 */ "dbnm ::= DOT nm",
 /* 132 */ "fullname ::= nm dbnm",
 /* 133 */ "joinop ::= COMMA|JOIN",
 /* 134 */ "joinop ::= JOIN_KW JOIN",
 /* 135 */ "joinop ::= JOIN_KW nm JOIN",
 /* 136 */ "joinop ::= JOIN_KW nm nm JOIN",
 /* 137 */ "on_opt ::= ON expr",
 /* 138 */ "on_opt ::=",
 /* 139 */ "using_opt ::= USING LP inscollist RP",
 /* 140 */ "using_opt ::=",
 /* 141 */ "orderby_opt ::=",
 /* 142 */ "orderby_opt ::= ORDER BY sortlist",
 /* 143 */ "sortlist ::= sortlist COMMA sortitem collate sortorder",
 /* 144 */ "sortlist ::= sortitem collate sortorder",
 /* 145 */ "sortitem ::= expr",
 /* 146 */ "sortorder ::= ASC",
 /* 147 */ "sortorder ::= DESC",
 /* 148 */ "sortorder ::=",
 /* 149 */ "collate ::=",
 /* 150 */ "collate ::= COLLATE id",
 /* 151 */ "groupby_opt ::=",
 /* 152 */ "groupby_opt ::= GROUP BY exprlist",
 /* 153 */ "having_opt ::=",
 /* 154 */ "having_opt ::= HAVING expr",
 /* 155 */ "limit_opt ::=",
 /* 156 */ "limit_opt ::= LIMIT expr",
 /* 157 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 158 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 159 */ "cmd ::= DELETE FROM fullname where_opt limit_opt",
 /* 160 */ "where_opt ::=",
 /* 161 */ "where_opt ::= WHERE expr",
 /* 162 */ "cmd ::= UPDATE fullname SET setlist where_opt limit_opt",
 /* 163 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 164 */ "setlist ::= nm EQ expr",
 /* 165 */ "cmd ::= insert_cmd INTO fullname inscollist_opt VALUES valueslist",
 /* 166 */ "cmd ::= insert_cmd INTO fullname inscollist_opt SET setlist",
 /* 167 */ "cmd ::= insert_cmd INTO fullname inscollist_opt select",
 /* 168 */ "cmd ::= insert_cmd OR REPLACE INTO fullname inscollist_opt VALUES valueslist",
 /* 169 */ "cmd ::= insert_cmd OR REPLACE INTO fullname inscollist_opt SET setlist",
 /* 170 */ "cmd ::= insert_cmd OR REPLACE INTO fullname inscollist_opt select",
 /* 171 */ "insert_cmd ::= INSERT",
 /* 172 */ "insert_cmd ::= REPLACE",
 /* 173 */ "valueslist ::= valueslist COMMA LP itemlist RP",
 /* 174 */ "valueslist ::= LP itemlist RP",
 /* 175 */ "valueslist ::= LP RP",
 /* 176 */ "itemlist ::= itemlist COMMA expr",
 /* 177 */ "itemlist ::= expr",
 /* 178 */ "inscollist_opt ::=",
 /* 179 */ "inscollist_opt ::= LP RP",
 /* 180 */ "inscollist_opt ::= LP inscollist RP",
 /* 181 */ "inscollist ::= inscollist COMMA nm",
 /* 182 */ "inscollist ::= nm",
 /* 183 */ "expr ::= term",
 /* 184 */ "expr ::= LP expr RP",
 /* 185 */ "term ::= NULL",
 /* 186 */ "expr ::= ID",
 /* 187 */ "expr ::= JOIN_KW",
 /* 188 */ "expr ::= nm DOT nm",
 /* 189 */ "expr ::= nm DOT nm DOT nm",
 /* 190 */ "term ::= INTEGER|FLOAT|BLOB",
 /* 191 */ "term ::= STRING",
 /* 192 */ "expr ::= REGISTER",
 /* 193 */ "expr ::= VARIABLE",
 /* 194 */ "expr ::= VARIABLE1",
 /* 195 */ "expr ::= CAST LP expr AS typetoken RP",
 /* 196 */ "expr ::= ID LP distinct exprlist RP",
 /* 197 */ "expr ::= ID LP STAR RP",
 /* 198 */ "term ::= CTIME_KW",
 /* 199 */ "expr ::= expr AND expr",
 /* 200 */ "expr ::= expr OR expr",
 /* 201 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 202 */ "expr ::= expr EQ|NE expr",
 /* 203 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 204 */ "expr ::= expr PLUS|MINUS expr",
 /* 205 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 206 */ "expr ::= expr CONCAT expr",
 /* 207 */ "likeop ::= LIKE_KW",
 /* 208 */ "likeop ::= NOT LIKE_KW",
 /* 209 */ "escape ::= ESCAPE expr",
 /* 210 */ "escape ::=",
 /* 211 */ "expr ::= expr likeop expr escape",
 /* 212 */ "expr ::= expr ISNULL|NOTNULL",
 /* 213 */ "expr ::= expr IS NULL",
 /* 214 */ "expr ::= expr NOT NULL",
 /* 215 */ "expr ::= expr IS NOT NULL",
 /* 216 */ "expr ::= NOT|BITNOT expr",
 /* 217 */ "expr ::= MINUS expr",
 /* 218 */ "expr ::= PLUS expr",
 /* 219 */ "between_op ::= BETWEEN",
 /* 220 */ "between_op ::= NOT BETWEEN",
 /* 221 */ "between_elem ::= INTEGER|STRING",
 /* 222 */ "expr ::= expr between_op between_elem AND between_elem",
 /* 223 */ "in_op ::= IN",
 /* 224 */ "in_op ::= NOT IN",
 /* 225 */ "expr ::= expr in_op LP exprlist RP",
 /* 226 */ "expr ::= LP select RP",
 /* 227 */ "expr ::= expr in_op LP select RP",
 /* 228 */ "expr ::= expr in_op nm dbnm",
 /* 229 */ "expr ::= EXISTS LP select RP",
 /* 230 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 231 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 232 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 233 */ "case_else ::= ELSE expr",
 /* 234 */ "case_else ::=",
 /* 235 */ "case_operand ::= expr",
 /* 236 */ "case_operand ::=",
 /* 237 */ "exprlist ::= exprlist COMMA expritem",
 /* 238 */ "exprlist ::= expritem",
 /* 239 */ "expritem ::= expr",
 /* 240 */ "expritem ::=",
 /* 241 */ "idxlist_opt ::=",
 /* 242 */ "idxlist_opt ::= LP idxlist RP",
 /* 243 */ "idxlist ::= idxlist COMMA idxitem collate sortorder",
 /* 244 */ "idxlist ::= idxitem collate sortorder",
 /* 245 */ "idxitem ::= nm",
 /* 246 */ "plus_num ::= plus_opt number",
 /* 247 */ "minus_num ::= MINUS number",
 /* 248 */ "number ::= INTEGER|FLOAT",
 /* 249 */ "plus_opt ::= PLUS",
 /* 250 */ "plus_opt ::=",
 /* 251 */ "raisetype ::= ROLLBACK",
 /* 252 */ "raisetype ::= ABORT",
 /* 253 */ "raisetype ::= FAIL",
};
#endif /* NDEBUG */

/*
** This function returns the symbolic name associated with a token
** value.
*/
const char *sqlite3ParserTokenName(int tokenType){
#ifndef NDEBUG
  if( tokenType>0 && tokenType<(sizeof(yyTokenName)/sizeof(yyTokenName[0])) ){
    return yyTokenName[tokenType];
  }else{
    return "Unknown";
  }
#else
  return "";
#endif
}

/*
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
void *sqlite3ParserAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(YYCODETYPE yymajor, YYMINORTYPE *yypminor){
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 153:
    case 188:
    case 205:
#line 391 "parse.y"
{sqlite3SelectDelete((yypminor->yy203));}
#line 1131 "parse.c"
      break;
    case 168:
    case 169:
    case 193:
    case 195:
    case 203:
    case 209:
    case 218:
    case 220:
    case 222:
    case 224:
    case 225:
#line 671 "parse.y"
{sqlite3ExprDelete((yypminor->yy396));}
#line 1146 "parse.c"
      break;
    case 172:
    case 181:
    case 191:
    case 194:
    case 196:
    case 198:
    case 208:
    case 211:
    case 212:
    case 216:
    case 223:
#line 915 "parse.y"
{sqlite3ExprListDelete((yypminor->yy162));}
#line 1161 "parse.c"
      break;
    case 187:
    case 192:
    case 200:
    case 201:
#line 519 "parse.y"
{sqlite3SrcListDelete((yypminor->yy41));}
#line 1169 "parse.c"
      break;
    case 197:
#line 580 "parse.y"
{
  sqlite3ExprDelete((yypminor->yy76).pLimit);
  sqlite3ExprDelete((yypminor->yy76).pOffset);
}
#line 1177 "parse.c"
      break;
    case 204:
    case 207:
    case 214:
#line 536 "parse.y"
{sqlite3IdListDelete((yypminor->yy306));}
#line 1184 "parse.c"
      break;
    case 215:
#line 642 "parse.y"
{sqlite3ValuesListDelete((yypminor->yy37));}
#line 1189 "parse.c"
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if( pParser->yyidx<0 ) return 0;
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor( yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/*
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from sqlite3ParserAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
  (*freeProc)((void*)pParser);
}

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;

  if( stateno>YY_SHIFT_MAX || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
    int iFallback;            /* Fallback token */
    if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
           && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
           yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
      }
#endif
      return yy_find_shift_action(pParser, iFallback);
    }
#endif
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  /* int stateno = pParser->yystack[pParser->yyidx].stateno; */

  if( stateno>YY_REDUCE_MAX ||
      (i = yy_reduce_ofst[stateno])==YY_REDUCE_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer ot the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
  if( yypParser->yyidx>=YYSTACKDEPTH ){
     sqlite3ParserARG_FETCH;
     yypParser->yyidx--;
#ifndef NDEBUG
     if( yyTraceFILE ){
       fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
     }
#endif
     while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
     /* Here code is inserted which will execute if the parser
     ** stack every overflows */
#line 43 "parse.y"

  sqlite3ErrorMsg(pParse, "parser stack overflow");
#line 1341 "parse.c"
     sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument var */
     return;
  }
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 137, 1 },
  { 138, 2 },
  { 138, 1 },
  { 140, 1 },
  { 140, 3 },
  { 139, 1 },
  { 139, 3 },
  { 142, 0 },
  { 142, 1 },
  { 142, 3 },
  { 141, 2 },
  { 141, 2 },
  { 143, 0 },
  { 143, 1 },
  { 141, 2 },
  { 141, 2 },
  { 141, 2 },
  { 144, 6 },
  { 147, 0 },
  { 147, 3 },
  { 146, 1 },
  { 146, 0 },
  { 145, 5 },
  { 145, 2 },
  { 150, 3 },
  { 150, 1 },
  { 152, 0 },
  { 152, 2 },
  { 152, 4 },
  { 152, 6 },
  { 152, 5 },
  { 155, 0 },
  { 155, 1 },
  { 154, 3 },
  { 156, 1 },
  { 159, 1 },
  { 160, 1 },
  { 148, 1 },
  { 148, 1 },
  { 148, 1 },
  { 157, 0 },
  { 157, 1 },
  { 161, 1 },
  { 161, 4 },
  { 161, 6 },
  { 162, 1 },
  { 162, 2 },
  { 163, 1 },
  { 163, 1 },
  { 158, 2 },
  { 158, 0 },
  { 166, 3 },
  { 166, 1 },
  { 166, 2 },
  { 166, 4 },
  { 166, 3 },
  { 166, 3 },
  { 166, 2 },
  { 167, 1 },
  { 167, 2 },
  { 167, 3 },
  { 167, 4 },
  { 167, 2 },
  { 167, 4 },
  { 167, 4 },
  { 167, 1 },
  { 167, 2 },
  { 175, 0 },
  { 175, 1 },
  { 173, 0 },
  { 173, 2 },
  { 176, 2 },
  { 176, 3 },
  { 176, 3 },
  { 176, 3 },
  { 177, 2 },
  { 177, 2 },
  { 177, 1 },
  { 177, 1 },
  { 174, 3 },
  { 174, 2 },
  { 178, 0 },
  { 178, 2 },
  { 178, 2 },
  { 151, 0 },
  { 151, 2 },
  { 179, 3 },
  { 179, 2 },
  { 179, 1 },
  { 180, 2 },
  { 180, 7 },
  { 180, 5 },
  { 180, 5 },
  { 180, 10 },
  { 182, 0 },
  { 182, 1 },
  { 170, 0 },
  { 170, 3 },
  { 184, 1 },
  { 184, 1 },
  { 184, 1 },
  { 141, 4 },
  { 186, 2 },
  { 186, 0 },
  { 141, 1 },
  { 153, 1 },
  { 153, 3 },
  { 189, 1 },
  { 189, 2 },
  { 189, 1 },
  { 188, 9 },
  { 190, 1 },
  { 190, 1 },
  { 190, 0 },
  { 198, 2 },
  { 198, 0 },
  { 191, 3 },
  { 191, 2 },
  { 191, 4 },
  { 199, 2 },
  { 199, 1 },
  { 199, 0 },
  { 192, 0 },
  { 192, 2 },
  { 201, 2 },
  { 201, 0 },
  { 200, 6 },
  { 200, 7 },
  { 205, 1 },
  { 205, 1 },
  { 149, 0 },
  { 149, 2 },
  { 187, 2 },
  { 202, 1 },
  { 202, 2 },
  { 202, 3 },
  { 202, 4 },
  { 203, 2 },
  { 203, 0 },
  { 204, 4 },
  { 204, 0 },
  { 196, 0 },
  { 196, 3 },
  { 208, 5 },
  { 208, 3 },
  { 209, 1 },
  { 171, 1 },
  { 171, 1 },
  { 171, 0 },
  { 210, 0 },
  { 210, 2 },
  { 194, 0 },
  { 194, 3 },
  { 195, 0 },
  { 195, 2 },
  { 197, 0 },
  { 197, 2 },
  { 197, 4 },
  { 197, 4 },
  { 141, 5 },
  { 193, 0 },
  { 193, 2 },
  { 141, 6 },
  { 212, 5 },
  { 212, 3 },
  { 141, 6 },
  { 141, 6 },
  { 141, 5 },
  { 141, 8 },
  { 141, 8 },
  { 141, 7 },
  { 213, 1 },
  { 213, 1 },
  { 215, 5 },
  { 215, 3 },
  { 215, 2 },
  { 216, 3 },
  { 216, 1 },
  { 214, 0 },
  { 214, 2 },
  { 214, 3 },
  { 207, 3 },
  { 207, 1 },
  { 169, 1 },
  { 169, 3 },
  { 168, 1 },
  { 169, 1 },
  { 169, 1 },
  { 169, 3 },
  { 169, 5 },
  { 168, 1 },
  { 168, 1 },
  { 169, 1 },
  { 169, 1 },
  { 169, 1 },
  { 169, 6 },
  { 169, 5 },
  { 169, 4 },
  { 168, 1 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 169, 3 },
  { 217, 1 },
  { 217, 2 },
  { 218, 2 },
  { 218, 0 },
  { 169, 4 },
  { 169, 2 },
  { 169, 3 },
  { 169, 3 },
  { 169, 4 },
  { 169, 2 },
  { 169, 2 },
  { 169, 2 },
  { 219, 1 },
  { 219, 2 },
  { 220, 1 },
  { 169, 5 },
  { 221, 1 },
  { 221, 2 },
  { 169, 5 },
  { 169, 3 },
  { 169, 5 },
  { 169, 4 },
  { 169, 4 },
  { 169, 5 },
  { 223, 5 },
  { 223, 4 },
  { 224, 2 },
  { 224, 0 },
  { 222, 1 },
  { 222, 0 },
  { 211, 3 },
  { 211, 1 },
  { 225, 1 },
  { 225, 0 },
  { 172, 0 },
  { 172, 3 },
  { 181, 5 },
  { 181, 3 },
  { 226, 1 },
  { 164, 2 },
  { 165, 2 },
  { 228, 1 },
  { 227, 1 },
  { 227, 0 },
  { 185, 1 },
  { 185, 1 },
  { 185, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0
        && yyruleno<sizeof(yyRuleName)/sizeof(yyRuleName[0]) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

#ifndef NDEBUG
  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.
  */
  memset(&yygotominor, 0, sizeof(yygotominor));
#endif

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 3:
      case 4:
#line 98 "parse.y"
{ sqlite3FinishCoding(pParse); }
#line 1674 "parse.c"
        break;
      case 7:
#line 102 "parse.y"
{ sqlite3BeginParse(pParse, 0); }
#line 1679 "parse.c"
        break;
      case 8:
#line 104 "parse.y"
{ sqlite3BeginParse(pParse, 1); }
#line 1684 "parse.c"
        break;
      case 9:
#line 105 "parse.y"
{ sqlite3BeginParse(pParse, 2); }
#line 1689 "parse.c"
        break;
      case 10:
#line 112 "parse.y"
{sqlite3BeginTransaction(pParse, SQLTYPE_TRANSACTION_BEGIN);}
#line 1694 "parse.c"
        break;
      case 11:
#line 113 "parse.y"
{sqlite3BeginTransaction(pParse, SQLTYPE_TRANSACTION_START);}
#line 1699 "parse.c"
        break;
      case 14:
#line 124 "parse.y"
{sqlite3CommitTransaction(pParse);}
#line 1704 "parse.c"
        break;
      case 15:
#line 126 "parse.y"
{sqlite3RollbackTransaction(pParse);}
#line 1709 "parse.c"
        break;
      case 17:
#line 131 "parse.y"
{
   sqlite3StartTable(pParse, 0, 0, 0, 0, 0);
}
#line 1716 "parse.c"
        break;
      case 18:
      case 21:
      case 67:
      case 81:
      case 83:
      case 94:
      case 103:
      case 112:
      case 113:
      case 219:
      case 223:
#line 136 "parse.y"
{yygotominor.yy444 = 0;}
#line 1731 "parse.c"
        break;
      case 19:
      case 20:
      case 68:
      case 82:
      case 102:
      case 111:
      case 220:
      case 224:
#line 137 "parse.y"
{yygotominor.yy444 = 1;}
#line 1743 "parse.c"
        break;
      case 22:
#line 143 "parse.y"
{
  //sqlite3EndTable(pParse,&X,&Y,0);
}
#line 1750 "parse.c"
        break;
      case 23:
#line 146 "parse.y"
{
  sqlite3EndTable(pParse,0,0,0);
  //sqlite3SelectDelete(S);
  yy_destructor(153,&yymsp[0].minor);
}
#line 1759 "parse.c"
        break;
      case 33:
#line 168 "parse.y"
{
  //A.z = X.z;
  //A.n = (pParse->sLastToken.z-X.z) + pParse->sLastToken.n;
}
#line 1767 "parse.c"
        break;
      case 34:
#line 172 "parse.y"
{
  //sqlite3AddColumn(pParse,&X);
  //A = X;
}
#line 1775 "parse.c"
        break;
      case 35:
      case 36:
      case 37:
      case 38:
      case 39:
      case 248:
#line 182 "parse.y"
{yygotominor.yy82 = yymsp[0].minor.yy0;}
#line 1785 "parse.c"
        break;
      case 41:
#line 241 "parse.y"
{ /*sqlite3AddColumnType(pParse,&X);*/ }
#line 1790 "parse.c"
        break;
      case 42:
      case 45:
      case 119:
      case 120:
      case 131:
      case 150:
      case 245:
      case 246:
      case 247:
#line 242 "parse.y"
{yygotominor.yy82 = yymsp[0].minor.yy82;}
#line 1803 "parse.c"
        break;
      case 43:
#line 243 "parse.y"
{
  yygotominor.yy82.z = yymsp[-3].minor.yy82.z;
  yygotominor.yy82.n = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-3].minor.yy82.z;
}
#line 1811 "parse.c"
        break;
      case 44:
#line 247 "parse.y"
{
  yygotominor.yy82.z = yymsp[-5].minor.yy82.z;
  yygotominor.yy82.n = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-5].minor.yy82.z;
}
#line 1819 "parse.c"
        break;
      case 46:
#line 253 "parse.y"
{yygotominor.yy82.z=yymsp[-1].minor.yy82.z; yygotominor.yy82.n=yymsp[0].minor.yy82.n+(yymsp[0].minor.yy82.z-yymsp[-1].minor.yy82.z);}
#line 1824 "parse.c"
        break;
      case 47:
#line 255 "parse.y"
{ yygotominor.yy444 = atoi((char*)yymsp[0].minor.yy82.z); }
#line 1829 "parse.c"
        break;
      case 48:
#line 256 "parse.y"
{ yygotominor.yy444 = -atoi((char*)yymsp[0].minor.yy82.z); }
#line 1834 "parse.c"
        break;
      case 53:
#line 265 "parse.y"
{ /*sqlite3AddDefaultValue(pParse,X);*/   yy_destructor(168,&yymsp[0].minor);
}
#line 1840 "parse.c"
        break;
      case 54:
#line 266 "parse.y"
{ /*sqlite3AddDefaultValue(pParse,X);*/ }
#line 1845 "parse.c"
        break;
      case 55:
#line 267 "parse.y"
{ /*sqlite3AddDefaultValue(pParse,X);*/  yy_destructor(168,&yymsp[0].minor);
}
#line 1851 "parse.c"
        break;
      case 56:
#line 268 "parse.y"
{
  //Expr *p = sqlite3Expr(TK_UMINUS, X, 0, 0);
  //sqlite3AddDefaultValue(pParse,p);
  yy_destructor(168,&yymsp[0].minor);
}
#line 1860 "parse.c"
        break;
      case 57:
#line 272 "parse.y"
{
  //Expr *p = sqlite3Expr(TK_STRING, 0, 0, &X);
  //sqlite3AddDefaultValue(pParse,p);
}
#line 1868 "parse.c"
        break;
      case 60:
#line 282 "parse.y"
{/*sqlite3AddNotNull(pParse, R);*/}
#line 1873 "parse.c"
        break;
      case 61:
#line 284 "parse.y"
{/*sqlite3AddPrimaryKey(pParse,0,R,I,Z);*/}
#line 1878 "parse.c"
        break;
      case 62:
#line 285 "parse.y"
{/*sqlite3CreateIndex(pParse,0,0,0,0,R,0,0,0,0);*/}
#line 1883 "parse.c"
        break;
      case 63:
#line 286 "parse.y"
{/*sqlite3AddCheckConstraint(pParse,X);*/  sqlite3ExprDelete(yymsp[-1].minor.yy396); }
#line 1888 "parse.c"
        break;
      case 64:
#line 288 "parse.y"
{/*sqlite3CreateForeignKey(pParse,0,&T,TA,R);*/ sqlite3ExprListDelete(yymsp[-1].minor.yy162); }
#line 1893 "parse.c"
        break;
      case 65:
#line 289 "parse.y"
{/*sqlite3DeferForeignKey(pParse,D);*/}
#line 1898 "parse.c"
        break;
      case 66:
#line 290 "parse.y"
{/*sqlite3AddCollateType(pParse, (char*)C.z, C.n);*/}
#line 1903 "parse.c"
        break;
      case 69:
#line 303 "parse.y"
{ yygotominor.yy444 = OE_Restrict * 0x010101; }
#line 1908 "parse.c"
        break;
      case 70:
#line 304 "parse.y"
{ yygotominor.yy444 = (yymsp[-1].minor.yy444 & yymsp[0].minor.yy331.mask) | yymsp[0].minor.yy331.value; }
#line 1913 "parse.c"
        break;
      case 71:
#line 306 "parse.y"
{ yygotominor.yy331.value = 0;     yygotominor.yy331.mask = 0x000000; }
#line 1918 "parse.c"
        break;
      case 72:
#line 307 "parse.y"
{ yygotominor.yy331.value = yymsp[0].minor.yy444;     yygotominor.yy331.mask = 0x0000ff; }
#line 1923 "parse.c"
        break;
      case 73:
#line 308 "parse.y"
{ yygotominor.yy331.value = yymsp[0].minor.yy444<<8;  yygotominor.yy331.mask = 0x00ff00; }
#line 1928 "parse.c"
        break;
      case 74:
#line 309 "parse.y"
{ yygotominor.yy331.value = yymsp[0].minor.yy444<<16; yygotominor.yy331.mask = 0xff0000; }
#line 1933 "parse.c"
        break;
      case 75:
#line 311 "parse.y"
{ yygotominor.yy444 = OE_SetNull; }
#line 1938 "parse.c"
        break;
      case 76:
#line 312 "parse.y"
{ yygotominor.yy444 = OE_SetDflt; }
#line 1943 "parse.c"
        break;
      case 77:
#line 313 "parse.y"
{ yygotominor.yy444 = OE_Cascade; }
#line 1948 "parse.c"
        break;
      case 78:
#line 314 "parse.y"
{ yygotominor.yy444 = OE_Restrict; }
#line 1953 "parse.c"
        break;
      case 79:
      case 80:
      case 95:
      case 97:
      case 98:
#line 316 "parse.y"
{yygotominor.yy444 = yymsp[0].minor.yy444;}
#line 1962 "parse.c"
        break;
      case 90:
#line 335 "parse.y"
{ sqlite3ExprListDelete(yymsp[-3].minor.yy162); }
#line 1967 "parse.c"
        break;
      case 91:
#line 337 "parse.y"
{/*sqlite3CreateIndex(pParse,0,0,0,yymsp[-2].minor.yy162,R,0,0,0,0);*/ sqlite3ExprListDelete(yymsp[-2].minor.yy162);}
#line 1972 "parse.c"
        break;
      case 92:
#line 338 "parse.y"
{/*sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy396);*/ sqlite3ExprDelete(yymsp[-2].minor.yy396);}
#line 1977 "parse.c"
        break;
      case 93:
#line 340 "parse.y"
{
        sqlite3ExprListDelete(yymsp[-6].minor.yy162);
        sqlite3ExprListDelete(yymsp[-2].minor.yy162);
 }
#line 1985 "parse.c"
        break;
      case 96:
#line 355 "parse.y"
{yygotominor.yy444 = OE_Default;}
#line 1990 "parse.c"
        break;
      case 99:
#line 360 "parse.y"
{yygotominor.yy444 = OE_Ignore;}
#line 1995 "parse.c"
        break;
      case 100:
      case 172:
#line 361 "parse.y"
{yygotominor.yy444 = OE_Replace;}
#line 2001 "parse.c"
        break;
      case 101:
#line 365 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy41, 0, yymsp[-1].minor.yy444);
}
#line 2008 "parse.c"
        break;
      case 104:
#line 385 "parse.y"
{
  sqlite3Select(pParse, yymsp[0].minor.yy203, SRT_Callback, 0, 0, 0, 0, 0);
  // sqlite3SelectDelete(yymsp[0].minor.yy203);
}
#line 2016 "parse.c"
        break;
      case 105:
      case 128:
#line 395 "parse.y"
{yygotominor.yy203 = yymsp[0].minor.yy203;}
#line 2022 "parse.c"
        break;
      case 106:
#line 397 "parse.y"
{
  if( yymsp[0].minor.yy203 ){
    yymsp[0].minor.yy203->op = yymsp[-1].minor.yy444;
    yymsp[0].minor.yy203->pPrior = yymsp[-2].minor.yy203;
  }
  yygotominor.yy203 = yymsp[0].minor.yy203;
}
#line 2033 "parse.c"
        break;
      case 107:
      case 109:
#line 405 "parse.y"
{yygotominor.yy444 = yymsp[0].major;}
#line 2039 "parse.c"
        break;
      case 108:
#line 406 "parse.y"
{yygotominor.yy444 = TK_ALL;}
#line 2044 "parse.c"
        break;
      case 110:
#line 410 "parse.y"
{
  yygotominor.yy203 = sqlite3SelectNew(yymsp[-6].minor.yy162,yymsp[-5].minor.yy41,yymsp[-4].minor.yy396,yymsp[-3].minor.yy162,yymsp[-2].minor.yy396,yymsp[-1].minor.yy162,yymsp[-7].minor.yy444,yymsp[0].minor.yy76.pLimit,yymsp[0].minor.yy76.pOffset);
}
#line 2051 "parse.c"
        break;
      case 114:
      case 242:
#line 431 "parse.y"
{yygotominor.yy162 = yymsp[-1].minor.yy162;}
#line 2057 "parse.c"
        break;
      case 115:
      case 141:
      case 151:
      case 241:
#line 432 "parse.y"
{yygotominor.yy162 = 0;}
#line 2065 "parse.c"
        break;
      case 116:
#line 433 "parse.y"
{
   yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-2].minor.yy162,yymsp[-1].minor.yy396,yymsp[0].minor.yy82.n?&yymsp[0].minor.yy82:0);
}
#line 2072 "parse.c"
        break;
      case 117:
#line 436 "parse.y"
{
  yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-1].minor.yy162, sqlite3Expr(TK_ALL, 0, 0, 0), 0);
}
#line 2079 "parse.c"
        break;
      case 118:
#line 439 "parse.y"
{
  Expr *pRight = sqlite3Expr(TK_ALL, 0, 0, 0);
  Expr *pLeft = sqlite3Expr(TK_ID, 0, 0, &yymsp[-2].minor.yy82);
  yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-3].minor.yy162, sqlite3Expr(TK_DOT, pLeft, pRight, 0), 0);
}
#line 2088 "parse.c"
        break;
      case 121:
#line 451 "parse.y"
{yygotominor.yy82.n = 0;}
#line 2093 "parse.c"
        break;
      case 122:
#line 463 "parse.y"
{yygotominor.yy41 = sqliteMalloc(sizeof(*yygotominor.yy41));}
#line 2098 "parse.c"
        break;
      case 123:
#line 464 "parse.y"
{yygotominor.yy41 = yymsp[0].minor.yy41;}
#line 2103 "parse.c"
        break;
      case 124:
#line 469 "parse.y"
{
   yygotominor.yy41 = yymsp[-1].minor.yy41;
   if( yygotominor.yy41 && yygotominor.yy41->nSrc>0 ) yygotominor.yy41->a[yygotominor.yy41->nSrc-1].jointype = yymsp[0].minor.yy444;
}
#line 2111 "parse.c"
        break;
      case 125:
#line 473 "parse.y"
{yygotominor.yy41 = 0;}
#line 2116 "parse.c"
        break;
      case 126:
#line 474 "parse.y"
{
  yygotominor.yy41 = sqlite3SrcListAppend(yymsp[-5].minor.yy41,&yymsp[-4].minor.yy82,&yymsp[-3].minor.yy82);
  if( yymsp[-2].minor.yy82.n ) sqlite3SrcListAddAlias(yygotominor.yy41,&yymsp[-2].minor.yy82);
  if( yymsp[-1].minor.yy396 ){
    if( yygotominor.yy41 && yygotominor.yy41->nSrc>1 ){ yygotominor.yy41->a[yygotominor.yy41->nSrc-2].pOn = yymsp[-1].minor.yy396; }
    else { sqlite3ExprDelete(yymsp[-1].minor.yy396); }
  }
  if( yymsp[0].minor.yy306 ){
    if( yygotominor.yy41 && yygotominor.yy41->nSrc>1 ){ yygotominor.yy41->a[yygotominor.yy41->nSrc-2].pUsing = yymsp[0].minor.yy306; }
    else { sqlite3IdListDelete(yymsp[0].minor.yy306); }
  }
}
#line 2132 "parse.c"
        break;
      case 127:
#line 488 "parse.y"
{
    yygotominor.yy41 = sqlite3SrcListAppend(yymsp[-6].minor.yy41,0,0);
    yygotominor.yy41->a[yygotominor.yy41->nSrc-1].pSelect = yymsp[-4].minor.yy203;
    if( yymsp[-2].minor.yy82.n ) sqlite3SrcListAddAlias(yygotominor.yy41,&yymsp[-2].minor.yy82);
    if( yymsp[-1].minor.yy396 ){
      if( yygotominor.yy41 && yygotominor.yy41->nSrc>1 ){ yygotominor.yy41->a[yygotominor.yy41->nSrc-2].pOn = yymsp[-1].minor.yy396; }
      else { sqlite3ExprDelete(yymsp[-1].minor.yy396); }
    }
    if( yymsp[0].minor.yy306 ){
      if( yygotominor.yy41 && yygotominor.yy41->nSrc>1 ){ yygotominor.yy41->a[yygotominor.yy41->nSrc-2].pUsing = yymsp[0].minor.yy306; }
      else { sqlite3IdListDelete(yymsp[0].minor.yy306); }
    }
  }
#line 2149 "parse.c"
        break;
      case 129:
#line 509 "parse.y"
{
     yygotominor.yy203 = sqlite3SelectNew(0,yymsp[0].minor.yy41,0,0,0,0,0,0,0);
  }
#line 2156 "parse.c"
        break;
      case 130:
#line 515 "parse.y"
{yygotominor.yy82.z=0; yygotominor.yy82.n=0;}
#line 2161 "parse.c"
        break;
      case 132:
#line 520 "parse.y"
{yygotominor.yy41 = sqlite3SrcListAppend(0,&yymsp[-1].minor.yy82,&yymsp[0].minor.yy82);}
#line 2166 "parse.c"
        break;
      case 133:
#line 524 "parse.y"
{ yygotominor.yy444 = JT_INNER; }
#line 2171 "parse.c"
        break;
      case 134:
#line 525 "parse.y"
{ yygotominor.yy444 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0); }
#line 2176 "parse.c"
        break;
      case 135:
#line 526 "parse.y"
{ yygotominor.yy444 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy82,0); }
#line 2181 "parse.c"
        break;
      case 136:
#line 528 "parse.y"
{ yygotominor.yy444 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy82,&yymsp[-1].minor.yy82); }
#line 2186 "parse.c"
        break;
      case 137:
      case 145:
      case 154:
      case 161:
      case 183:
      case 209:
      case 233:
      case 235:
      case 239:
#line 532 "parse.y"
{yygotominor.yy396 = yymsp[0].minor.yy396;}
#line 2199 "parse.c"
        break;
      case 138:
      case 153:
      case 160:
      case 210:
      case 234:
      case 236:
      case 240:
#line 533 "parse.y"
{yygotominor.yy396 = 0;}
#line 2210 "parse.c"
        break;
      case 139:
      case 180:
#line 537 "parse.y"
{yygotominor.yy306 = yymsp[-1].minor.yy306;}
#line 2216 "parse.c"
        break;
      case 140:
      case 178:
      case 179:
#line 538 "parse.y"
{yygotominor.yy306 = 0;}
#line 2223 "parse.c"
        break;
      case 142:
      case 152:
#line 549 "parse.y"
{yygotominor.yy162 = yymsp[0].minor.yy162;}
#line 2229 "parse.c"
        break;
      case 143:
#line 550 "parse.y"
{
  yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-4].minor.yy162,yymsp[-2].minor.yy396,yymsp[-1].minor.yy82.n>0?&yymsp[-1].minor.yy82:0);
  if( yygotominor.yy162 ) yygotominor.yy162->a[yygotominor.yy162->nExpr-1].sortOrder = yymsp[0].minor.yy444;
}
#line 2237 "parse.c"
        break;
      case 144:
#line 554 "parse.y"
{
  yygotominor.yy162 = sqlite3ExprListAppend(0,yymsp[-2].minor.yy396,yymsp[-1].minor.yy82.n>0?&yymsp[-1].minor.yy82:0);
  if( yygotominor.yy162 && yygotominor.yy162->a ) yygotominor.yy162->a[0].sortOrder = yymsp[0].minor.yy444;
}
#line 2245 "parse.c"
        break;
      case 146:
      case 148:
#line 563 "parse.y"
{yygotominor.yy444 = SQLITE_SO_ASC;}
#line 2251 "parse.c"
        break;
      case 147:
#line 564 "parse.y"
{yygotominor.yy444 = SQLITE_SO_DESC;}
#line 2256 "parse.c"
        break;
      case 149:
#line 566 "parse.y"
{yygotominor.yy82.z = 0; yygotominor.yy82.n = 0;}
#line 2261 "parse.c"
        break;
      case 155:
#line 584 "parse.y"
{yygotominor.yy76.pLimit = 0; yygotominor.yy76.pOffset = 0;}
#line 2266 "parse.c"
        break;
      case 156:
#line 585 "parse.y"
{yygotominor.yy76.pLimit = yymsp[0].minor.yy396; yygotominor.yy76.pOffset = 0;}
#line 2271 "parse.c"
        break;
      case 157:
#line 587 "parse.y"
{yygotominor.yy76.pLimit = yymsp[-2].minor.yy396; yygotominor.yy76.pOffset = yymsp[0].minor.yy396;}
#line 2276 "parse.c"
        break;
      case 158:
#line 589 "parse.y"
{yygotominor.yy76.pOffset = yymsp[-2].minor.yy396; yygotominor.yy76.pLimit = yymsp[0].minor.yy396;}
#line 2281 "parse.c"
        break;
      case 159:
#line 593 "parse.y"
{sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy41,yymsp[-1].minor.yy396, yymsp[0].minor.yy76.pLimit, yymsp[0].minor.yy76.pOffset);}
#line 2286 "parse.c"
        break;
      case 162:
#line 605 "parse.y"
{sqlite3Update(pParse,yymsp[-4].minor.yy41,yymsp[-2].minor.yy162,yymsp[-1].minor.yy396,OE_Default, yymsp[0].minor.yy76.pLimit, yymsp[0].minor.yy76.pOffset);}
#line 2291 "parse.c"
        break;
      case 163:
#line 611 "parse.y"
{yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-4].minor.yy162,yymsp[0].minor.yy396,&yymsp[-2].minor.yy82);}
#line 2296 "parse.c"
        break;
      case 164:
#line 612 "parse.y"
{yygotominor.yy162 = sqlite3ExprListAppend(0,yymsp[0].minor.yy396,&yymsp[-2].minor.yy82);}
#line 2301 "parse.c"
        break;
      case 165:
#line 617 "parse.y"
{sqlite3Insert(pParse, yymsp[-3].minor.yy41, 0, yymsp[0].minor.yy37, 0, yymsp[-2].minor.yy306, yymsp[-5].minor.yy444);}
#line 2306 "parse.c"
        break;
      case 166:
#line 621 "parse.y"
{sqlite3Insert(pParse, yymsp[-3].minor.yy41, yymsp[0].minor.yy162, 0, 0, yymsp[-2].minor.yy306, yymsp[-5].minor.yy444);}
#line 2311 "parse.c"
        break;
      case 167:
#line 624 "parse.y"
{sqlite3Insert(pParse, yymsp[-2].minor.yy41, 0, 0, yymsp[0].minor.yy203, yymsp[-1].minor.yy306, yymsp[-4].minor.yy444);}
#line 2316 "parse.c"
        break;
      case 168:
#line 628 "parse.y"
{sqlite3Insert(pParse, yymsp[-3].minor.yy41, 0, yymsp[0].minor.yy37, 0, yymsp[-2].minor.yy306, yymsp[-7].minor.yy444);}
#line 2321 "parse.c"
        break;
      case 169:
#line 632 "parse.y"
{sqlite3Insert(pParse, yymsp[-3].minor.yy41, yymsp[0].minor.yy162, 0, 0, yymsp[-2].minor.yy306, yymsp[-7].minor.yy444);}
#line 2326 "parse.c"
        break;
      case 170:
#line 635 "parse.y"
{sqlite3Insert(pParse, yymsp[-2].minor.yy41, 0, 0, yymsp[0].minor.yy203, yymsp[-1].minor.yy306, yymsp[-6].minor.yy444);}
#line 2331 "parse.c"
        break;
      case 171:
#line 638 "parse.y"
{ yygotominor.yy444 = OE_Default; }
#line 2336 "parse.c"
        break;
      case 173:
#line 644 "parse.y"
{ yygotominor.yy37 = sqlite3ValuesListAppend(yymsp[-4].minor.yy37, yymsp[-1].minor.yy162);}
#line 2341 "parse.c"
        break;
      case 174:
#line 645 "parse.y"
{ yygotominor.yy37 = sqlite3ValuesListAppend(0, yymsp[-1].minor.yy162); }
#line 2346 "parse.c"
        break;
      case 175:
#line 646 "parse.y"
{ yygotominor.yy37 = 0; }
#line 2351 "parse.c"
        break;
      case 176:
      case 237:
#line 651 "parse.y"
{yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-2].minor.yy162,yymsp[0].minor.yy396,0);}
#line 2357 "parse.c"
        break;
      case 177:
      case 238:
#line 652 "parse.y"
{yygotominor.yy162 = sqlite3ExprListAppend(0,yymsp[0].minor.yy396,0);}
#line 2363 "parse.c"
        break;
      case 181:
#line 662 "parse.y"
{yygotominor.yy306 = sqlite3IdListAppend(yymsp[-2].minor.yy306,&yymsp[0].minor.yy82);}
#line 2368 "parse.c"
        break;
      case 182:
#line 663 "parse.y"
{yygotominor.yy306 = sqlite3IdListAppend(0,&yymsp[0].minor.yy82);}
#line 2373 "parse.c"
        break;
      case 184:
#line 674 "parse.y"
{yygotominor.yy396 = yymsp[-1].minor.yy396; sqlite3ExprSpan(yygotominor.yy396,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); }
#line 2378 "parse.c"
        break;
      case 185:
      case 190:
      case 191:
#line 675 "parse.y"
{yygotominor.yy396 = sqlite3Expr(yymsp[0].major, 0, 0, &yymsp[0].minor.yy0);}
#line 2385 "parse.c"
        break;
      case 186:
      case 187:
#line 676 "parse.y"
{yygotominor.yy396 = sqlite3Expr(TK_ID, 0, 0, &yymsp[0].minor.yy0);}
#line 2391 "parse.c"
        break;
      case 188:
#line 678 "parse.y"
{
  Expr *temp1 = sqlite3Expr(TK_ID, 0, 0, &yymsp[-2].minor.yy82);
  Expr *temp2 = sqlite3Expr(TK_ID, 0, 0, &yymsp[0].minor.yy82);
  yygotominor.yy396 = sqlite3Expr(TK_DOT, temp1, temp2, 0);
}
#line 2400 "parse.c"
        break;
      case 189:
#line 683 "parse.y"
{
  Expr *temp1 = sqlite3Expr(TK_ID, 0, 0, &yymsp[-4].minor.yy82);
  Expr *temp2 = sqlite3Expr(TK_ID, 0, 0, &yymsp[-2].minor.yy82);
  Expr *temp3 = sqlite3Expr(TK_ID, 0, 0, &yymsp[0].minor.yy82);
  Expr *temp4 = sqlite3Expr(TK_DOT, temp2, temp3, 0);
  yygotominor.yy396 = sqlite3Expr(TK_DOT, temp1, temp4, 0);
}
#line 2411 "parse.c"
        break;
      case 192:
#line 692 "parse.y"
{yygotominor.yy396 = sqlite3RegisterExpr(pParse, &yymsp[0].minor.yy0);}
#line 2416 "parse.c"
        break;
      case 193:
      case 194:
#line 693 "parse.y"
{
  Token *pToken = &yymsp[0].minor.yy0;
  Expr *pExpr = yygotominor.yy396 = sqlite3Expr(TK_VARIABLE, 0, 0, pToken);
  //sqlite3ExprAssignVarNumber(pParse, pExpr);
}
#line 2426 "parse.c"
        break;
      case 195:
#line 704 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_CAST, yymsp[-3].minor.yy396, 0, &yymsp[-1].minor.yy82);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0);
}
#line 2434 "parse.c"
        break;
      case 196:
#line 709 "parse.y"
{
  yygotominor.yy396 = sqlite3ExprFunction(yymsp[-1].minor.yy162, &yymsp[-4].minor.yy0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);
  if( yymsp[-2].minor.yy444 && yygotominor.yy396 ){
    yygotominor.yy396->flags |= EP_Distinct;
  }
}
#line 2445 "parse.c"
        break;
      case 197:
#line 716 "parse.y"
{
  yygotominor.yy396 = sqlite3ExprFunction(0, &yymsp[-3].minor.yy0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
}
#line 2453 "parse.c"
        break;
      case 198:
#line 720 "parse.y"
{
  /* The CURRENT_TIME, CURRENT_DATE, and CURRENT_TIMESTAMP values are
  ** treated as functions that return constants */
  yygotominor.yy396 = sqlite3ExprFunction(0,&yymsp[0].minor.yy0);
  if( yygotominor.yy396 ) yygotominor.yy396->op = TK_CONST_FUNC;
}
#line 2463 "parse.c"
        break;
      case 199:
      case 200:
      case 201:
      case 202:
      case 203:
      case 204:
      case 205:
      case 206:
#line 726 "parse.y"
{yygotominor.yy396 = sqlite3Expr(yymsp[-1].major, yymsp[-2].minor.yy396, yymsp[0].minor.yy396, 0);}
#line 2475 "parse.c"
        break;
      case 207:
#line 736 "parse.y"
{yygotominor.yy78.eOperator = yymsp[0].minor.yy0; yygotominor.yy78.not = 0;}
#line 2480 "parse.c"
        break;
      case 208:
#line 737 "parse.y"
{yygotominor.yy78.eOperator = yymsp[0].minor.yy0; yygotominor.yy78.not = 1;}
#line 2485 "parse.c"
        break;
      case 211:
#line 742 "parse.y"
{
  ExprList *pList;
  pList = sqlite3ExprListAppend(0, yymsp[-3].minor.yy396, 0);
  pList = sqlite3ExprListAppend(pList, yymsp[-1].minor.yy396, 0);
  if( yymsp[0].minor.yy396 ){
    pList = sqlite3ExprListAppend(pList, yymsp[0].minor.yy396, 0);
  }
  //yygotominor.yy396 = sqlite3ExprFunction(pList, &yymsp[-2].minor.yy78.eOperator);
  yygotominor.yy396 = sqlite3ExprLikeOp(pList, &yymsp[-2].minor.yy78.eOperator);
  if( yymsp[-2].minor.yy78.not ) yygotominor.yy396 = sqlite3Expr(TK_NOT, yygotominor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396, &yymsp[-3].minor.yy396->span, &yymsp[-1].minor.yy396->span);
}
#line 2501 "parse.c"
        break;
      case 212:
#line 755 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(yymsp[0].major, yymsp[-1].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-1].minor.yy396->span,&yymsp[0].minor.yy0);
}
#line 2509 "parse.c"
        break;
      case 213:
#line 759 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_ISNULL, yymsp[-2].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-2].minor.yy396->span,&yymsp[0].minor.yy0);
}
#line 2517 "parse.c"
        break;
      case 214:
#line 763 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_NOTNULL, yymsp[-2].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-2].minor.yy396->span,&yymsp[0].minor.yy0);
}
#line 2525 "parse.c"
        break;
      case 215:
#line 767 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_NOTNULL, yymsp[-3].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-3].minor.yy396->span,&yymsp[0].minor.yy0);
}
#line 2533 "parse.c"
        break;
      case 216:
#line 771 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(yymsp[-1].major, yymsp[0].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy396->span);
}
#line 2541 "parse.c"
        break;
      case 217:
#line 775 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_UMINUS, yymsp[0].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy396->span);
}
#line 2549 "parse.c"
        break;
      case 218:
#line 779 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_UPLUS, yymsp[0].minor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy396->span);
}
#line 2557 "parse.c"
        break;
      case 221:
#line 790 "parse.y"
{ yygotominor.yy396 = sqlite3Expr(yymsp[0].major, 0, 0, &yymsp[0].minor.yy0); }
#line 2562 "parse.c"
        break;
      case 222:
#line 793 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(0, yymsp[-2].minor.yy396, 0);
  pList = sqlite3ExprListAppend(pList, yymsp[0].minor.yy396, 0);
  yygotominor.yy396 = sqlite3Expr(TK_BETWEEN, yymsp[-4].minor.yy396, 0, 0);
  if( yygotominor.yy396 ){
    yygotominor.yy396->pList = pList;
  }else{
    sqlite3ExprListDelete(pList);
  }
  if( yymsp[-3].minor.yy444 ) yygotominor.yy396 = sqlite3Expr(TK_NOT, yygotominor.yy396, 0, 0);
  sqlite3ExprSpan(yygotominor.yy396,&yymsp[-4].minor.yy396->span,&yymsp[0].minor.yy396->span);
}
#line 2578 "parse.c"
        break;
      case 225:
#line 809 "parse.y"
{
    yygotominor.yy396 = sqlite3Expr(TK_IN, yymsp[-4].minor.yy396, 0, 0);
    if( yygotominor.yy396 ){
      yygotominor.yy396->pList = yymsp[-1].minor.yy162;
    }else{
      sqlite3ExprListDelete(yymsp[-1].minor.yy162);
    }
    if( yymsp[-3].minor.yy444 ) yygotominor.yy396 = sqlite3Expr(TK_NOT, yygotominor.yy396, 0, 0);
    sqlite3ExprSpan(yygotominor.yy396,&yymsp[-4].minor.yy396->span,&yymsp[0].minor.yy0);
  }
#line 2592 "parse.c"
        break;
      case 226:
#line 819 "parse.y"
{
    yygotominor.yy396 = sqlite3Expr(TK_SELECT, 0, 0, 0);
    if( yygotominor.yy396 ){
      yygotominor.yy396->pSelect = yymsp[-1].minor.yy203;
    }else{
      sqlite3SelectDelete(yymsp[-1].minor.yy203);
    }
    sqlite3ExprSpan(yygotominor.yy396,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0);
  }
#line 2605 "parse.c"
        break;
      case 227:
#line 828 "parse.y"
{
    yygotominor.yy396 = sqlite3Expr(TK_IN, yymsp[-4].minor.yy396, 0, 0);
    if( yygotominor.yy396 ){
      yygotominor.yy396->pSelect = yymsp[-1].minor.yy203;
    }else{
      sqlite3SelectDelete(yymsp[-1].minor.yy203);
    }
    if( yymsp[-3].minor.yy444 ) yygotominor.yy396 = sqlite3Expr(TK_NOT, yygotominor.yy396, 0, 0);
    sqlite3ExprSpan(yygotominor.yy396,&yymsp[-4].minor.yy396->span,&yymsp[0].minor.yy0);
  }
#line 2619 "parse.c"
        break;
      case 228:
#line 838 "parse.y"
{
    SrcList *pSrc = sqlite3SrcListAppend(0,&yymsp[-1].minor.yy82,&yymsp[0].minor.yy82);
    yygotominor.yy396 = sqlite3Expr(TK_IN, yymsp[-3].minor.yy396, 0, 0);
    if( yygotominor.yy396 ){
      yygotominor.yy396->pSelect = sqlite3SelectNew(0,pSrc,0,0,0,0,0,0,0);
    }else{
      sqlite3SrcListDelete(pSrc);
    }
    if( yymsp[-2].minor.yy444 ) yygotominor.yy396 = sqlite3Expr(TK_NOT, yygotominor.yy396, 0, 0);
    sqlite3ExprSpan(yygotominor.yy396,&yymsp[-3].minor.yy396->span,yymsp[0].minor.yy82.z?&yymsp[0].minor.yy82:&yymsp[-1].minor.yy82);
  }
#line 2634 "parse.c"
        break;
      case 229:
#line 849 "parse.y"
{
    Expr *p = yygotominor.yy396 = sqlite3Expr(TK_EXISTS, 0, 0, 0);
    if( p ){
      p->pSelect = yymsp[-1].minor.yy203;
      sqlite3ExprSpan(p,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
    }else{
      sqlite3SelectDelete(yymsp[-1].minor.yy203);
    }
  }
#line 2647 "parse.c"
        break;
      case 230:
#line 861 "parse.y"
{
  yygotominor.yy396 = sqlite3Expr(TK_CASE, yymsp[-3].minor.yy396, yymsp[-1].minor.yy396, 0);
  if( yygotominor.yy396 ){
    yygotominor.yy396->pList = yymsp[-2].minor.yy162;
  }else{
    sqlite3ExprListDelete(yymsp[-2].minor.yy162);
  }
  sqlite3ExprSpan(yygotominor.yy396, &yymsp[-4].minor.yy0, &yymsp[0].minor.yy0);
}
#line 2660 "parse.c"
        break;
      case 231:
#line 872 "parse.y"
{
  yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-4].minor.yy162, yymsp[-2].minor.yy396, 0);
  yygotominor.yy162 = sqlite3ExprListAppend(yygotominor.yy162, yymsp[0].minor.yy396, 0);
}
#line 2668 "parse.c"
        break;
      case 232:
#line 876 "parse.y"
{
  yygotominor.yy162 = sqlite3ExprListAppend(0, yymsp[-2].minor.yy396, 0);
  yygotominor.yy162 = sqlite3ExprListAppend(yygotominor.yy162, yymsp[0].minor.yy396, 0);
}
#line 2676 "parse.c"
        break;
      case 243:
#line 920 "parse.y"
{
  Expr *p = 0;
  if( yymsp[-1].minor.yy82.n>0 ){
    p = sqlite3Expr(TK_COLUMN, 0, 0, 0);
    if( p ) p->pColl = sqlite3LocateCollSeq(pParse, (char*)yymsp[-1].minor.yy82.z, yymsp[-1].minor.yy82.n);
  }
  yygotominor.yy162 = sqlite3ExprListAppend(yymsp[-4].minor.yy162, p, &yymsp[-2].minor.yy82);
  if( yygotominor.yy162 ) yygotominor.yy162->a[yygotominor.yy162->nExpr-1].sortOrder = yymsp[0].minor.yy444;
}
#line 2689 "parse.c"
        break;
      case 244:
#line 929 "parse.y"
{
  Expr *p = 0;
  if( yymsp[-1].minor.yy82.n>0 ){
    p = sqlite3Expr(TK_COLUMN, 0, 0, 0);
    if( p ) p->pColl = sqlite3LocateCollSeq(pParse, (char*)yymsp[-1].minor.yy82.z, yymsp[-1].minor.yy82.n);
  }
  yygotominor.yy162 = sqlite3ExprListAppend(0, p, &yymsp[-2].minor.yy82);
  if( yygotominor.yy162 ) yygotominor.yy162->a[yygotominor.yy162->nExpr-1].sortOrder = yymsp[0].minor.yy444;
}
#line 2702 "parse.c"
        break;
      case 251:
#line 1060 "parse.y"
{yygotominor.yy444 = OE_Rollback;}
#line 2707 "parse.c"
        break;
      case 252:
#line 1061 "parse.y"
{yygotominor.yy444 = OE_Abort;}
#line 2712 "parse.c"
        break;
      case 253:
#line 1062 "parse.y"
{yygotominor.yy444 = OE_Fail;}
#line 2717 "parse.c"
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = yyact;
      yymsp->major = yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else if( yyact == YYNSTATE + YYNRULE + 1 ){
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 34 "parse.y"

  if( pParse->zErrMsg==0 ){
    if( TOKEN.z[0] ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
    }else{
      sqlite3ErrorMsg(pParse, "incomplete SQL statement");
    }
  }
#line 2784 "parse.c"
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
    /* if( yymajor==0 ) return; // not sure why this was here... */
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  sqlite3ParserARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,yymajor);
    if( yyact<YYNSTATE ){
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      if( yyendofinput && yypParser->yyidx>=0 ){
        yymajor = 0;
      }else{
        yymajor = YYNOCODE;
      }
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else if( yyact == YY_ERROR_ACTION ){
      int yymx;
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_shift_action(yypParser,YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }else{
      yy_accept(yypParser);
      yymajor = YYNOCODE;
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
