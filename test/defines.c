#define TABSIZE 100
int table[TABSIZE];

 fred #TABSIZE

 fred # TABSIZE

 fred TABSIZE ## TABSIZE

#define ABSDIFF(a,b) ((a)>(b)?(a)-(b):(b)-(a))

ABSDIFF(5,6)

ABSDIFF(ABSDIFF(5,6),7)

#define tempfile(dir) #dir "/%s"

tempfile(/usr/tmp)

#define tempfile1(dir) # dir "/%s"

tempfile1(/usr/tmp)

#define cat(x, y) x ## y

cat(var,123)

cat(cat(1,2),3)

#define cat1(x, y) x fred ## fred y

cat1(var,123)

cat1(cat1(1,2),3)

#define xcat(x,y) cat(x,y)

xcat(xcat(1,2),3)
