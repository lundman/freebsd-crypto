/*	$NetBSD: openfirmio.c,v 1.4 2002/09/06 13:23:19 gehenna Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)openfirm.c	8.1 (Berkeley) 6/11/93
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ofw/openfirmio.h>

static dev_t openfirm_dev;

static d_ioctl_t openfirm_ioctl;

#define	OPENFIRM_MINOR	0

static struct cdevsw openfirm_cdevsw = {
	.d_open =	nullopen,
	.d_close =	nullclose,
	.d_ioctl =	openfirm_ioctl,
	.d_name =	"openfirm",
};

static phandle_t lastnode;	/* speed hack */

static int openfirm_checkid(phandle_t, phandle_t);
static int openfirm_getstr(int, const char *, char **);

/* Maximum accepted name length. */
#define	OFW_NAME_MAX	8191

/*
 * Verify target ID is valid (exists in the OPENPROM tree), as
 * listed from node ID sid forward.
 */
static int
openfirm_checkid(phandle_t sid, phandle_t tid)
{

	for (; sid != 0; sid = OF_peer(sid))
		if (sid == tid || openfirm_checkid(OF_child(sid), tid))
			return (1);

	return (0);
}

static int
openfirm_getstr(int len, const char *user, char **cpp)
{
	int error;
	char *cp;

	/* Reject obvious bogus requests */
	if ((u_int)len > OFW_NAME_MAX)
		return (ENAMETOOLONG);

	*cpp = cp = malloc(len + 1, M_TEMP, M_WAITOK);
	if (cp == NULL)
		return (ENOMEM);
	error = copyin(user, cp, len);
	cp[len] = '\0';
	return (error);
}

int
openfirm_ioctl(dev_t dev, u_long cmd, caddr_t data, int flags,
    struct thread *td)
{
	struct ofiocdesc *of;
	phandle_t node;
	int len, ok, error;
	char *name, *value;
	char newname[32];

	if ((flags & FREAD) == 0)
		return (EBADF);

	of = (struct ofiocdesc *)data;
	switch (cmd) {
	case OFIOCGETOPTNODE:
		*(phandle_t *) data = OF_finddevice("/options");
		return (0);
	case OFIOCGET:
#if 0
	case OFIOCSET:
#endif
	case OFIOCNEXTPROP:
	case OFIOCFINDDEVICE:
	case OFIOCGETPROPLEN:
		node = of->of_nodeid;
		break;
	case OFIOCGETNEXT:
	case OFIOCGETCHILD:
		node = *(phandle_t *)data;
		break;
	default:
		return (ENOIOCTL);
	}

	if (node != 0 && node != lastnode) {
		/* Not an easy one, must search for it */
		ok = openfirm_checkid(OF_peer(0), node);
		if (!ok)
			return (EINVAL);
		lastnode = node;
	}

	name = value = NULL;
	error = 0;
	switch (cmd) {

	case OFIOCGET:
	case OFIOCGETPROPLEN:
		if (node == 0)
			return (EINVAL);
		error = openfirm_getstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		len = OF_getproplen(node, name);
		if (cmd == OFIOCGETPROPLEN) {
			of->of_buflen = len;
			break;
		}
		if (len > of->of_buflen) {
			error = ENOMEM;
			break;
		}
		of->of_buflen = len;
		/* -1 means no entry; 0 means no value */
		if (len <= 0)
			break;
		value = malloc(len, M_TEMP, M_WAITOK);
		if (value == NULL) {
			error = ENOMEM;
			break;
		}
		len = OF_getprop(node, name, (void *)value, len);
		error = copyout(value, of->of_buf, len);
		break;

#if 0
	case OFIOCSET:
		if ((flags & FWRITE) == 0)
			return (EBADF);
		if (node == 0)
			return (EINVAL);
		error = openfirm_getstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		error = openfirm_getstr(of->of_buflen, of->of_buf, &value);
		if (error)
			break;
		len = OF_setprop(node, name, value, of->of_buflen);
		if (len != of->of_buflen)
			error = EINVAL;
		break;
#endif

	case OFIOCNEXTPROP:
		if (node == 0 || of->of_buflen < 0)
			return (EINVAL);
		if (of->of_namelen != 0) {
			error = openfirm_getstr(of->of_namelen, of->of_name,
			    &name);
			if (error)
				break;
		}
		ok = OF_nextprop(node, name, newname);
		if (ok == 0) {
			error = ENOENT;
			break;
		}
		if (ok == -1) {
			error = EINVAL;
			break;
		}
		len = strlen(newname) + 1;
		if (len > of->of_buflen)
			len = of->of_buflen;
		else
			of->of_buflen = len;
		error = copyout(newname, of->of_buf, len);
		break;

	case OFIOCGETNEXT:
		node = OF_peer(node);
		*(phandle_t *)data = lastnode = node;
		break;

	case OFIOCGETCHILD:
		if (node == 0)
			return (EINVAL);
		node = OF_child(node);
		*(phandle_t *)data = lastnode = node;
		break;

	case OFIOCFINDDEVICE:
		error = openfirm_getstr(of->of_namelen, of->of_name, &name);
		if (error)
			break;
		node = OF_finddevice(name);
		if (node == 0 || node == -1) {
			error = ENOENT;
			break;
		}
		of->of_nodeid = lastnode = node;
		break;
	}

	if (name != NULL)
		free(name, M_TEMP);
	if (value != NULL)
		free(value, M_TEMP);

	return (error);
}

static int
openfirm_modevent(module_t mod, int type, void *data)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("openfirm: <OpenFirmware control device>\n");
		/*
		 * Allow only root access by default; this device may allow
		 * users to peek into firmware passwords, and likely to crash
		 * the machine on some boxen due to firmware quirks.
		 */
		openfirm_dev = make_dev(&openfirm_cdevsw, OPENFIRM_MINOR,
		    UID_ROOT, GID_WHEEL, 0600, "openfirm");
		return 0;

	case MOD_UNLOAD:
		destroy_dev(openfirm_dev);
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(openfirm, openfirm_modevent, NULL);
