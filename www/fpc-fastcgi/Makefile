# Created by: Christopher Key <cjk32@cam.ac.uk>
# $FreeBSD$

CATEGORIES=	www lang
PKGNAMESUFFIX=	-fastcgi

COMMENT=	Free Pascal fastcgi unit

USE_FPC=	yes

MASTERDIR=	${.CURDIR}/../../lang/fpc
WRKUNITDIR=	${FPCSRCDIR}/packages/${PKGNAMESUFFIX:S/-//}
EXTRACTUNITDIR=	${WRKUNITDIR}

.include "${MASTERDIR}/Makefile"
