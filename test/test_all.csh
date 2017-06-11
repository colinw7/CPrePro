#!/bin/csh -f

foreach file (*.c)
  echo $file

  gcc -trigraphs -E -P $file -o $file:r.i

  CPrePro $file > $file:r.temp.i

  diff -bw $file:r.i $file:r.temp.i

  rm -f $file:r.i
  rm -f $file:r.temp.i
end

exit 0
