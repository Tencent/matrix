//
// Created by Yves on 2020/6/28.
//


#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "BenchMarkCommon.h"
#include "thread.h"
#include "timer.h"
#include "atomic.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

#define TAG "MemoryBenchmark"

#define printf(FMT, args...) LOGD(TAG, FMT, ##args)

#ifdef _MSC_VER
#  define PRIsize "Iu"
#else
#  define PRIsize "zu"
#endif

#define MODE_RANDOM 0
#define MODE_FIXED  1

#define SIZE_MODE_EVEN 0
#define SIZE_MODE_LINEAR  1
#define SIZE_MODE_EXP 2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct benchmark_arg benchmark_arg;
typedef struct thread_pointers thread_pointers;

struct benchmark_arg {
    size_t numthreads;
    size_t index;
    size_t mode;
    size_t size_mode;
    size_t cross_rate;
    size_t loop_count;
    size_t alloc_count;
    size_t min_size;
    size_t max_size;
    size_t accumulator;
    uint64_t ticks;
    uint64_t mops;
    atomicptr_t foreign;
    atomic32_t allocated;
    int32_t peak_allocated;
    thread_arg thread_arg;
    benchmark_arg* args;
};

struct thread_pointers {
    void** pointers;
    size_t count;
    void* next;
    atomic32_t* allocated;
};

static int benchmark_start;
static atomic32_t benchmark_threads_sync;
static atomic32_t cross_thread_counter;
static size_t alloc_scatter;
static size_t free_scatter;

//Fixed set of random numbers
static size_t random_size[2000] = {
        42301,	6214,	74627,	93605,	82351,	93731,	23458,	99146,	63110,	22890,	29895,	62570,	91269,	60237,	99940,	64028,	95252,	82540,	20157,	90844,
        80126,	82425,	74582,	70716,	69905,	55267,	81755,	85685,	17895,	97983,	39747,	45772,	71311,	84679,	10633,	96622,	15408,	38774,	31383,	79884,
        59907,	69840,	70483,	1296,	30221,	32374,	1250,	6347,	55189,	30819,	88155,	18121,	34568,	90285,	27466,	21037,	54054,	3219,	32951,	78141,
        45307,	29446,	68281,	95416,	58882,	67815,	98134,	2339,	97192,	32586,	89181,	84327,	21090,	73236,	34831,	66699,	98076,	76431,	82647,	94073,
        97868,	72757,	1078,	60005,	57151,	30003,	22000,	71102,	87440,	77764,	28581,	20434,	10655,	3167,	50417,	88166,	76793,	14801,	90802,	74376,
        31055,	49270,	1811,	99060,	66802,	94820,	37732,	40916,	89868,	46639,	91529,	74982,	44250,	89547,	91703,	93058,	71578,	85890,	77997,	36514,
        31241,	56600,	45720,	69069,	83887,	4396,	91279,	86956,	69816,	79329,	94753,	80730,	71401,	34363,	28333,	90214,	54635,	78220,	89331,	46985,
        57244,	31825,	55509,	56620,	53052,	93569,	5047,	96610,	39491,	66722,	42181,	58022,	47908,	48366,	50190,	25365,	77605,	87328,	33861,	95641,
        17634,	5996,	23188,	97862,	14954,	47887,	39062,	43748,	21261,	68198,	33379,	84240,	73622,	80466,	11015,	56289,	79325,	49658,	79713,	43863,
        60814,	24277,	19592,	89493,	82630,	95915,	48588,	16,		52733,	90874,	13240,	10894,	53813,	36264,	52999,	63240,	24143,	14198,	49899,	49364,
        6764,	24958,	92996,	64150,	15801,	51305,	24669,	29519,	25870,	1776,	58258,	6464,	87940,	20386,	25,		70486,	98172,	11585,	10171,	5157,
        7026,	38703,	63684,	86195,	95216,	78626,	79196,	67688,	56185,	8529,	41126,	16917,	70005,	82329,	67929,	68971,	10460,	29003,	8395,	24405,
        3895,	93557,	34560,	71824,	44131,	48196,	87466,	34517,	17289,	67171,	47857,	98431,	34300,	75002,	32113,	46694,	50866,	31554,	46622,	83785,
        79479,	58018,	58379,	28308,	97161,	67403,	85605,	60156,	82077,	9349,	8124,	76999,	99456,	17139,	91334,	932,	6836,	87864,	36635,	67900,
        1529,	25061,	99041,	29110,	13974,	6517,	25337,	62714,	70512,	26081,	51977,	73938,	33913,	37983,	62940,	35631,	58798,	28189,	24009,	43543,
        70222,	27381,	52259,	73386,	76069,	44347,	20046,	77165,	16347,	71345,	22879,	50230,	37429,	68115,	70002,	89613,	69316,	60787,	58923,	51295,
        40284,	96998,	35508,	87173,	30133,	12407,	28667,	40805,	74633,	43303,	45342,	37778,	20934,	42910,	52266,	90173,	90947,	92460,	48039,	74023,
        72091,	69029,	18019,	4521,	18104,	93795,	52380,	75881,	74698,	22760,	74315,	47044,	98770,	3146,	20657,	8725,	53817,	23967,	84651,	36005,
        5331,	90981,	94529,	14026,	70400,	50286,	96587,	50844,	81796,	61110,	36776,	22624,	93962,	12909,	2490,	63451,	16560,	72472,	73436,	52307,
        3521,	21280,	4160,	41326,	118,	52625,	78163,	91680,	97730,	44749,	72617,	6735,	38840,	30276,	21979,	3013,	98953,	38592,	63622,	14286,
        26609,	45281,	96512,	1266,	15845,	13075,	63503,	55194,	57552,	347,	28896,	66327,	27818,	18314,	36467,	32685,	79917,	67475,	6482,	27502,
        27895,	81645,	88851,	63447,	97801,	59402,	61663,	62266,	11723,	53664,	31123,	50583,	52561,	93801,	60911,	95186,	1523,	13959,	75972,	83397,
        98867,	13677,	26462,	60373,	50646,	29253,	69891,	23620,	97227,	70196,	685,	46204,	72222,	76264,	79291,	32699,	88327,	950,	86803,	67946,
        65417,	68473,	34521,	81124,	29656,	78884,	84,		11889,	93078,	63564,	93310,	73297,	11835,	14176,	9106,	51926,	49470,	90576,	54176,	79782,
        68503,	65790,	25873,	71786,	48914,	1729,	81335,	59162,	96305,	46442,	19220,	64437,	80518,	59209,	56079,	83046,	2171,	11606,	65951,	44268,
        75604,	75013,	336,	46693,	56101,	69968,	26795,	55351,	2926,	51530,	4574,	65148,	36917,	70077,	82638,	53513,	36907,	4469,	94792,	1248,
        62907,	93233,	44798,	35998,	28031,	86786,	15581,	9546,	3067,	65953,	42127,	93017,	73500,	47493,	34152,	42566,	99077,	63233,	43072,	12191,
        94924,	46953,	3641,	58957,	2971,	11982,	13551,	69306,	82293,	25723,	36647,	71048,	76501,	58174,	33792,	14359,	6882,	46120,	14201,	36645,
        61886,	99622,	58258,	70316,	66228,	53947,	32478,	36276,	43902,	37878,	67333,	764,	83139,	86356,	7510,	61421,	45025,	9383,	38610,	93338,
        36981,	85186,	53596,	18730,	11841,	79036,	61283,	86019,	87281,	71951,	95833,	84380,	19627,	9626,	97079,	72846,	20890,	29832,	93108,	7976,
        62701,	17762,	21406,	70085,	40096,	19644,	57467,	25989,	32652,	23781,	591,	17087,	98696,	22640,	89961,	48544,	52682,	85931,	44827,	84419,
        13891,	82403,	25915,	64876,	1092,	63868,	85965,	92472,	90590,	6645,	82417,	12635,	44125,	17017,	28025,	98728,	21429,	83742,	74565,	94388,
        64814,	56266,	5530,	82469,	32833,	78753,	76171,	82758,	44987,	58705,	75785,	22708,	26724,	77955,	84443,	55066,	44605,	92125,	1225,	78907,
        92345,	27374,	74731,	66764,	16133,	18429,	82040,	57794,	75229,	63230,	84688,	40516,	64711,	52314,	88591,	45887,	24727,	89863,	48026,	60643,
        89466,	74838,	84712,	87291,	9805,	38722,	26550,	88896,	12322,	89245,	97273,	26602,	68253,	42065,	13831,	80784,	63967,	10057,	48370,	90991,
        22164,	36692,	74691,	99469,	45153,	92702,	71622,	24003,	37475,	34155,	49983,	42747,	95163,	38986,	16633,	71055,	64068,	51835,	90775,	64181,
        41674,	52152,	86065,	5176,	34552,	11045,	95891,	37901,	50122,	85359,	78631,	67931,	37636,	86730,	25850,	61677,	40015,	71019,	44850,	40945,
        41041,	35159,	424,	53685,	97795,	77347,	31533,	94160,	27348,	63354,	26319,	40107,	93098,	87100,	85148,	95354,	7848,	26932,	59006,	12235,
        17688,	81474,	78287,	84516,	61413,	68919,	38238,	57286,	54401,	18562,	25023,	88031,	54460,	58083,	83677,	18915,	31861,	85374,	87576,	59732,
        38534,	65960,	54797,	77939,	38196,	87986,	75270,	10987,	96933,	43058,	82075,	75153,	13907,	74334,	35358,	53207,	83535,	54154,	70746,	2441,
        51129,	32650,	97619,	91451,	76454,	16015,	40537,	14889,	16211,	63991,	97437,	32730,	5189,	79601,	67330,	95751,	62744,	19292,	75233,	18084,
        37573,	40609,	42724,	93700,	33349,	20980,	40936,	94332,	22383,	1416,	72096,	52578,	50442,	30252,	11023,	46171,	36137,	65402,	99112,	67145,
        22003,	54711,	69025,	10911,	26043,	60371,	38943,	23493,	88350,	71488,	58854,	18883,	21344,	86625,	58563,	41158,	18495,	48188,	942,	66045,
        31255,	60368,	97474,	57619,	71639,	33064,	46735,	35467,	20637,	5041,	83609,	38123,	49670,	76412,	66878,	68287,	85916,	91311,	60061,	36099,
        28495,	90856,	80025,	43741,	69408,	83471,	36181,	95935,	35330,	50544,	62083,	43570,	89033,	33867,	36027,	15254,	30197,	34458,	39364,	74865,
        24812,	37022,	5954,	72980,	17251,	38420,	25210,	22888,	98643,	24684,	21543,	61874,	33534,	3202,	52652,	9661,	54900,	57117,	10607,	65871,
        36892,	96446,	73068,	42280,	98969,	18166,	92463,	14979,	89072,	87142,	70769,	19354,	28149,	4840,	80852,	6332,	82116,	76515,	31224,	98426,
        49973,	14581,	41012,	67269,	13159,	17895,	23331,	89950,	33053,	59052,	75888,	37680,	33410,	64278,	86775,	58344,	56445,	94767,	73347,	68064,
        15906,	55708,	90978,	50676,	8131,	78335,	18423,	42033,	16784,	77371,	1779,	48452,	68259,	20383,	84196,	99516,	24988,	39436,	84941,	53532,
        82934,	56012,	86532,	26276,	50102,	95350,	92394,	72824,	91468,	51207,	46677,	74147,	17396,	73951,	61050,	84191,	83137,	53207,	71850,	73087,
        60066,	58680,	40205,	40903,	88778,	70023,	52889,	58243,	33085,	94965,	57111,	99423,	67011,	83618,	95835,	20599,	11034,	24152,	25380,	27540,
        73219,	64152,	42450,	45985,	75245,	86575,	70394,	62669,	51370,	96295,	59743,	85700,	64532,	3403,	50481,	60582,	54875,	92772,	74427,	75038,
        30668,	93071,	8085,	14688,	67282,	80982,	5324,	94615,	19268,	218,	63596,	74942,	8766,	48092,	7247,	35807,	1617,	67115,	39693,	31807,
        50062,	43147,	24706,	80094,	75012,	10409,	4959,	51613,	24339,	41384,	68303,	29069,	9971,	66575,	71694,	54059,	47732,	76043,	13003,	71451,
        72224,	31273,	66756,	52916,	73991,	22734,	41818,	15810,	28867,	44316,	74895,	60210,	92030,	54114,	98692,	92327,	60861,	64847,	44885,	53175,
        40713,	66802,	194,	58710,	76645,	20203,	66546,	98777,	5418,	38879,	5263,	25406,	77388,	20914,	53056,	73849,	19265,	22355,	39435,	85914,
        494,	19760,	20364,	903,	31617,	85835,	9835,	42381,	2442,	29412,	69033,	84184,	91987,	64246,	75227,	25048,	7900,	45999,	10019,	41123,
        90308,	72412,	90235,	47141,	93995,	76257,	96004,	52493,	91642,	30038,	85834,	57092,	50095,	66871,	3419,	41040,	80452,	92769,	61837,	30941,
        46436,	67831,	82636,	82534,	31242,	19962,	61308,	19447,	37538,	28143,	49442,	418,	5697,	91278,	75828,	23803,	99969,	95404,	54686,	10389,
        49364,	57913,	23776,	37305,	99796,	52566,	55563,	34806,	81928,	74742,	59961,	8783,	78909,	5754,	53610,	46793,	75199,	39462,	41905,	86633,
        2510,	17823,	11402,	57000,	71562,	57163,	12648,	73365,	24722,	1369,	33470,	24342,	60541,	98872,	83077,	28367,	28518,	17001,	76839,	48127,
        98368,	49622,	21930,	59294,	81371,	14056,	20446,	90327,	9004,	31843,	35487,	93502,	19142,	17218,	43677,	42361,	8538,	24342,	45545,	17763,
        95904,	83989,	93051,	86548,	17700,	40673,	30548,	25636,	84204,	85583,	48442,	45686,	57349,	67781,	99236,	60399,	26916,	20719,	12654,	29727,
        54503,	21347,	15072,	65239,	73085,	97065,	34856,	57254,	20993,	87740,	2358,	7401,	34081,	84586,	77782,	38801,	66088,	71230,	97174,	50108,
        25173,	62865,	87387,	90522,	60616,	82589,	19470,	66429,	72246,	30131,	99837,	38489,	83145,	87510,	78816,	99793,	1713,	93034,	63952,	14964,
        99553,	6925,	8645,	90572,	49178,	38455,	97624,	91051,	39833,	11584,	50823,	52915,	37241,	38347,	51895,	97465,	37295,	86221,	69851,	12563,
        63686,	6783,	23498,	9999,	48545,	99429,	36814,	20903,	13473,	89449,	55533,	30269,	92525,	78863,	32798,	98799,	6993,	37376,	24058,	82949,
        19363,	26898,	58435,	40545,	21173,	24598,	74477,	80100,	82134,	10642,	91659,	45488,	77142,	12812,	5135,	30842,	52189,	38228,	57309,	17403,
        11373,	21568,	60916,	36950,	55835,	55785,	23423,	89045,	42861,	98577,	27943,	75686,	59340,	20697,	23929,	59319,	87107,	34460,	71684,	26064,
        77777,	89852,	7287,	31625,	35375,	92061,	43376,	26687,	44255,	28676,	97442,	55313,	31523,	65877,	9220,	55068,	99424,	18327,	66865,	37648,
        28095,	61967,	28769,	81395,	3143,	90859,	37048,	40894,	64647,	23697,	79689,	62952,	28921,	37991,	21013,	86140,	1116,	56753,	93196,	16310,
        98576,	72886,	46453,	8654,	50373,	99234,	6113,	97632,	62708,	61101,	33931,	87983,	30152,	45779,	98494,	55851,	76132,	32239,	87613,	55196,
        2918,	12991,	44375,	69565,	49627,	17472,	60832,	5379,	47572,	45556,	74889,	95921,	62036,	96059,	3909,	52552,	55285,	20199,	25930,	99967,
        68458,	7181,	61674,	18479,	80956,	29791,	25413,	91060,	99125,	23566,	51800,	50744,	89097,	31911,	70605,	42816,	66700,	46028,	48770,	63851,
        64023,	35792,	27000,	52926,	1263,	87504,	42319,	90060,	22659,	6024,	99993,	88029,	99170,	72740,	15197,	90232,	59519,	92011,	84116,	17503,
        33985,	45034,	2771,	59768,	20541,	76922,	97173,	59769,	70795,	10877,	25759,	64087,	96456,	76387,	25410,	82611,	47991,	94311,	18423,	23087,
        20096,	59066,	7965,	29232,	37590,	32338,	74190,	16061,	87403,	86026,	26953,	24574,	18596,	53276,	81813,	84669,	59980,	26175,	35430,	10620,
        50992,	46236,	57904,	56494,	23357,	93792,	42763,	43735,	39952,	53049,	94826,	68957,	62419,	30629,	72587,	76494,	66848,	95907,	6380,	98019,
        81166,	82102,	86605,	15449,	6687,	69255,	56751,	52925,	84112,	67773,	59353,	15543,	36601,	3387,	79601,	83257,	49246,	76874,	56185,	793,
        59738,	52149,	26613,	66307,	8234,	24065,	92988,	71189,	79064,	71518,	32203,	67702,	9543,	98621,	93075,	34932,	15937,	87714,	71486,	75270,
        35729,	41826,	34535,	41952,	14408,	89411,	25440,	34547,	64653,	1052,	2091,	55795,	24536,	50576,	61768,	25630,	48362,	74274,	82782,	37429,
        26345,	45185,	69504,	68905,	64931,	44057,	69869,	76963,	38003,	25727,	93730,	4799,	87357,	88156,	95203,	33217,	45889,	57727,	37421,	89580,
        18862,	49760,	11881,	41838,	35387,	83063,	26361,	328,	6753,	87380,	70890,	28029,	74084,	42224,	8151,	29816,	90197,	76425,	14733,	82706,
        59843,	98843,	74236,	7062,	61936,	69254,	64153,	72080,	72850,	88695,	5871,	72131,	1032,	4002,	99201,	7426,	88307,	3533,	34065,	72500,
        44972,	97675,	72795,	8365,	37366,	62425,	22108,	83202,	29045,	1772,	88598,	81519,	41398,	16707,	34486,	2272,	92916,	47652,	99365,	95005,
        39416,	77780,	28778,	75294,	81891,	95050,	37955,	28803,	19527,	67,		69492,	28635,	96926,	59073,	4874,	75644,	68338,	41368,	71873,	71289,
        67924,	4733,	14164,	17892,	38828,	84141,	66764,	69505,	95978,	65222,	58685,	848,	24990,	72158,	34250,	66702,	16290,	41201,	48260,	62154,
        34092,	30249,	20908,	54756,	93067,	45932,	58912,	4959,	61289,	46612,	36779,	97765,	59802,	43280,	39452,	6689,	36436,	32689,	27298,	90154,
        52142,	27957,	32314,	41858,	50225,	88155,	84872,	66423,	47164,	50056,	70331,	45054,	35502,	41234,	95896,	41575,	14100,	4617,	68543,	37829,
        5296,	11231,	89156,	86146,	72106,	83030,	35808,	64542,	28741,	15339,	26443,	10559,	52283,	56443,	45477,	37830,	28763,	98269,	47644,	20443,
        14448,	44861,	6370,	80492,	85001,	72737,	51720,	31424,	99527,	22420,	38776,	62662,	27502,	36580,	3307,	83960,	81721,	51624,	64182,	45836,
        1142,	33260,	75913,	94960,	16724,	92842,	89870,	92244,	24408,	19935,	94145,	25952,	60513,	98704,	69713,	46450,	9281,	16864,	7195,	96912,
        24452,	93864,	23523,	10910,	71732,	27237,	25286,	40790,	66746,	88387,	35004,	99859,	70027,	31707,	41359,	59875,	52986,	24154,	39601,	13952,
        14845,	36397,	78033,	97093,	43471,	79587,	17740,	2334,	20975,	70433,	48316,	20809,	89126,	346,	53303,	95290,	46100,	98972,	25761,	56374,
        72548,	522,	60873,	56778,	69867,	58418,	11277,	6346,	76779,	35626,	41615,	77473,	41372,	27889,	38640,	64797,	74422,	59716,	6530,	17798,
        86356,	6037,	51450,	32010,	5755,	56904,	35165,	5598,	17784,	92297,	61238,	76340,	53938,	93742,	25511,	33923,	77947,	91562,	99162,	90512,
        96749,	60367,	32742,	47696,	35649,	35581,	91908,	23524,	73036,	58325,	1642,	84646,	91901,	25729,	82064,	81835,	19387,	61543,	68288,	95618,
        86715,	75797,	29530,	40875,	68019,	23736,	46328,	47927,	7882,	77085,	20355,	74989,	71929,	24800,	13585,	29510,	81173,	53946,	12017,	57628,
        67465,	1584,	68135,	48915,	36062,	15397,	32156,	98903,	63257,	92838,	39730,	51909,	86167,	3358,	1553,	54615,	83915,	4448,	25246,	28089,
        7795,	71271,	7179,	37688,	76932,	54541,	9356,	95744,	60661,	31195,	17491,	43766,	45565,	17181,	47779,	22976,	32372,	30265,	10905,	21731
};
static size_t random_size_lin[2000];
static size_t random_size_exp[2000];
static size_t* random_size_arr;

static size_t num_alloc_ops[311] = {
        58,	56,	65,	85,	83,	70,	33,	58,	69,	32,
        70,	88,	58,	81,	98,	91,	57,	32,	42,	88,
        84,	93,	80,	89,	34,	83,	86,	48,	32,	70,
        77,	72,	56,	50,	81,	59,	95,	69,	96,	38,
        44,	97,	74,	64,	85,	53,	85,	78,	58,	48,
        61,	91,	89,	57,	68,	61,	64,	74,	46,	34,
        48,	82,	86,	93,	37,	66,	59,	50,	47,	36,
        79,	33,	71,	43,	63,	80,	57,	87,	65,	76,
        59,	81,	50,	85,	75,	93,	95,	45,	61,	55,
        32,	52,	68,	62,	97,	34,	65,	88,	97,	85,
        88,	39,	44,	39,	55,	45,	40,	50,	44,	96,
        94,	99,	91,	84,	48,	54,	35,	76,	91,	47,
        59,	34,	69,	78,	71,	67,	81,	91,	43,	78,
        41,	47,	49,	65,	77,	62,	31,	56,	50,	94,
        40,	48,	42,	96,	63,	33,	42,	99,	81,	67,
        74,	65,	83,	47,	46,	44,	41,	46,	50,	42,
        31,	38,	79,	85,	47,	65,	94,	73,	49,	96,
        30,	95,	59,	50,	32,	94,	71,	89,	85,	94,
        83,	80,	68,	41,	84,	42,	64,	72,	96,	34,
        67,	58,	79,	87,	80,	57,	95,	67,	99,	38,
        81,	93,	65,	67,	35,	61,	80,	81,	59,	72,
        46,	49,	39,	64,	51,	66,	36,	72,	78,	46,
        61,	99,	32,	74,	78,	68,	34,	91,	65,	89,
        74,	37,	38,	76,	30,	68,	58,	36,	39,	92,
        73,	41,	57,	68,	68,	61,	64,	71,	58,	42,
        36,	53,	58,	82,	67,	78,	59,	75,	96,	64,
        92,	68,	89,	75,	73,	97,	72,	67,	32,	85,
        47,	61,	36,	43,	86,	43,	68,	99,	43,	87,
        39,	66,	78,	94,	60,	45,	35,	98,	36,	76,
        91,	98,	87,	41,	35,	85,	55,	93,	60,	59,
        34,	96,	86,	44,	84,	50,	89,	81,	56,	88,
        74
};

static size_t num_free_ops[257] = {
        94,	51,	88,	47,	51,	76,	99,	47,	48,	96,
        74,	61,	51,	40,	76,	31,	41,	61,	93,	55,
        78,	87,	83,	35,	35,	59,	58,	86,	97,	80,
        35,	44,	83,	30,	98,	35,	71,	55,	54,	93,
        95,	78,	49,	51,	89,	42,	85,	81,	47,	82,
        32,	40,	38,	52,	98,	30,	86,	91,	91,	84,
        65,	62,	93,	33,	93,	67,	59,	81,	96,	91,
        70,	69,	42,	81,	94,	80,	66,	58,	71,	35,
        40,	89,	42,	45,	68,	91,	40,	33,	57,	42,
        61,	55,	54,	74,	88,	60,	84,	32,	49,	92,
        69,	41,	82,	38,	93,	32,	74,	67,	32,	40,
        35,	90,	99,	81,	43,	51,	92,	72,	49,	90,
        54,	58,	88,	62,	66,	30,	52,	82,	84,	99,
        58,	77,	95,	79,	50,	32,	45,	31,	98,	45,
        38,	96,	37,	40,	69,	58,	65,	79,	44,	66,
        60,	45,	83,	40,	55,	62,	58,	33,	39,	64,
        56,	53,	64,	95,	47,	70,	73,	75,	98,	41,
        77,	91,	32,	56,	35,	77,	73,	93,	60,	32,
        47,	45,	30,	88,	68,	43,	32,	89,	57,	44,
        38,	42,	52,	88,	65,	42,	40,	55,	65,	47,
        31,	35,	64,	88,	63,	59,	61,	84,	52,	40,
        67,	97,	92,	61,	93,	30,	87,	96,	96,	93,
        76,	47,	43,	41,	49,	83,	69,	52,	82,	54,
        50,	57,	79,	62,	85,	49,	98,	66,	34,	34,
        65,	34,	44,	38,	87,	69,	82,	41,	74,	59,
        97,	40,	56,	66,	91,	64,	93
};

static size_t primes[] = {
        7, 11, 13, 17, 19, 23, 29,
        31, 37, 41, 43, 47, 53, 59,
        61, 67, 71, 73, 79, 83, 89, 97,
        101, 103, 107, 109, 113, 127,
        131, 137, 139, 149, 151, 157,
        163, 167, 173, 179, 181, 191,
        193, 197, 199
};

#if defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#else
#include <unistd.h>
#include <sys/resource.h>
//#include "../../../../../../../../Library/Android/sdk/ndk-bundle/sysroot/usr/include/search.h"

#endif

int
benchmark_run(int argc, char** argv);

static size_t
get_process_memory_usage(void) {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters;
	memset(&counters, 0, sizeof(counters));
	counters.cb = sizeof(counters);
	GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
	return counters.WorkingSetSize;
#elif defined(__APPLE__)
    struct task_basic_info info;
	mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO,
	    (task_info_t)&info, &info_count) != KERN_SUCCESS)
		return 0;
	return info.resident_size;
#else
    long rss = 0L;
    FILE* fp = fopen("/proc/self/statm", "r");
    if (!fp)
        return 0;
    if (fscanf(fp, "%*s%ld", &rss) != 1)
        rss = 0;
    fclose(fp);
    return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
#endif
}

static size_t
get_process_peak_memory_usage(void) {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters;
	memset(&counters, 0, sizeof(counters));
	counters.cb = sizeof(counters);
	GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
	return counters.PeakWorkingSetSize;
#else
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
#if defined(__APPLE__)
    return (size_t)rusage.ru_maxrss;
#else
    return (size_t)rusage.ru_maxrss * 1024;
#endif
#endif
}

static void
put_cross_thread_memory(atomicptr_t* ptr, thread_pointers* pointers) {
    void* prev;
    uintptr_t newval;
    do {
        prev = atomic_load_ptr(ptr);
        pointers->next = (void*)((uintptr_t)prev & ~(uintptr_t)0xF);
        newval = (uintptr_t)pointers | (atomic_incr32(&cross_thread_counter) & 0xF);
    } while (!atomic_cas_ptr(ptr, (void*)newval, prev));
}

static thread_pointers*
get_cross_thread_memory(atomicptr_t* ptr) {
    void* prev;
    thread_pointers* current;
    do {
        prev = atomic_load_ptr(ptr);
        current = (thread_pointers*)((uintptr_t)prev & ~(uintptr_t)0xF);
    } while (current && !atomic_cas_ptr(ptr, (void*)((uintptr_t)atomic_incr32(&cross_thread_counter) & 0xF), prev));
    return current;
}

static void
benchmark_worker(void* argptr) {
    benchmark_arg* arg = (benchmark_arg*)argptr;
    thread_pointers* foreign = 0;
    void** pointers;
    const size_t random_size_count = (sizeof(random_size) / sizeof(random_size[0]));
    const size_t alloc_ops_count = (sizeof(num_alloc_ops) / sizeof(num_alloc_ops[0]));
    const size_t free_ops_count = (sizeof(num_free_ops) / sizeof(num_free_ops[0]));
    const size_t alignment[3] = { 0, 8, 16 };

    size_t alloc_idx = 0;
    size_t free_idx = 0;
    size_t iop;
    uint64_t tick_start, ticks_elapsed;
    int32_t allocated;
    size_t cross_index = 0;
    int aborted = 0;

    benchmark_thread_initialize();

    size_t pointers_size = sizeof(void*) * arg->alloc_count;
    pointers = static_cast<void **>(benchmark_malloc(16, pointers_size));
    memset(pointers, 0, pointers_size);
    atomic_add32(&arg->allocated, (int32_t)pointers_size);

    while (!benchmark_start)
        thread_sleep(10);

    arg->ticks = 0;
    arg->mops = 0;
    for (size_t iter = 0; iter < 2; ++iter) {
        size_t size_index = ((arg->index + 1) * ((iter + 1) * 37)) % random_size_count;

        uint64_t iter_ticks_elapsed = 0;
        int do_foreign = 1;

        for (size_t iloop = 0; iloop < arg->loop_count; ++iloop) {

            foreign = get_cross_thread_memory(&arg->foreign);

            allocated = 0;
            tick_start = timer_current();

            const size_t free_op_count = num_free_ops[(iter + iloop) % free_ops_count];
            const size_t alloc_op_count = num_alloc_ops[(iter + iloop) % alloc_ops_count];

            for (iop = 0; iop < free_op_count; ++iop) {
                if (pointers[free_idx]) {
                    allocated -= *(int32_t*)pointers[free_idx];
                    benchmark_free(pointers[free_idx]);
                    ++arg->mops;
                    pointers[free_idx] = 0;
                }

                free_idx = (free_idx + free_scatter) % arg->alloc_count;
            }

            while (foreign) {
                int32_t foreign_allocated = 0;
                for (iop = 0; iop < foreign->count; ++iop) {
                    foreign_allocated -= *(int32_t*)foreign->pointers[iop];
                    benchmark_free(foreign->pointers[iop]);
                    ++arg->mops;
                }

                void* next = foreign->next;
                foreign_allocated -= (int32_t)(foreign->count * sizeof(void*) + sizeof(thread_pointers));
                atomic_add32(foreign->allocated, foreign_allocated);
                benchmark_free(foreign->pointers);
                benchmark_free(foreign);
                arg->mops += 2;
                foreign = (thread_pointers *)next;
            }

            for (iop = 0; iop < alloc_op_count; ++iop) {
                if (pointers[alloc_idx]) {
                    allocated -= *(int32_t*)pointers[alloc_idx];
                    benchmark_free(pointers[alloc_idx]);
                    ++arg->mops;
                }
                size_t size = arg->min_size;
                if (arg->mode == MODE_RANDOM)
                    size = random_size_arr[(size_index + 2) % random_size_count];
                pointers[alloc_idx] = benchmark_malloc((size < 4096) ? alignment[(size_index + iop) % 3] : 0, size);
                //Make sure to write to each page to commit it for measuring memory usage
                *(int32_t*)pointers[alloc_idx] = (int32_t)size;
                if (size) {
                    size_t num_pages = (size - 1) / 4096;
                    for (size_t page = 1; page < num_pages; ++page)
                        *((char*)(pointers[alloc_idx]) + (page * 4096)) = 1;
                    *((char*)(pointers[alloc_idx]) + (size - 1)) = 1;
                }
                allocated += (int32_t)size;
                ++arg->mops;

                alloc_idx = (alloc_idx + alloc_scatter) % arg->alloc_count;
                size_index = (size_index + 1) % random_size_count;
            }

            foreign = 0;
            if (arg->cross_rate && ((iloop % arg->cross_rate) == 0) && (do_foreign > 0)) {
                foreign = (thread_pointers *)benchmark_malloc(16, sizeof(thread_pointers));
                foreign->count = alloc_op_count;
                foreign->pointers = (void **)benchmark_malloc(16, sizeof(void*) * alloc_op_count);
                foreign->allocated = &arg->allocated;
                allocated += (int32_t)(alloc_op_count * sizeof(void*) + sizeof(thread_pointers));
                arg->mops += 2;

                for (iop = 0; iop < alloc_op_count; ++iop) {
                    size_t size = arg->min_size;
                    if (arg->mode == MODE_RANDOM)
                        size = random_size_arr[(size_index + 2) % random_size_count];
                    foreign->pointers[iop] = benchmark_malloc((size < 4096) ? alignment[(size_index + iop) % 3] : 0, size);
                    *(int32_t*)foreign->pointers[iop] = (int32_t)size;
                    if (size) {
                        size_t num_pages = (size - 1) / 4096;
                        for (size_t page = 1; page < num_pages; ++page)
                            *((char*)(foreign->pointers[iop]) + (page * 4096)) = 1;
                        *((char*)(foreign->pointers[iop]) + (size - 1)) = 1;
                    }
                    allocated += (int32_t)size;
                    ++arg->mops;

                    size_index = (size_index + 1) % random_size_count;
                }
            }

            ticks_elapsed = timer_current() - tick_start;
            iter_ticks_elapsed += ticks_elapsed;
            arg->ticks += ticks_elapsed;

            int32_t current_allocated = atomic_add32(&arg->allocated, allocated);
            if (arg->peak_allocated < current_allocated)
                arg->peak_allocated = current_allocated;

            if (foreign) {
                cross_index = (cross_index + 1) % arg->numthreads;
                if ((arg->numthreads > 1) && (cross_index == arg->index))
                    cross_index = (cross_index + 1) % arg->numthreads;
                benchmark_arg* cross_arg = &arg->args[cross_index];
                put_cross_thread_memory(&cross_arg->foreign, foreign);
                foreign = 0;
            }

            if (atomic_load32(&benchmark_threads_sync) > 0)
                do_foreign = 0; //one thread completed

            if (timer_ticks_to_seconds(iter_ticks_elapsed) > 120) {
                aborted = 1;
                break;
            }
        }

        /*
        allocated = 0;
        tick_start = timer_current();
        for (size_t iptr = 0; iptr < arg->alloc_count; ++iptr) {
            if (!pointers[iptr]) {
                size_t size = arg->min_size;
                if (arg->mode == MODE_RANDOM)
                    size = random_size_exp[(size_index + 2) % random_size_count];
                pointers[iptr] = benchmark_malloc((size < 4096) ? alignment[size_index % 3] : 0, size);
                *(int32_t*)pointers[iptr] = (int32_t)size;
                size_t num_pages = (size - 1) / 4096;
                for (size_t page = 1; page < num_pages; ++page)
                    *((char*)(pointers[iptr]) + (page * 4096)) = 1;
                *((char*)(pointers[iptr]) + (size - 1)) = 1;
                allocated += (int32_t)size;
                ++arg->mops;

                size_index = (size_index + 1) % random_size_count;
            }
        }
        ticks_elapsed = timer_current() - tick_start;

        atomic_add32(&arg->allocated, allocated);

        iter_ticks_elapsed += ticks_elapsed;
        arg->ticks += ticks_elapsed;
        */

        //Sync and allow main thread to gather allocation stats
        atomic_incr32(&benchmark_threads_sync);
        do {
            foreign = get_cross_thread_memory(&arg->foreign);
            if (foreign) {
                tick_start = timer_current();
                while (foreign) {
                    allocated = 0;
                    for (iop = 0; iop < foreign->count; ++iop) {
                        allocated -= *(int32_t*)foreign->pointers[iop];
                        benchmark_free(foreign->pointers[iop]);
                        ++arg->mops;
                    }

                    void* next = foreign->next;
                    allocated -= (int32_t)(foreign->count * sizeof(void*) + sizeof(thread_pointers));
                    atomic_add32(foreign->allocated, allocated);
                    benchmark_free(foreign->pointers);
                    benchmark_free(foreign);
                    arg->mops += 2;
                    foreign = static_cast<thread_pointers *>(next);
                }
                ticks_elapsed = timer_current() - tick_start;
                arg->ticks += ticks_elapsed;
            }

            thread_sleep(1);
            thread_fence();
        } while (atomic_load32(&benchmark_threads_sync));

        allocated = 0;
        tick_start = timer_current();
        for (size_t iptr = 0; iptr < arg->alloc_count; ++iptr) {
            if (pointers[iptr]) {
                allocated -= *(int32_t*)pointers[iptr];
                benchmark_free(pointers[iptr]);
                ++arg->mops;
                pointers[iptr] = 0;
            }
        }
        ticks_elapsed = timer_current() - tick_start;
        atomic_add32(&arg->allocated, allocated);

        iter_ticks_elapsed += ticks_elapsed;
        arg->ticks += ticks_elapsed;

        thread_sleep(10);

        foreign = get_cross_thread_memory(&arg->foreign);
        while (foreign) {
            for (iop = 0; iop < foreign->count; ++iop)
                benchmark_free(foreign->pointers[iop]);

            void* next = foreign->next;
            benchmark_free(foreign->pointers);
            benchmark_free(foreign);
            foreign = static_cast<thread_pointers *>(next);
        }

        printf(".");
        fflush(stdout);

        //printf(" %.2f ", timer_ticks_to_seconds(iter_ticks_elapsed));
        //if (aborted)
        //	printf("(aborted) ");
        //fflush(stdout);
        aborted = 0;
    }

    //Sync threads
    thread_sleep(1000);
    thread_fence();
    atomic_incr32(&benchmark_threads_sync);
    do {
        foreign = get_cross_thread_memory(&arg->foreign);
        if (foreign) {
            tick_start = timer_current();
            while (foreign) {
                allocated = 0;
                for (iop = 0; iop < foreign->count; ++iop) {
                    allocated -= *(int32_t*)foreign->pointers[iop];
                    benchmark_free(foreign->pointers[iop]);
                    ++arg->mops;
                }

                void* next = foreign->next;
                allocated -= (int32_t)(foreign->count * sizeof(void*) + sizeof(thread_pointers));
                atomic_add32(foreign->allocated, allocated);
                benchmark_free(foreign->pointers);
                benchmark_free(foreign);
                arg->mops += 2;
                foreign = static_cast<thread_pointers *>(next);
            }
            ticks_elapsed = timer_current() - tick_start;
            arg->ticks += ticks_elapsed;
        }

        thread_sleep(1);
        thread_fence();
    } while (atomic_load32(&benchmark_threads_sync));

    benchmark_free(pointers);
    atomic_add32(&arg->allocated, -(int32_t)pointers_size);

    benchmark_thread_finalize();

    arg->accumulator += arg->mops;

    thread_exit(0);
}

int
benchmark_run(int argc, char** argv) {
    if (timer_initialize() < 0)
        return -1;
    if (benchmark_initialize() < 0)
        return -2;

    if ((argc < 9) || (argc > 10)) {
        printf("Usage: benchmark <thread count> <mode> <size mode> <cross rate> <loops> <allocs> <op count> <min size> <max size>\n"
               "         <thread count>     Number of execution threads\n"
               "         <mode>             0 for random size [min, max], 1 for fixed size (min)\n"
               "         <size mode>        0 for even distribution, 1 for linear dropoff, 2 for exp dropoff\n"
               "         <cross rate>       Rate of cross-thread deallocations (every n iterations), 0 for none\n"
               "         <loops>            Number of loops in each iteration (0 for default, 800k)\n"
               "         <allocs>           Number of concurrent allocations in each thread, (0 for default, 10k)\n"
               "         <op count>         Iteration operation count\n"
               "         <min size>         Minimum size for random mode, fixed size for fixed mode\n"
               "         <max size>         Maximum size for random mode, ignored for fixed mode\n");
        return -3;
    }

    size_t thread_count = (size_t)strtol(argv[1], 0, 10);
    size_t mode = (size_t)strtol(argv[2], 0, 10);
    size_t size_mode = (size_t)strtol(argv[3], 0, 10);
    size_t cross_rate = (size_t)strtol(argv[4], 0, 10);
    size_t loop_count = (size_t)strtol(argv[5], 0, 10);
    size_t alloc_count = (size_t)strtol(argv[6], 0, 10);
    size_t op_count = (size_t)strtol(argv[7], 0, 10);
    size_t min_size = (size_t)strtol(argv[8], 0, 10);
    size_t max_size = (argc > 9) ? (size_t)strtol(argv[9], 0, 10) : 0;

    if ((thread_count < 1) || (thread_count > 64)) {
        printf("Invalid thread count: %s\n", argv[1]);
        return -3;
    }
    if ((mode != MODE_RANDOM) && (mode != MODE_FIXED)) {
        printf("Invalid mode: %s\n", argv[2]);
        return -3;
    }
    if ((size_mode != SIZE_MODE_EVEN) && (size_mode != SIZE_MODE_LINEAR) && (size_mode != SIZE_MODE_EXP)) {
        printf("Invalid size mode: %s\n", argv[3]);
        return -3;
    }
    if (!loop_count || (loop_count > 0x00FFFFFF))
        loop_count = 800*1024;
    if (!alloc_count || (alloc_count > 0x00FFFFFF))
        alloc_count = 10*1024;
    if (!op_count || (op_count > 0x00FFFFFF))
        op_count = 1000;
    if ((mode == MODE_RANDOM) && (!max_size || (max_size < min_size))) {
        printf("Invalid min/max size for random mode: %s %s\n", argv[7], (argc > 8) ? argv[8] : "<missing>");
        return -3;
    }
    if ((mode == MODE_FIXED) && !min_size) {
        printf("Invalid size for fixed mode: %s\n", argv[7]);
        return -3;
    }

    if (thread_count == 1)
        cross_rate = 0;

    size_t iprime = 0;
    alloc_scatter = primes[iprime];
    while ((alloc_count % alloc_scatter) == 0)
        alloc_scatter = primes[++iprime];
    free_scatter = primes[++iprime];
    while ((alloc_count % free_scatter) == 0)
        free_scatter = primes[++iprime];

    //Setup the random size tables
    size_t size_range = max_size - min_size;
    const size_t random_size_count = (sizeof(random_size) / sizeof(random_size[0]));
    for (size_t ir = 0; ir < random_size_count; ++ir)
        random_size[ir] = min_size + (size_t)round((double)size_range * ((double)random_size[ir] / 100000.0));

    if (!size_range)
        ++size_range;
    for (size_t ir = 0; ir < random_size_count; ++ir) {
        double w0 = 1.0 - (double)(random_size[ir] - min_size) / (double)size_range;
        double w1 = 1.0 - (double)(random_size[(ir + 1) % random_size_count] - min_size) / (double)size_range;
        double even = (double)(random_size[(ir + 2) % random_size_count] - min_size);
        random_size_lin[ir] = min_size + (size_t)(even * fabs((w0 + w1) - 1.0));
        random_size_exp[ir] = min_size + (size_t)(even * (w0 * w1));
    }

    //Setup operation count
    size_t alloc_op_count = (sizeof(num_alloc_ops) / sizeof(num_alloc_ops[0]));
    for (size_t ic = 0; ic < alloc_op_count; ++ic)
        num_alloc_ops[ic] = (size_t)((double)op_count * ((double)num_alloc_ops[ic] / 100.0));
    size_t free_op_count = (sizeof(num_free_ops) / sizeof(num_free_ops[0]));
    for (size_t ic = 0; ic < free_op_count; ++ic)
        num_free_ops[ic] = (size_t)(0.8 * (double)op_count * ((double)num_free_ops[ic] / 100.0));

    if (size_mode == SIZE_MODE_EVEN)
        random_size_arr = random_size;
    if (size_mode == SIZE_MODE_LINEAR)
        random_size_arr = random_size_lin;
    if (size_mode == SIZE_MODE_EXP)
        random_size_arr = random_size_exp;

    benchmark_arg* arg;
    uintptr_t* thread_handle;

    arg = static_cast<benchmark_arg *>(benchmark_malloc(0, sizeof(benchmark_arg) * thread_count));
    thread_handle = static_cast<uintptr_t *>(benchmark_malloc(0, sizeof(thread_handle) *
                                                                 thread_count));

    benchmark_start = 0;

    if (mode == MODE_RANDOM)
        printf("%-12s %u threads random %s size [%u,%u] %u loops %u allocs %u ops: ",
               benchmark_name(),
               (unsigned int)thread_count,
               (size_mode == SIZE_MODE_EVEN) ? "even" : ((size_mode == SIZE_MODE_LINEAR) ? "linear" : "exp"),
               (unsigned int)min_size, (unsigned int)max_size,
               (unsigned int)loop_count, (unsigned int)alloc_count, (unsigned int)op_count);
    else
        printf("%-12s %u threads fixed size [%u] %u loops %u allocs %u ops: ",
               benchmark_name(),
               (unsigned int)thread_count,
               (unsigned int)min_size,
               (unsigned int)loop_count, (unsigned int)alloc_count, (unsigned int)op_count);
    fflush(stdout);
    LOGD(TAG, "flushed");


    size_t memory_usage = 0;
    size_t cur_memory_usage = 0;
    size_t sample_allocated = 0;
    size_t cur_allocated = 0;
    uint64_t mops = 0;
    uint64_t ticks = 0;

    for (size_t iter = 0; iter < 2; ++iter) {
        benchmark_start = 0;
        atomic_store32(&benchmark_threads_sync, 0);
        thread_fence();

        for (size_t ithread = 0; ithread < thread_count; ++ithread) {
            arg[ithread].numthreads = thread_count;
            arg[ithread].index = ithread;
            arg[ithread].mode = mode;
            arg[ithread].size_mode = size_mode;
            arg[ithread].cross_rate = cross_rate;
            arg[ithread].loop_count = loop_count;
            arg[ithread].alloc_count = alloc_count;
            arg[ithread].min_size = min_size;
            arg[ithread].max_size = max_size;
            arg[ithread].thread_arg.fn = benchmark_worker;
            arg[ithread].thread_arg.arg = &arg[ithread];
            atomic_store_ptr(&arg[ithread].foreign, 0);
            atomic_store32(&arg[ithread].allocated, 0);
            arg[ithread].peak_allocated = 0;
            arg[ithread].args = arg;
            thread_fence();
            thread_handle[ithread] = thread_run(&arg[ithread].thread_arg);
        }

        thread_sleep(1000);

        benchmark_start = 1;
        thread_fence();

        while (atomic_load32(&benchmark_threads_sync) < (int32_t)thread_count) {
            thread_sleep(1000);
            thread_fence();
        }
        thread_sleep(1000);
        thread_fence();

        cur_allocated = 0;
        for (size_t ithread = 0; ithread < thread_count; ++ithread) {
            size_t thread_allocated = (size_t)atomic_load32(&arg[ithread].allocated);
            cur_allocated += thread_allocated;
        }
        cur_memory_usage = get_process_memory_usage();
        if ((cur_allocated > sample_allocated) || (cur_memory_usage > memory_usage)) {
            sample_allocated = cur_allocated;
            memory_usage = cur_memory_usage;
        }

        atomic_store32(&benchmark_threads_sync, 0);
        thread_fence();

        thread_sleep(1000);
        while (atomic_load32(&benchmark_threads_sync) < (int32_t)thread_count) {
            thread_sleep(1000);
            thread_fence();
        }
        thread_sleep(1000);
        thread_fence();

        cur_allocated = 0;
        for (size_t ithread = 0; ithread < thread_count; ++ithread) {
            size_t thread_allocated = (size_t)atomic_load32(&arg[ithread].allocated);
            cur_allocated += thread_allocated;
        }
        cur_memory_usage = get_process_memory_usage();
        if ((cur_allocated > sample_allocated) || (cur_memory_usage > memory_usage)) {
            sample_allocated = cur_allocated;
            memory_usage = cur_memory_usage;
        }

        atomic_store32(&benchmark_threads_sync, 0);
        thread_fence();

        thread_sleep(1000);
        while (atomic_load32(&benchmark_threads_sync) < (int32_t)thread_count) {
            thread_sleep(1000);
            thread_fence();
        }
        thread_sleep(1000);
        thread_fence();

        atomic_store32(&benchmark_threads_sync, 0);
        thread_fence();

        for (size_t ithread = 0; ithread < thread_count; ++ithread) {
            thread_join(thread_handle[ithread]);
            ticks += arg[ithread].ticks;
            mops += arg[ithread].mops;
            if (!arg[ithread].accumulator)
                exit(-1);
        }
    }

    if (!ticks)
        ticks = 1;

    benchmark_free(thread_handle);
    benchmark_free(arg);

    sleep(2);

    FILE* fd;
    char filebuf[64];
    if (mode == 0)
        sprintf(filebuf, "/sdcard/benchmark-random-%u-%u-%u-%s.txt",
                (unsigned int)thread_count, (unsigned int)min_size,
                (unsigned int)max_size, benchmark_name());
    else
        sprintf(filebuf, "/sdcard/benchmark-fixed-%u-%u-%s.txt",
                (unsigned int)thread_count, (unsigned int)min_size,
                benchmark_name());
    fd = fopen(filebuf, "w+b");

    size_t peak_allocated = get_process_peak_memory_usage();
    double time_elapsed = timer_ticks_to_seconds(ticks);
    double average_mops = (double)mops / time_elapsed;
    char linebuf[128];
    int len = snprintf(linebuf, sizeof(linebuf), "%u,%" PRIsize ",%" PRIsize ",%" PRIsize "\n",
                       (unsigned int)average_mops,
                       peak_allocated,
                       sample_allocated,
                       memory_usage);
    if (fd) {
        fwrite(linebuf, (len > 0) ? (size_t)len : 0, 1, fd);
        fflush(fd);
    }

    printf("%u memory ops/CPU second (%uMiB peak, %uMiB -> %uMiB bytes sample, %.0f%% overhead)\n",
           (unsigned int)average_mops, (unsigned int)(peak_allocated / (1024 * 1024)),
           (unsigned int)(sample_allocated / (1024 * 1024)), (unsigned int)(memory_usage / (1024 * 1024)),
           100.0 * ((double)memory_usage - (double)sample_allocated) / (double)sample_allocated);
    fflush(stdout);

    if (fd)
        fclose(fd);

    if (benchmark_finalize() < 0)
        return -4;

    return 0;
}

#if ( defined( __APPLE__ ) && __APPLE__ )
#  include <TargetConditionals.h>
#  if defined( __IPHONE__ ) || ( defined( TARGET_OS_IPHONE ) && TARGET_OS_IPHONE ) || ( defined( TARGET_IPHONE_SIMULATOR ) && TARGET_IPHONE_SIMULATOR )
#    define NO_MAIN 1
#  endif
#endif

#if !defined(NO_MAIN)

int
benchmark_entry(int argc, char** argv) {
    return benchmark_run(argc, argv);
}

#ifdef __cplusplus
}
#endif

#endif