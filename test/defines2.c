#define FRED 1

#if defined ( FRED )
FRED
"FRED"
#endif

#define BILL(a,b) a+b

BILL ( A , B )

BILL ( A ", " , B )
