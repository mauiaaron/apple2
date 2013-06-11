dnl This was cribbed from GNU libc (notice the symbol choices), but highly 
dnl simplified since we can assume linking is working.
dnl A2_ASM_UNDERSCORES
AC_DEFUN(A2_ASM_UNDERSCORES,[
AC_CACHE_CHECK(for _ prefix on C symbol names, a2_cv_asm_underscores,
		   [AC_TRY_LINK([asm ("_glibc_foobar:");], [glibc_foobar ();],
			        a2_cv_asm_underscores=yes,
			        a2_cv_asm_underscores=no)])
if test $a2_cv_asm_underscores = no; then
  AC_DEFINE(NO_UNDERSCORES)
fi
])
