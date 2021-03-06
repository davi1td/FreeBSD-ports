# Created by: Yuri Victorovich <yuri@rawbw.com>
# $FreeBSD$

PORTNAME=	tiny-network-utilities
PORTVERSION=	0.150519
CATEGORIES=	net

MAINTAINER=	yuri@rawbw.com
COMMENT=	Minimalistic DHCP, UDP proxy, etc server implementations

LICENSE=	BSD4CLAUSE

RUN_DEPENDS=	${PYTHON_PKGNAMEPREFIX}netifaces>0:net/py-netifaces \
		${PYTHON_PKGNAMEPREFIX}hexdump>0:textproc/py-hexdump

USE_GITHUB=	yes
GH_ACCOUNT=	yurivict
GH_TAGNAME=	d72f531

USES=		python:3.4+
NO_BUILD=	yes
NO_ARCH=	yes

FILES_EXEC=	tiny-dhcp-server tiny-udp-proxy tiny-udp-anti-nat
FILES_LIB=	net_checksums tiny_utils
PLIST_FILES=	${FILES_EXEC:%=bin/%} ${FILES_LIB:%=${PYTHON_SITELIBDIR}/%.py}

.include <bsd.port.pre.mk>

.if defined(PACKAGE_BUILDING) && !defined(PACKAGE_BUILDING_FLAVORS) && \
    ${PYTHON_VER} != ${PYTHON_DEFAULT}
IGNORE=	you have python ${PYTHON_DEFAULT} set as the default, and this needs ${PYTHON_VER}
.endif

do-install:
.for file_exec in ${FILES_EXEC}
	${INSTALL_SCRIPT} ${WRKSRC}/${file_exec}.py \
		${STAGEDIR}${PREFIX}/bin/${file_exec}
.endfor
	@${MKDIR} ${STAGEDIR}${PYTHON_SITELIBDIR}
.for file_lib in ${FILES_LIB}
	${INSTALL_DATA} ${WRKSRC}/${file_lib}.py \
		${STAGEDIR}${PYTHON_SITELIBDIR}/${file_lib}.py
.endfor

.include <bsd.port.post.mk>
