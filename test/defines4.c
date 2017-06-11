#define cat(x, y) x ## y
#define xcat(x,y) cat(x,y)

xcat(xcat(1,2),3)
