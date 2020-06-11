[1mdiff --git a/lib/core.c b/lib/core.c[m
[1mindex 49f415f..574926e 100644[m
[1m--- a/lib/core.c[m
[1m+++ b/lib/core.c[m
[36m@@ -527,7 +527,7 @@[m [mvoid _printi64(i64 i) {[m
     while (m / 10) m /= 10, p *= 10;[m
     while (p) *writ++ = '0' + (i / p % 10), p /= 10;[m
 [m
[31m-    write(0, buf, writ - buf);[m
[32m+[m[32m    write(1, buf, writ - buf);[m
 }[m
 [m
 void _printu64(u64 u) {[m
[36m@@ -539,7 +539,7 @@[m [mvoid _printu64(u64 u) {[m
     while (m / 10) m /= 10, p *= 10;[m
     while (p) *writ++ = '0' + (u / p % 10), p /= 10;[m
 [m
[31m-    write(0, buf, writ - buf);[m
[32m+[m[32m    write(1, buf, writ - buf);[m
 }[m
 [m
 void _printf64(double d) {[m
[36m@@ -571,7 +571,7 @@[m [mvoid _printf64(double d) {[m
     }[m
     if (isZero) *writ++ = '0';[m
 [m
[31m-    write(0, buf, writ - buf);[m
[32m+[m[32m    write(1, buf, writ - buf);[m
 }[m
 [m
 i64 prev = 0;[m
[36m@@ -584,12 +584,12 @@[m [mvoid _printstr(void* str) {[m
     //     _printf64((n - prev) / 1000000000.0);[m
     //     prev = n;[m
     // }[m
[31m-    write(0, (u8*)str + 8, _strlen(str));[m
[32m+[m[32m    write(1, (u8*)str + 8, _strlen(str));[m
 }[m
 [m
 void _printbool(i8 b) {[m
[31m-    if (b) write(0, "true", 4);[m
[31m-    else write(0, "false", 5);[m
[32m+[m[32m    if (b) write(1, "true", 4);[m
[32m+[m[32m    else write(1, "false", 5);[m
 }[m
 [m
 /* * * * * * * * * * * * *[m
