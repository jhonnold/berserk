#pragma once
/* print.h standalone print macros
https://github.com/FrozenVoid/print.h
print(args) print all arguments(space separated)
hexprint(args) prints all arguments in hex(except strings)
dbgprint(args) prints all arguments to stderr
dbghexprint(args) prints all arguments to stderr in hex(if possible)

dprint(delim,args) prints all arguments separated by delim string
hexdprint(delim,args) hex prints all arguments separated by delim string

fprint(FILE*,args) write all arguments(space separated) to file
hexfprint(FILE*,args) hex-write all arguments(space separated) to file

fdprint(delim,file,args)  writes all arguments with custom delim
hexfdprint(delim,file,args) writes all arguments in hex(except strings) with custom delim */
#include <stdint.h>
#include <inttypes.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

//argcount
#define num1_1023  1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555,556,557,558,559,560,561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633,634,635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672,673,674,675,676,677,678,679,680,681,682,683,684,685,686,687,688,689,690,691,692,693,694,695,696,697,698,699,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,739,740,741,742,743,744,745,746,747,748,749,750,751,752,753,754,755,756,757,758,759,760,761,762,763,764,765,766,767,768,769,770,771,772,773,774,775,776,777,778,779,780,781,782,783,784,785,786,787,788,789,790,791,792,793,794,795,796,797,798,799,800,801,802,803,804,805,806,807,808,809,810,811,812,813,814,815,816,817,818,819,820,821,822,823,824,825,826,827,828,829,830,831,832,833,834,835,836,837,838,839,840,841,842,843,844,845,846,847,848,849,850,851,852,853,854,855,856,857,858,859,860,861,862,863,864,865,866,867,868,869,870,871,872,873,874,875,876,877,878,879,880,881,882,883,884,885,886,887,888,889,890,891,892,893,894,895,896,897,898,899,900,901,902,903,904,905,906,907,908,909,910,911,912,913,914,915,916,917,918,919,920,921,922,923,924,925,926,927,928,929,930,931,932,933,934,935,936,937,938,939,940,941,942,943,944,945,946,947,948,949,950,951,952,953,954,955,956,957,958,959,960,961,962,963,964,965,966,967,968,969,970,971,972,973,974,975,976,977,978,979,980,981,982,983,984,985,986,987,988,989,990,991,992,993,994,995,996,997,998,999,1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,1016,1017,1018,1019,1020,1021,1022,1023
#define num0_1023 0,num1_1023
#define argcount_count1(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,\
_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,\
_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,\
_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,\
_66,_67,_68,_69,_70,_71,_72,_73,_74,_75,_76,_77,_78,_79,_80,_81,_82,\
_83,_84,_85,_86,_87,_88,_89,_90,_91,_92,_93,_94,_95,_96,_97,_98,_99,\
_100,_101,_102,_103,_104,_105,_106,_107,_108,_109,_110,_111,_112,_113,\
_114,_115,_116,_117,_118,_119,_120,_121,_122,_123,_124,_125,_126,_127,\
_128,_129,_130,_131,_132,_133,_134,_135,_136,_137,_138,_139,_140,_141,\
_142,_143,_144,_145,_146,_147,_148,_149,_150,_151,_152,_153,_154,_155,\
_156,_157,_158,_159,_160,_161,_162,_163,_164,_165,_166,_167,_168,_169,\
_170,_171,_172,_173,_174,_175,_176,_177,_178,_179,_180,_181,_182,_183,\
_184,_185,_186,_187,_188,_189,_190,_191,_192,_193,_194,_195,_196,_197,\
_198,_199,_200,_201,_202,_203,_204,_205,_206,_207,_208,_209,_210,_211,\
_212,_213,_214,_215,_216,_217,_218,_219,_220,_221,_222,_223,_224,_225,\
_226,_227,_228,_229,_230,_231,_232,_233,_234,_235,_236,_237,_238,_239,\
_240,_241,_242,_243,_244,_245,_246,_247,_248,_249,_250,_251,_252,_253,\
_254,_255,_256,_257,_258,_259,_260,_261,_262,_263,_264,_265,_266,_267,\
_268,_269,_270,_271,_272,_273,_274,_275,_276,_277,_278,_279,_280,_281,\
_282,_283,_284,_285,_286,_287,_288,_289,_290,_291,_292,_293,_294,_295,\
_296,_297,_298,_299,_300,_301,_302,_303,_304,_305,_306,_307,_308,_309,\
_310,_311,_312,_313,_314,_315,_316,_317,_318,_319,_320,_321,_322,_323,\
_324,_325,_326,_327,_328,_329,_330,_331,_332,_333,_334,_335,_336,_337,\
_338,_339,_340,_341,_342,_343,_344,_345,_346,_347,_348,_349,_350,_351,\
_352,_353,_354,_355,_356,_357,_358,_359,_360,_361,_362,_363,_364,_365,\
_366,_367,_368,_369,_370,_371,_372,_373,_374,_375,_376,_377,_378,_379,\
_380,_381,_382,_383,_384,_385,_386,_387,_388,_389,_390,_391,_392,_393,\
_394,_395,_396,_397,_398,_399,_400,_401,_402,_403,_404,_405,_406,_407,\
_408,_409,_410,_411,_412,_413,_414,_415,_416,_417,_418,_419,_420,_421,\
_422,_423,_424,_425,_426,_427,_428,_429,_430,_431,_432,_433,_434,_435,\
_436,_437,_438,_439,_440,_441,_442,_443,_444,_445,_446,_447,_448,_449,\
_450,_451,_452,_453,_454,_455,_456,_457,_458,_459,_460,_461,_462,_463,\
_464,_465,_466,_467,_468,_469,_470,_471,_472,_473,_474,_475,_476,_477,\
_478,_479,_480,_481,_482,_483,_484,_485,_486,_487,_488,_489,_490,_491,\
_492,_493,_494,_495,_496,_497,_498,_499,_500,_501,_502,_503,_504,_505,\
_506,_507,_508,_509,_510,_511,_512,_513,_514,_515,_516,_517,_518,_519,\
_520,_521,_522,_523,_524,_525,_526,_527,_528,_529,_530,_531,_532,_533,\
_534,_535,_536,_537,_538,_539,_540,_541,_542,_543,_544,_545,_546,_547,\
_548,_549,_550,_551,_552,_553,_554,_555,_556,_557,_558,_559,_560,_561,\
_562,_563,_564,_565,_566,_567,_568,_569,_570,_571,_572,_573,_574,_575,\
_576,_577,_578,_579,_580,_581,_582,_583,_584,_585,_586,_587,_588,_589,\
_590,_591,_592,_593,_594,_595,_596,_597,_598,_599,_600,_601,_602,_603,\
_604,_605,_606,_607,_608,_609,_610,_611,_612,_613,_614,_615,_616,_617,\
_618,_619,_620,_621,_622,_623,_624,_625,_626,_627,_628,_629,_630,_631,\
_632,_633,_634,_635,_636,_637,_638,_639,_640,_641,_642,_643,_644,_645,\
_646,_647,_648,_649,_650,_651,_652,_653,_654,_655,_656,_657,_658,_659,\
_660,_661,_662,_663,_664,_665,_666,_667,_668,_669,_670,_671,_672,_673,\
_674,_675,_676,_677,_678,_679,_680,_681,_682,_683,_684,_685,_686,_687,\
_688,_689,_690,_691,_692,_693,_694,_695,_696,_697,_698,_699,_700,_701,\
_702,_703,_704,_705,_706,_707,_708,_709,_710,_711,_712,_713,_714,_715,\
_716,_717,_718,_719,_720,_721,_722,_723,_724,_725,_726,_727,_728,_729,\
_730,_731,_732,_733,_734,_735,_736,_737,_738,_739,_740,_741,_742,_743,\
_744,_745,_746,_747,_748,_749,_750,_751,_752,_753,_754,_755,_756,_757,\
_758,_759,_760,_761,_762,_763,_764,_765,_766,_767,_768,_769,_770,_771,\
_772,_773,_774,_775,_776,_777,_778,_779,_780,_781,_782,_783,_784,_785,\
_786,_787,_788,_789,_790,_791,_792,_793,_794,_795,_796,_797,_798,_799,\
_800,_801,_802,_803,_804,_805,_806,_807,_808,_809,_810,_811,_812,_813,\
_814,_815,_816,_817,_818,_819,_820,_821,_822,_823,_824,_825,_826,_827,\
_828,_829,_830,_831,_832,_833,_834,_835,_836,_837,_838,_839,_840,_841,\
_842,_843,_844,_845,_846,_847,_848,_849,_850,_851,_852,_853,_854,_855,\
_856,_857,_858,_859,_860,_861,_862,_863,_864,_865,_866,_867,_868,_869,\
_870,_871,_872,_873,_874,_875,_876,_877,_878,_879,_880,_881,_882,_883,\
_884,_885,_886,_887,_888,_889,_890,_891,_892,_893,_894,_895,_896,_897,\
_898,_899,_900,_901,_902,_903,_904,_905,_906,_907,_908,_909,_910,_911,\
_912,_913,_914,_915,_916,_917,_918,_919,_920,_921,_922,_923,_924,_925,\
_926,_927,_928,_929,_930,_931,_932,_933,_934,_935,_936,_937,_938,_939,\
_940,_941,_942,_943,_944,_945,_946,_947,_948,_949,_950,_951,_952,_953,\
_954,_955,_956,_957,_958,_959,_960,_961,_962,_963,_964,_965,_966,_967,\
_968,_969,_970,_971,_972,_973,_974,_975,_976,_977,_978,_979,_980,_981,\
_982,_983,_984,_985,_986,_987,_988,_989,_990,_991,_992,_993,_994,_995,\
_996,_997,_998,_999,_1000,_1001,_1002,_1003,_1004,_1005,_1006,_1007,\
_1008,_1009,_1010,_1011,_1012,_1013,_1014,_1015,_1016,_1017,_1018,\
_1019,_1020,_1021,_1022,_1023,_1024,_1025,N,...) N
    #define argcount_count(args...) argcount_count1(args)
    //  args push>>> n. 3,2,1 :: :args:<numbers>
    #define argcount(...) argcount_count(0,##__VA_ARGS__,1025,\
1024,1023,1022,1021,1020,1019,1018,1017,1016,1015,1014,1013,1012,\
1011,1010,1009,1008,1007,1006,1005,1004,1003,1002,1001,1000,999,998,\
997,996,995,994,993,992,991,990,989,988,987,986,985,984,983,982,981,\
980,979,978,977,976,975,974,973,972,971,970,969,968,967,966,965,964,\
963,962,961,960,959,958,957,956,955,954,953,952,951,950,949,948,947,\
946,945,944,943,942,941,940,939,938,937,936,935,934,933,932,931,930,\
929,928,927,926,925,924,923,922,921,920,919,918,917,916,915,914,913,\
912,911,910,909,908,907,906,905,904,903,902,901,900,899,898,897,896,\
895,894,893,892,891,890,889,888,887,886,885,884,883,882,881,880,879,\
878,877,876,875,874,873,872,871,870,869,868,867,866,865,864,863,862,\
861,860,859,858,857,856,855,854,853,852,851,850,849,848,847,846,845,\
844,843,842,841,840,839,838,837,836,835,834,833,832,831,830,829,828,\
827,826,825,824,823,822,821,820,819,818,817,816,815,814,813,812,811,\
810,809,808,807,806,805,804,803,802,801,800,799,798,797,796,795,794,\
793,792,791,790,789,788,787,786,785,784,783,782,781,780,779,778,777,\
776,775,774,773,772,771,770,769,768,767,766,765,764,763,762,761,760,\
759,758,757,756,755,754,753,752,751,750,749,748,747,746,745,744,743,\
742,741,740,739,738,737,736,735,734,733,732,731,730,729,728,727,726,\
725,724,723,722,721,720,719,718,717,716,715,714,713,712,711,710,709,\
708,707,706,705,704,703,702,701,700,699,698,697,696,695,694,693,692,\
691,690,689,688,687,686,685,684,683,682,681,680,679,678,677,676,675,\
674,673,672,671,670,669,668,667,666,665,664,663,662,661,660,659,658,\
657,656,655,654,653,652,651,650,649,648,647,646,645,644,643,642,641,\
640,639,638,637,636,635,634,633,632,631,630,629,628,627,626,625,624,\
623,622,621,620,619,618,617,616,615,614,613,612,611,610,609,608,607,\
606,605,604,603,602,601,600,599,598,597,596,595,594,593,592,591,590,\
589,588,587,586,585,584,583,582,581,580,579,578,577,576,575,574,573,\
572,571,570,569,568,567,566,565,564,563,562,561,560,559,558,557,556,\
555,554,553,552,551,550,549,548,547,546,545,544,543,542,541,540,539,\
538,537,536,535,534,533,532,531,530,529,528,527,526,525,524,523,522,\
521,520,519,518,517,516,515,514,513,512,511,510,509,508,507,506,505,\
504,503,502,501,500,499,498,497,496,495,494,493,492,491,490,489,488,\
487,486,485,484,483,482,481,480,479,478,477,476,475,474,473,472,471,\
470,469,468,467,466,465,464,463,462,461,460,459,458,457,456,455,454,\
453,452,451,450,449,448,447,446,445,444,443,442,441,440,439,438,437,\
436,435,434,433,432,431,430,429,428,427,426,425,424,423,422,421,420,\
419,418,417,416,415,414,413,412,411,410,409,408,407,406,405,404,403,\
402,401,400,399,398,397,396,395,394,393,392,391,390,389,388,387,386,\
385,384,383,382,381,380,379,378,377,376,375,374,373,372,371,370,369,\
368,367,366,365,364,363,362,361,360,359,358,357,356,355,354,353,352,\
351,350,349,348,347,346,345,344,343,342,341,340,339,338,337,336,335,\
334,333,332,331,330,329,328,327,326,325,324,323,322,321,320,319,318,\
317,316,315,314,313,312,311,310,309,308,307,306,305,304,303,302,301,\
300,299,298,297,296,295,294,293,292,291,290,289,288,287,286,285,284,\
283,282,281,280,279,278,277,276,275,274,273,272,271,270,269,268,267,\
266,265,264,263,262,261,260,259,258,257,256,255,254,253,252,251,250,\
249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,\
232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,\
215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,\
198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,\
181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,\
164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,\
147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,\
130,129,128,127,126,125,124,123,122,121,120,119,118,117,116,115,114,\
113,112,111,110,109,108,107,106,105,104,103,102,101,100,99,98,97,96,\
95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,\
72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,\
49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,\
26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define merge1(a,b) a##b
#define merge(a,b) merge1(a,b)
#define id(args...) args
#define detuple(arg) id arg
#define stringify1(args...) #args
#define stringify(args...) stringify1(args)
#define mergetuples(a,b) (detuple(a),detuple(b))
#define chainapply(func,arg...) merge(chainapply,argcount(arg))(func,arg)
#define chainapply0(...)
#define chainapply1(func,a,args...) func(a)
#define chainapply2(func,a,args...) func(a),chainapply1(func,args)
#define chainapply3(func,a,args...) func(a),chainapply2(func,args)
#define chainapply4(func,a,args...) func(a),chainapply3(func,args)
#define chainapply5(func,a,args...) func(a),chainapply4(func,args)
#define chainapply6(func,a,args...) func(a),chainapply5(func,args)
#define chainapply7(func,a,args...) func(a),chainapply6(func,args)
#define chainapply8(func,a,args...) func(a),chainapply7(func,args)
#define chainapply9(func,a,args...) func(a),chainapply8(func,args)
#define chainapply10(func,a,args...) func(a),chainapply9(func,args)
#define chainapply11(func,a,args...) func(a),chainapply10(func,args)
#define chainapply12(func,a,args...) func(a),chainapply11(func,args)
#define chainapply13(func,a,args...) func(a),chainapply12(func,args)
#define chainapply14(func,a,args...) func(a),chainapply13(func,args)
#define chainapply15(func,a,args...) func(a),chainapply14(func,args)
#define chainapply16(func,a,args...) func(a),chainapply15(func,args)
#define chainapply17(func,a,args...) func(a),chainapply16(func,args)
#define chainapply18(func,a,args...) func(a),chainapply17(func,args)
#define chainapply19(func,a,args...) func(a),chainapply18(func,args)
#define chainapply20(func,a,args...) func(a),chainapply19(func,args)
#define chainapply21(func,a,args...) func(a),chainapply20(func,args)
#define chainapply22(func,a,args...) func(a),chainapply21(func,args)
#define chainapply23(func,a,args...) func(a),chainapply22(func,args)
#define chainapply24(func,a,args...) func(a),chainapply23(func,args)
#define chainapply25(func,a,args...) func(a),chainapply24(func,args)
#define chainapply26(func,a,args...) func(a),chainapply25(func,args)
#define chainapply27(func,a,args...) func(a),chainapply26(func,args)
#define chainapply28(func,a,args...) func(a),chainapply27(func,args)
#define chainapply29(func,a,args...) func(a),chainapply28(func,args)
#define chainapply30(func,a,args...) func(a),chainapply29(func,args)
#define chainapply31(func,a,args...) func(a),chainapply30(func,args)
#define chainapply32(func,a,args...) func(a),chainapply31(func,args)
#define chainapply33(func,a,args...) func(a),chainapply32(func,args)
#define chainapply34(func,a,args...) func(a),chainapply33(func,args)
#define chainapply35(func,a,args...) func(a),chainapply34(func,args)
#define chainapply36(func,a,args...) func(a),chainapply35(func,args)
#define chainapply37(func,a,args...) func(a),chainapply36(func,args)
#define chainapply38(func,a,args...) func(a),chainapply37(func,args)
#define chainapply39(func,a,args...) func(a),chainapply38(func,args)
#define chainapply40(func,a,args...) func(a),chainapply39(func,args)
#define chainapply41(func,a,args...) func(a),chainapply40(func,args)
#define chainapply42(func,a,args...) func(a),chainapply41(func,args)
#define chainapply43(func,a,args...) func(a),chainapply42(func,args)
#define chainapply44(func,a,args...) func(a),chainapply43(func,args)
#define chainapply45(func,a,args...) func(a),chainapply44(func,args)
#define chainapply46(func,a,args...) func(a),chainapply45(func,args)
#define chainapply47(func,a,args...) func(a),chainapply46(func,args)
#define chainapply48(func,a,args...) func(a),chainapply47(func,args)
#define chainapply49(func,a,args...) func(a),chainapply48(func,args)
#define chainapply50(func,a,args...) func(a),chainapply49(func,args)
#define chainapply51(func,a,args...) func(a),chainapply50(func,args)
#define chainapply52(func,a,args...) func(a),chainapply51(func,args)
#define chainapply53(func,a,args...) func(a),chainapply52(func,args)
#define chainapply54(func,a,args...) func(a),chainapply53(func,args)
#define chainapply55(func,a,args...) func(a),chainapply54(func,args)
#define chainapply56(func,a,args...) func(a),chainapply55(func,args)
#define chainapply57(func,a,args...) func(a),chainapply56(func,args)
#define chainapply58(func,a,args...) func(a),chainapply57(func,args)
#define chainapply59(func,a,args...) func(a),chainapply58(func,args)
#define chainapply60(func,a,args...) func(a),chainapply59(func,args)
#define chainapply61(func,a,args...) func(a),chainapply60(func,args)
#define chainapply62(func,a,args...) func(a),chainapply61(func,args)
#define chainapply63(func,a,args...) func(a),chainapply62(func,args)
#define chainapply64(func,a,args...) func(a),chainapply63(func,args)
#define tapply(tup,func,args...) merge(tapply,argcount(args))(tup,func,args)
#define tapply0(...)
#define tapply1(tup,func,x,...) func(tup,x)
#define tapply2(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply3(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply4(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply5(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply6(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply7(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply8(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply9(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply10(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply11(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply12(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply13(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply14(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply15(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply16(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply17(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply18(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply19(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply20(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply21(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply22(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply23(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply24(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply25(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply26(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply27(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply28(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply29(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply30(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply31(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply32(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply33(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply34(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply35(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply36(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply37(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply38(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply39(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply40(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply41(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply42(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply43(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply44(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply45(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply46(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply47(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply48(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply49(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply50(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply51(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply52(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply53(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply54(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply55(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply56(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply57(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply58(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply59(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply60(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply61(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply62(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply63(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define tapply64(tup,func,x,args...) func(tup,x),merge(tapply,argcount(args))(tup,func,args)
#define appendall1(tup,arg) mergetuples(tup,(arg))
#define appendall(tup,args...) tapply(tup,appendall1,args)
#define SPACE_DELIM " "
#define pformat(x) _Generic((x),\
 char:   "%c",\
 char*:  "%s",\
long long unsigned int: "%llu" ,\
long long int: "%lli" ,\
uint64_t: "%" PRIu64,\
 int64_t: "%" PRIi64,\
uint32_t: "%" PRIu32,\
 int32_t: "%" PRIi32,\
uint16_t: "%" PRIu16,\
 int16_t: "%" PRIi16,\
uint8_t:  "%" PRIu8,\
 int8_t:  "%" PRIi8,\
 float:  "%." stringify(FLT_DIG) "G",\
double:  "%." stringify(DBL_DIG) "G",\
 long double: "%." stringify(LDBL_DIG) "LG",\
default:"%p" )

#define hexformat(x) _Generic((x),\
 char:   "%c",\
 char*:  "%s",\
long long unsigned int: "%llx" ,\
long long int: "%llx" ,\
uint64_t: "%" PRIx64,\
 int64_t: "%" PRIx64,\
uint32_t: "%" PRIx32,\
 int32_t: "%" PRIx32,\
uint16_t: "%" PRIx16,\
 int16_t: "%" PRIx16,\
uint8_t:  "%" PRIx8,\
 int8_t:  "%" PRIx8,\
  float:  "%." stringify(FLT_DIG)"A",\
double:  "%." stringify(DBL_DIG) "A",\
 long double: "%." stringify(LDBL_DIG)"LA",\
default:"%p" )
#define core_print1(delim,file,format_type,arg) \
fprintf(file,delim),fprintf(file,format_type(arg),arg)
#define print1(arg) printf(SPACE_DELIM),printf(pformat(arg),arg)
#define hexprint1(arg) printf(SPACE_DELIM),printf(hexformat(arg),arg)
#define fprint2(file,arg) core_print1(SPACE_DELIM,file,pformat,arg)
#define fprint1(tup) fprint2 tup
#define hexfprint2(file,arg) core_print1(SPACE_DELIM,file,hexformat,arg)
#define hexfprint1(tup) hexfprint2 tup
#define dprint2(delim,arg) printf(delim),printf(pformat(arg),arg)
#define dprint1(tup) dprint2 tup
#define hexdprint2(delim,arg) core_print1(delim,stdout,hexformat,arg)
#define hexdprint1(tup) hexdprint2 tup
#define dfprint2(delim,file,arg) core_print1(delim,file,pformat,arg)
#define dfprint1(tup) dfprint2 tup
#define hexfdprint2(delim,file,arg) core_print1(delim,file,hexformat,arg)
#define hexfdprint1(tup) hexfdprint2 tup
//---- applicative
#define print(args...) chainapply(print1,args)
#define hexprint(args...) chainapply(hexprint1,args)
#define fprint(file,args...) chainapply(fprint1,appendall((file),args))
#define hexfprint(file,args...) chainapply(hexfprint1,appendall((file),args))
#define dprint(delim,args...) chainapply(dprint1,appendall((delim),args))
#define hexdprint(delim,args...) chainapply(hexdprint1,appendall((delim),args))
#define dfprint(delim,file,args...) chainapply(dfprint1,appendall((delim,file),args))
#define hexfdprint(delim,file,args...) chainapply(hexfdprint1,appendall((delim,file),args))
#define dbgprint(args...) fprint(stderr,args)
#define dbghexprint(args...) hexfprint(stderr,args)


