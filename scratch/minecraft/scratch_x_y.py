#
# build a pixel_x_y-array from some strings
#
# the result is printed in x,y each in a row.


f0="                         *          *        "    
f1="                         *          *        "
f2=" ***   ***  * **   ***  ***    ***  * **     "
f3="*     *   * **  *     *  *    *     **  *    "
f4=" ***  *     *      ****  *    *     *   *    "
f5="    * *   * *     *   *  *  * *   * *   *    "
f6="****   ***  *      ****   **   ***  *   *    "

def printPixel(f, level):
    for i in range(len(f)):
        if '*' == f[i]:
            print(i)
            print(level)
            
printPixel(f6, 1)
printPixel(f5, 2)
printPixel(f4, 3)
printPixel(f3, 4)
printPixel(f2, 5)
printPixel(f1, 6)
printPixel(f0, 7)

            