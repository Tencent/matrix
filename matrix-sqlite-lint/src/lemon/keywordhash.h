/* Hash score: 151 */
static int keywordCode(const char *z, int n){
  static const char zText[529] =
    "ABORTABLEFTEMPORARYAFTERAISELECTHENDATABASEACHECKEYANALYZELSE"
    "SCAPEXCEPTRANSACTIONATURALIKEXCLUSIVEXISTSTATEMENTRIGGEREFERENCES"
    "ANDEFAULTATTACHAVINGLOBEFOREIGNOREGEXPLAINITIALLYAUTO_INCREMENT"
    "BEGINNEREINDEXBETWEENOTNULLIMITBYCASCADEFERRABLECASECASTCOLLATE"
    "COMMITCONFLICTCONSTRAINTERSECTCREATECROSSCURRENT_DATECURRENT_TIMESTAMP"
    "LANDEFERREDELETEDESCDETACHDISTINCTDROPRAGMATCHFAILFROMFULLGROUP"
    "DATEIFIMMEDIATEINSERTINSTEADINTOFFSETISNULLJOINORDERENAMEOUTER"
    "EPLACEPRIMARYQUERYRESTRICTRIGHTROLLBACKROWHENUNIONUNIQUEUSING"
    "VACUUMVALUESVIEWHERE";
  static const unsigned char aHash[127] = {
      86,  75, 102,  85,   0,   5,   0,   0, 109,   0,  78,   0,   0,
      89,  38,  69,  87,   0, 101, 104,  91,  84,   0,   9,   0,   0,
     108,   0, 105, 100,   0,  39,   0,   0,  35,   0,   0,  59,  64,
       0,  57,  14,   0,  99,  30,  98,   0, 103,  68,   0,   0,  25,
       0,  70,  56,   0,  12,   0, 110,  32,  11,   0,  72,  34,  18,
       0,   0,   0,  42,  76,  48,  41,  45,  15,  82,   0,  31,   0,
      67,  20,   0,  65,   0,   0,   0,  58,  37,  60,   7,  81,  40,
      62,  80,   0,   1,   0,  13,  97,  95,   8,   0, 107,  77,  93,
      49,   6,  52,   0,   0,  44,  88,   0,  96,   0,  63,   0,   0,
      22,   0, 111,  46,  51,   0,   2,  50,   0, 106,
  };
  static const unsigned char aNext[111] = {
       0,   0,   0,   0,   0,   3,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   0,
       0,   0,  24,   0,  26,   0,   0,   0,   0,   0,   0,   0,  10,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  23,   0,   0,
       0,   0,   0,   0,  47,   0,  54,  43,   0,   0,   0,   0,   0,
       0,  36,  66,   0,   0,   0,  19,  55,  16,   0,  74,   0,   0,
      61,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,  33,
      90,  92,   0,  53,  94,   0,  17,   0,   0,  21,  28,  73,   0,
      29,  83,   0,  27,   0,  71,   0,
  };
  static const unsigned char aLen[111] = {
       5,   5,   4,   9,   4,   2,   5,   5,   6,   4,   3,   8,   2,
       4,   5,   3,   7,   4,   6,   6,  11,   2,   7,   4,   9,   6,
       9,   7,  10,   3,   7,   6,   6,   4,   6,   3,   7,   6,   6,
       7,   9,   3,  14,   2,   5,   5,   7,   7,   3,   7,   4,   5,
       2,   7,   3,  10,   4,   4,   7,   6,   8,  10,   9,   6,   5,
      12,  17,  12,   4,   8,   6,   4,   6,   8,   2,   4,   6,   5,
       4,   4,   4,   5,   6,   2,   9,   6,   7,   4,   2,   6,   3,
       6,   4,   5,   6,   5,   7,   7,   5,   8,   5,   8,   3,   4,
       5,   6,   5,   6,   6,   4,   5,
  };
  static const unsigned short int aOffset[111] = {
       0,   4,   7,  10,  10,  14,  19,  23,  26,  31,  33,  35,  40,
      42,  44,  48,  51,  57,  60,  65,  70,  79,  80,  86,  89,  97,
     102, 110, 116, 126, 128, 135, 140, 145, 148, 150, 150, 154, 158,
     161, 166, 171, 175, 180, 189, 192, 196, 203, 209, 209, 212, 215,
     220, 222, 223, 227, 237, 241, 245, 252, 258, 266, 273, 282, 288,
     293, 305, 305, 321, 325, 332, 338, 342, 348, 349, 356, 359, 363,
     368, 372, 376, 380, 383, 389, 391, 400, 406, 413, 416, 416, 419,
     422, 428, 432, 436, 442, 446, 453, 460, 465, 473, 478, 486, 488,
     492, 497, 503, 508, 514, 520, 523,
  };
  static const unsigned char aCode[111] = {
    TK_ABORT,      TK_TABLE,      TK_JOIN_KW,    TK_TEMP,       TK_TEMP,
    TK_OR,         TK_AFTER,      TK_RAISE,      TK_SELECT,     TK_THEN,
    TK_END,        TK_DATABASE,   TK_AS,         TK_EACH,       TK_CHECK,
    TK_KEY,        TK_ANALYZE,    TK_ELSE,       TK_ESCAPE,     TK_EXCEPT,
    TK_TRANSACTION,TK_ON,         TK_JOIN_KW,    TK_LIKE_KW,    TK_EXCLUSIVE,
    TK_EXISTS,     TK_STATEMENT,  TK_TRIGGER,    TK_REFERENCES, TK_AND,
    TK_DEFAULT,    TK_ATTACH,     TK_HAVING,     TK_LIKE_KW,    TK_BEFORE,
    TK_FOR,        TK_FOREIGN,    TK_IGNORE,     TK_LIKE_KW,    TK_EXPLAIN,
    TK_INITIALLY,  TK_ALL,        TK_AUTOINCR,   TK_IN,         TK_BEGIN,
    TK_JOIN_KW,    TK_REINDEX,    TK_BETWEEN,    TK_NOT,        TK_NOTNULL,
    TK_NULL,       TK_LIMIT,      TK_BY,         TK_CASCADE,    TK_ASC,
    TK_DEFERRABLE, TK_CASE,       TK_CAST,       TK_COLLATE,    TK_COMMIT,
    TK_CONFLICT,   TK_CONSTRAINT, TK_INTERSECT,  TK_CREATE,     TK_JOIN_KW,
    TK_CTIME_KW,   TK_CTIME_KW,   TK_CTIME_KW,   TK_PLAN,       TK_DEFERRED,
    TK_DELETE,     TK_DESC,       TK_DETACH,     TK_DISTINCT,   TK_IS,
    TK_DROP,       TK_PRAGMA,     TK_MATCH,      TK_FAIL,       TK_FROM,
    TK_JOIN_KW,    TK_GROUP,      TK_UPDATE,     TK_IF,         TK_IMMEDIATE,
    TK_INSERT,     TK_INSTEAD,    TK_INTO,       TK_OF,         TK_OFFSET,
    TK_SET,        TK_ISNULL,     TK_JOIN,       TK_ORDER,      TK_RENAME,
    TK_JOIN_KW,    TK_REPLACE,    TK_PRIMARY,    TK_QUERY,      TK_RESTRICT,
    TK_JOIN_KW,    TK_ROLLBACK,   TK_ROW,        TK_WHEN,       TK_UNION,
    TK_UNIQUE,     TK_USING,      TK_VACUUM,     TK_VALUES,     TK_VIEW,
    TK_WHERE,
  };
  int h, i;
  if( n<2 ) return TK_ID;
  h = ((charMap(z[0])*4) ^
      (charMap(z[n-1])*3) ^
      n) % 127;
  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){
    if( aLen[i]==n && sqlite3StrNICmp(&zText[aOffset[i]],z,n)==0 ){
      return aCode[i];
    }
  }
  return TK_ID;
}
int sqlite3KeywordCode(const unsigned char *z, int n){
  return keywordCode((char*)z, n);
}
