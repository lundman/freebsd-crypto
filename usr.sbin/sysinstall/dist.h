/* $FreeBSD$  */

#ifndef _DIST_H_INCLUDE
#define _DIST_H_INCLUDE

/* Bitfields for distributions - hope we never have more than 32! :-) */
#define DIST_BASE		0x00001
#define DIST_GAMES		0x00002
#define DIST_MANPAGES		0x00004
#define DIST_PROFLIBS		0x00008
#define DIST_DICT		0x00010
#define DIST_SRC		0x00020
#define DIST_DOC		0x00040
#define DIST_INFO		0x00080
#ifdef __i386__			/* only applicable on x86 */
#define DIST_COMPAT1X		0x00100
#define DIST_COMPAT20		0x00200
#define DIST_COMPAT21		0x00400
#define DIST_COMPAT22		0x00800
#define DIST_COMPAT3X		0x01000
#define DIST_COMPAT4X		0x02000
#endif
#define DIST_XF86		0x04000
#define DIST_CRYPTO		0x08000
#define DIST_CATPAGES		0x10000
#define DIST_PORTS		0x20000
#define DIST_LOCAL		0x40000
#define DIST_ALL		0x7FFFF

/* Subtypes for CRYPTO distribution */
#define DIST_CRYPTO_CRYPTO	0x0001
#define DIST_CRYPTO_SCRYPTO	0x0002
#define DIST_CRYPTO_SSECURE	0x0004
#if __FreeBSD__ <= 3
#define DIST_CRYPTO_KERBEROS	0x0008
#else
#define DIST_CRYPTO_KERBEROS4	0x0008
#define DIST_CRYPTO_KERBEROS5	0x0010
#define DIST_CRYPTO_SKERBEROS4	0x0020
#define DIST_CRYPTO_SKERBEROS5	0x0040
#endif
#define DIST_CRYPTO_ALL		0x007F

/* Subtypes for SRC distribution */
#define DIST_SRC_BASE		0x00001
#define DIST_SRC_CONTRIB	0x00002
#define DIST_SRC_GNU		0x00004
#define DIST_SRC_ETC		0x00008
#define DIST_SRC_GAMES		0x00010
#define DIST_SRC_INCLUDE	0x00020
#define DIST_SRC_LIB		0x00040
#define DIST_SRC_LIBEXEC	0x00080
#define DIST_SRC_TOOLS		0x00100
#define DIST_SRC_RELEASE	0x00200
#define DIST_SRC_SBIN		0x00400
#define DIST_SRC_SHARE		0x00800
#define DIST_SRC_SYS		0x01000
#define DIST_SRC_UBIN		0x02000
#define DIST_SRC_USBIN		0x04000
#define DIST_SRC_BIN		0x08000
#define DIST_SRC_ALL		0x0FFFF

/* Subtypes for XFree86 distribution */
#define DIST_XF86_BIN		0x0001
#define DIST_XF86_CFG		0x0002
#define DIST_XF86_DOC		0x0004
#define DIST_XF86_HTML		0x0008
#define DIST_XF86_LIB		0x0010
#define DIST_XF86_LKIT98	0x0020
#define DIST_XF86_LKIT		0x0040
#define DIST_XF86_MAN		0x0080
#define DIST_XF86_PROG		0x0100
#define DIST_XF86_PS		0x0200
#define DIST_XF86_SET		0x0400
#define DIST_XF86_9SET		0x0800
#define DIST_XF86_MISC_ALL	0x0FFF
#define DIST_XF86_SERVER	0x8000
#ifdef __i386__
#define DIST_XF86_SERVER_9MGA		0x0000001
#define DIST_XF86_SERVER_9480		0x0000002
#define DIST_XF86_SERVER_9EGC		0x0000004
#define DIST_XF86_SERVER_9GA9		0x0000008
#define DIST_XF86_SERVER_9GAN		0x0000010
#define DIST_XF86_SERVER_9LPW		0x0000020
#define DIST_XF86_SERVER_9NKV		0x0000040
#define DIST_XF86_SERVER_9NS3		0x0000080
#define DIST_XF86_SERVER_9SVG		0x0000100
#define DIST_XF86_SERVER_9SPW		0x0000200
#define DIST_XF86_SERVER_9TGU		0x0000400
#define DIST_XF86_SERVER_9WEP		0x0000800
#define DIST_XF86_SERVER_9WS		0x0001000
#define DIST_XF86_SERVER_9WSN		0x0002000
#elif __alpha__
#define DIST_XF86_SERVER_TGA		0x0000001
#endif
#define DIST_XF86_SERVER_8514		0x0004000
#define DIST_XF86_SERVER_AGX		0x0008000
#define DIST_XF86_SERVER_I128		0x0010000
#define DIST_XF86_SERVER_MACH8		0x0020000
#define DIST_XF86_SERVER_MACH32		0x0040000
#define DIST_XF86_SERVER_MACH64		0x0080000
#define DIST_XF86_SERVER_MONO		0x0100000
#define DIST_XF86_SERVER_P9000		0x0200000
#define DIST_XF86_SERVER_S3		0x0400000
#define DIST_XF86_SERVER_S3V		0x0800000
#define DIST_XF86_SERVER_SVGA		0x1000000
#define DIST_XF86_SERVER_VGA16		0x2000000
#define DIST_XF86_SERVER_W32		0x4000000
#define DIST_XF86_SERVER_3DL		0x8000000
#define DIST_XF86_SERVER_ALL		0xFFFFFFF
#define DIST_XF86_FONTS		0x10000
#define DIST_XF86_FONTS_MISC		0x0001
#define DIST_XF86_FONTS_100		0x0002
#define DIST_XF86_FONTS_CYR		0x0004
#define DIST_XF86_FONTS_SCALE		0x0008
#define DIST_XF86_FONTS_NON		0x0010
#define DIST_XF86_FONTS_SERVER		0x0020
#define DIST_XF86_FONTS_ALL		0x003F
#define DIST_XF86_ALL		0x1FFFF

/* Canned distribution sets */
#define _DIST_USER \
	( DIST_BASE | DIST_DOC | DIST_MANPAGES | DIST_DICT | DIST_CRYPTO )

#define _DIST_DEVELOPER \
	( _DIST_USER | DIST_PROFLIBS | DIST_INFO | DIST_SRC )

#define DIST_CRYPTO_BIN \
	( DIST_CRYPTO_CRYPTO | DIST_CRYPTO_KERBEROS4 | DIST_CRYPTO_KERBEROS5 )

#endif	/* _DIST_H_INCLUDE */





