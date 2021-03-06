/* mboxname.c -- Mailbox list manipulation routines
 * $Id: mboxname.c,v 1.37 2006/11/30 17:11:19 murch Exp $
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "assert.h"
#include "glob.h"
#include "global.h"
#include "mailbox.h"
#include "exitcodes.h"
#include "imap_err.h"
#include "xmalloc.h"

#include "mboxname.h"
#include "mboxlist.h"

/* Mailbox patterns which the design of the server prohibits */
static char *badmboxpatterns[] = {
    "",
    "*\t*",
    "*\n*",
    "*/*",
    ".*",
    "*.",
    "*..*",
    "user",
};
#define NUM_BADMBOXPATTERNS (sizeof(badmboxpatterns)/sizeof(*badmboxpatterns))

#define XX 127
/*
 * Table for decoding modified base64 for IMAP UTF-7 mailbox names
 */
static const char index_mod64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, 63,XX,XX,XX,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};
#define CHARMOD64(c)  (index_mod64[(unsigned char)(c)])


/*
 * Convert the external mailbox 'name' to an internal name.
 * If 'userid' is non-null, it is the name of the current user.
 * On success, results are placed in the buffer pointed to by
 * 'result', the buffer must be of size MAX_MAILBOX_NAME+1.
 */

/* Handle conversion from the standard namespace to the internal namespace */
static int mboxname_tointernal(struct namespace *namespace, const char *name,
			       const char *userid, char *result)
{
    char *cp;
    int userlen, domainlen = 0, namelen;

    /* Blank the result, just in case */
    result[0] = '\0';

    userlen = userid ? strlen(userid) : 0;
    namelen = strlen(name);

    if (config_virtdomains) {
	if (userid && (cp = strrchr(userid, '@'))) {
	    /* user logged in as user@domain */
	    userlen = cp++ - userid;
	    /* don't prepend default domain */
	    if (!(config_defdomain && !strcasecmp(config_defdomain, cp))) {
		domainlen = strlen(cp)+1;
		if (domainlen > MAX_MAILBOX_NAME) 
		    return IMAP_MAILBOX_BADNAME; 
		sprintf(result, "%s!", cp);
	    }
	}
	if ((cp = strrchr(name, '@'))) {
	    /* mailbox specified as mbox@domain */
	    namelen = cp - name;

	    if (config_defdomain && !strcasecmp(config_defdomain, cp+1)) {
		if (domainlen) {
		    /* don't allow cross-domain access */
		    return IMAP_MAILBOX_BADNAME;
		}
		/* don't prepend default domain */
	    }
	    else {
		if ((!domainlen && !namespace->isadmin) ||
		    (domainlen && strcasecmp(userid+userlen, cp))) {
		    /* don't allow cross-domain access
		       (except for global admin) */
		    return IMAP_MAILBOX_BADNAME;
		}
		domainlen = strlen(cp+1)+1;
		if (domainlen > MAX_MAILBOX_NAME) 
		    return IMAP_MAILBOX_BADNAME; 
		sprintf(result, "%s!", cp+1);
	    }
	}

	/* if no domain specified, we're in the default domain */
    }

    result += domainlen;

    /* Personal (INBOX) namespace */
    if ((name[0] == 'i' || name[0] == 'I') &&
	!strncasecmp(name, "inbox", 5) &&
	(namelen == 5 || name[5] == namespace->hier_sep)) {

	if (!userid || ((cp = strchr(userid, namespace->hier_sep)) &&
			(cp - userid < userlen))) {
	    return IMAP_MAILBOX_BADNAME;
	}

	if (domainlen+namelen+userlen > MAX_MAILBOX_NAME) {
	    return IMAP_MAILBOX_BADNAME;
	}

	sprintf(result, "user.%.*s%.*s", userlen, userid, namelen-5, name+5);

	/* Translate any separators in userid+mailbox */
	mboxname_hiersep_tointernal(namespace, result+5+userlen, 0);
	return 0;
    }

    /* Other Users & Shared namespace */
    if (domainlen+namelen > MAX_MAILBOX_NAME) {
	return IMAP_MAILBOX_BADNAME;
    }
    sprintf(result, "%.*s", namelen, name);

    /* Translate any separators in mailboxname */
    mboxname_hiersep_tointernal(namespace, result, 0);
    return 0;
}

/* Handle conversion from the alternate namespace to the internal namespace */
static int mboxname_tointernal_alt(struct namespace *namespace,
				   const char *name,
				   const char *userid, char *result)
{
    char *cp;
    int userlen, domainlen = 0, namelen;
    int prefixlen;
    size_t resultlen;

    /* Blank the result, just in case */
    result[0] = '\0';

    userlen = userid ? strlen(userid) : 0;
    namelen = strlen(name);

    if (config_virtdomains) {
	if (userid && (cp = strchr(userid, '@'))) {
	    /* user logged in as user@domain */
	    userlen = cp++ - userid;
	    if (!(config_defdomain && !strcasecmp(config_defdomain, cp))) {
		/* don't prepend default domain */
		domainlen = strlen(cp)+1;
		if (domainlen > MAX_MAILBOX_NAME) 
		    return IMAP_MAILBOX_BADNAME; 
		sprintf(result, "%s!", cp);
	    }
	}
	if ((cp = strrchr(name, '@'))) {
	    /* mailbox specified as mbox@domain */
	    namelen = cp - name;

	    if (config_defdomain && !strcasecmp(config_defdomain, cp+1)) {
		if (domainlen) {
		    /* don't allow cross-domain access */
		    return IMAP_MAILBOX_BADNAME;
		}
		/* don't prepend default domain */
	    }
	    else {
		if ((!domainlen && !namespace->isadmin) ||
		    (domainlen && strcasecmp(userid+userlen, cp))) {
		    /* don't allow cross-domain access 
		       (except for global admin) */
		    return IMAP_MAILBOX_BADNAME;
		}
		domainlen = strlen(cp+1)+1;
		if (domainlen > MAX_MAILBOX_NAME) 
		    return IMAP_MAILBOX_BADNAME; 
		sprintf(result, "%s!", cp+1);
	    }
	}

	/* if no domain specified, we're in the default domain */
    }

    result += domainlen;

    /* Shared namespace */
    prefixlen = strlen(namespace->prefix[NAMESPACE_SHARED]);
    if(prefixlen == 0) return IMAP_MAILBOX_BADNAME;

    if (!strncmp(name, namespace->prefix[NAMESPACE_SHARED], prefixlen-1) &&
	(namelen == prefixlen-1 || name[prefixlen-1] == namespace->hier_sep)) {

	if (namelen ==  prefixlen-1) {
	    /* can't create folders using undelimited prefix */
	    return IMAP_MAILBOX_BADNAME;
	}

	if (domainlen+namelen-prefixlen > MAX_MAILBOX_NAME) {
	    return IMAP_MAILBOX_BADNAME;
	}

	sprintf(result, "%.*s", namelen-prefixlen, name+prefixlen);

	/* Translate any separators in mailboxname */
	mboxname_hiersep_tointernal(namespace, result, 0);
	return 0;
    }

    /* Other Users namespace */
    prefixlen = strlen(namespace->prefix[NAMESPACE_USER]);
    if(prefixlen == 0) return IMAP_MAILBOX_BADNAME;

    if (!strncmp(name, namespace->prefix[NAMESPACE_USER], prefixlen-1) &&
	(namelen == prefixlen-1 || name[prefixlen-1] == namespace->hier_sep)) {

	if (namelen == prefixlen-1) {
	    /* can't create folders using undelimited prefix */
	    return IMAP_MAILBOX_BADNAME;
	}

	if (domainlen+namelen-prefixlen+5 > MAX_MAILBOX_NAME) {
	    return IMAP_MAILBOX_BADNAME;
	}

	sprintf(result, "user.%.*s", namelen-prefixlen, name+prefixlen);

	/* Translate any separators in userid+mailbox */
	mboxname_hiersep_tointernal(namespace, result+5, 0);
	return 0;
    }

    /* Personal (INBOX) namespace */
    if (!userid || ((cp = strchr(userid, namespace->hier_sep)) &&
		    (cp - userid < userlen))) {
	return IMAP_MAILBOX_BADNAME;
    }

    if (domainlen+userlen+5 > MAX_MAILBOX_NAME) {
	return IMAP_MAILBOX_BADNAME;
    }

    sprintf(result, "user.%.*s", userlen, userid);

    /* INBOX */
    if ((name[0] == 'i' || name[0] == 'I') &&
	!strncasecmp(name, "inbox", 5) && 
	(namelen == 5 || name[5] == namespace->hier_sep)) {

	if (name[5] == namespace->hier_sep) {
	    /* can't create folders under INBOX */
	    return IMAP_MAILBOX_BADNAME;
	}

	return 0;
    }

    resultlen = strlen(result);

    /* other personal folder */
    if (domainlen+resultlen+6+namelen > MAX_MAILBOX_NAME) {
	return IMAP_MAILBOX_BADNAME;
    }
    snprintf(result+resultlen, MAX_MAILBOX_NAME+1-resultlen, ".%.*s", 
	     namelen, name);

    /* Translate any separators in mailboxname */
    mboxname_hiersep_tointernal(namespace, result+6+userlen, 0);
    return 0;
}

/*
 * Convert the internal mailbox 'name' to an external name.
 * If 'userid' is non-null, it is the name of the current user.
 * On success, results are placed in the buffer pointed to by
 * 'result', the buffer must be of size MAX_MAILBOX_NAME+1.
 */

/* Handle conversion from the internal namespace to the standard namespace */
static int mboxname_toexternal(struct namespace *namespace, const char *name,
			       const char *userid, char *result)
{
    char *domain = NULL, *cp;
    size_t domainlen = 0, resultlen;

    /* Blank the result, just in case */
    result[0] = '\0';

    if(strlen(name) > MAX_MAILBOX_NAME) return IMAP_MAILBOX_BADNAME;
    
    if (config_virtdomains && (cp = strchr(name, '!'))) {
	domain = (char*) name;
	domainlen = cp++ - name;
	name = cp;

	/* don't use the domain if it matches the user's domain */
	if (userid && (cp = strchr(userid, '@')) &&
	    (strlen(++cp) == domainlen) && !strncmp(domain, cp, domainlen))
	    domain = NULL;
    }

    strcpy(result, name);

    /* Translate any separators in mailboxname */
    mboxname_hiersep_toexternal(namespace, result, 0);

    resultlen = strlen(result);

    /* Append domain */
    if (domain) {
	if(resultlen+domainlen+1 > MAX_MAILBOX_NAME) 
	    return IMAP_MAILBOX_BADNAME;

	snprintf(result+resultlen, MAX_MAILBOX_NAME+1-resultlen, 
		 "@%.*s", domainlen, domain);
    }

    return 0;
}

/* Handle conversion from the internal namespace to the alternate namespace */
static int mboxname_toexternal_alt(struct namespace *namespace, const char *name,
				  const char *userid, char *result)
{
    char *domain;
    size_t userlen, resultlen;

    /* Blank the result, just in case */
    result[0] = '\0';

    if(strlen(name) > MAX_MAILBOX_NAME) return IMAP_MAILBOX_BADNAME;
    
    if (!userid) return IMAP_MAILBOX_BADNAME;

    userlen = strlen(userid);

    if (config_virtdomains && (domain = strchr(userid, '@'))) {
	size_t domainlen = strlen(domain);

	userlen = domain - userid;

	if (!strncmp(name, domain+1, domainlen-1) &&
	    name[domainlen-1] == '!') {
	    name += domainlen;
	}
    }

    /* Personal (INBOX) namespace */
    if (!strncasecmp(name, "inbox", 5) &&
	(name[5] == '\0' || name[5] == '.')) {
	if (name[5] == '\0')
	    strcpy(result, name);
	else
	    strcpy(result, name+6);
    }
    /* paranoia - this shouldn't be needed */
    else if (!strncmp(name, "user.", 5) &&
	     !strncmp(name+5, userid, userlen) &&
	     (name[5+userlen] == '\0' ||
	      name[5+userlen] == '.')) {
	if (name[5+userlen] == '\0')
	    strcpy(result, "INBOX");
	else
	    strcpy(result, name+5+userlen+1);
    }

    /* Other Users namespace */
    else if (!strncmp(name, "user", 4) &&
	     (name[4] == '\0' || name[4] == '.')) {
	size_t prefixlen = strlen(namespace->prefix[NAMESPACE_USER]);

	if ((prefixlen > MAX_MAILBOX_NAME) || 
	    ((name[4] == '.') && 
	     ((prefixlen+1+strlen(name+5)) > MAX_MAILBOX_NAME)))
	    return IMAP_MAILBOX_BADNAME;

	sprintf(result, "%.*s",
		prefixlen-1, namespace->prefix[NAMESPACE_USER]);
	resultlen = strlen(result);
	if (name[4] == '.') {
	    sprintf(result+resultlen, "%c%s", namespace->hier_sep, name+5);
	}
    }

    /* Shared namespace */
    else {
	/* special case:  LIST/LSUB "" % */
	if (!strncmp(name, namespace->prefix[NAMESPACE_SHARED],
		     strlen(namespace->prefix[NAMESPACE_SHARED])-1)) {
	    strcpy(result, name);
	}
	else {
	    strcpy(result, namespace->prefix[NAMESPACE_SHARED]);
	    strcat(result, name);
	}
    }

    /* Translate any separators in mailboxname */
    mboxname_hiersep_toexternal(namespace, result, 0);
    return 0;
}

/*
 * Create namespace based on config options.
 */
int mboxname_init_namespace(struct namespace *namespace, int isadmin)
{
    const char *prefix;

    assert(namespace != NULL);

    namespace->isadmin = isadmin;

    namespace->hier_sep =
	config_getswitch(IMAPOPT_UNIXHIERARCHYSEP) ? '/' : '.';
    namespace->isalt = !isadmin && config_getswitch(IMAPOPT_ALTNAMESPACE);

    if (namespace->isalt) {
	/* alternate namespace */
	strcpy(namespace->prefix[NAMESPACE_INBOX], "");

	prefix = config_getstring(IMAPOPT_USERPREFIX);
	if (!prefix || strlen(prefix) == 0 ||
	    strlen(prefix) >= MAX_NAMESPACE_PREFIX ||
	    strchr(prefix,namespace->hier_sep) != NULL)
	    return IMAP_NAMESPACE_BADPREFIX;
	sprintf(namespace->prefix[NAMESPACE_USER], "%.*s%c",
		MAX_NAMESPACE_PREFIX-1, prefix, namespace->hier_sep);

	prefix = config_getstring(IMAPOPT_SHAREDPREFIX);
	if (!prefix || strlen(prefix) == 0 ||
	    strlen(prefix) >= MAX_NAMESPACE_PREFIX ||
	    strchr(prefix, namespace->hier_sep) != NULL ||
	    !strncmp(namespace->prefix[NAMESPACE_USER], prefix, strlen(prefix)))
	    return IMAP_NAMESPACE_BADPREFIX;
	sprintf(namespace->prefix[NAMESPACE_SHARED], "%.*s%c",
		MAX_NAMESPACE_PREFIX-1, prefix, namespace->hier_sep); 

	namespace->mboxname_tointernal = mboxname_tointernal_alt;
	namespace->mboxname_toexternal = mboxname_toexternal_alt;
	namespace->mboxlist_findall = mboxlist_findall_alt;
	namespace->mboxlist_findsub = mboxlist_findsub_alt;
    }

    else {
	/* standard namespace */
	sprintf(namespace->prefix[NAMESPACE_INBOX], "%s%c",
		"INBOX", namespace->hier_sep);
	sprintf(namespace->prefix[NAMESPACE_USER], "%s%c",
		"user", namespace->hier_sep);
	strcpy(namespace->prefix[NAMESPACE_SHARED], "");

	namespace->mboxname_tointernal = mboxname_tointernal;
	namespace->mboxname_toexternal = mboxname_toexternal;
	namespace->mboxlist_findall = mboxlist_findall;
	namespace->mboxlist_findsub = mboxlist_findsub;
    }

    return 0;
}

/*
 * Translate separator charactors in a mailboxname from its external
 * representation to its internal representation '.'.
 * If using the unixhierarchysep '/', all '.'s get translated to DOTCHAR.
 */
char *mboxname_hiersep_tointernal(struct namespace *namespace, char *name,
				  int length)
{
    char *p;

    assert(namespace != NULL);
    assert(namespace->hier_sep == '.' || namespace->hier_sep == '/');

    if (!length) length = strlen(name);

    if (namespace->hier_sep == '/') {
	/* change all '/'s to '.' and all '.'s to DOTCHAR */
	for (p = name; *p && length; p++, length--) {
	    if (*p == '/') *p = '.';
	    else if (*p == '.') *p = DOTCHAR;
	}
    }

    return name;
}

/*
 * Translate separator charactors in a mailboxname from its internal
 * representation '.' to its external representation.
 * If using the unixhierarchysep '/', all DOTCHAR get translated to '.'.
 */
char *mboxname_hiersep_toexternal(struct namespace *namespace, char *name,
                                  int length)
{
    char *p;

    assert(namespace != NULL);
    assert(namespace->hier_sep == '.' || namespace->hier_sep == '/');

    if (!length) length=strlen(name);

    if (namespace->hier_sep == '/') {
	/* change all '.'s to '/' and all DOTCHARs to '.' */
	for (p = name; *p && length; p++, length--) {
	    if (*p == '.') *p = '/';
	    else if (*p == DOTCHAR) *p = '.';
	}
    }

    return name;
}

/*
 * Return nonzero if 'userid' owns the (internal) mailbox 'name'.
 */
int mboxname_userownsmailbox(const char *userid, const char *name)
{
    struct namespace internal = { '.', 0, 0, { "INBOX.", "user.", "" },
				  NULL, NULL, NULL, NULL };
    char inboxname[MAX_MAILBOX_NAME+1];

    if (!mboxname_tointernal(&internal, "INBOX", userid, inboxname) &&
	!strncmp(name, inboxname, strlen(inboxname)) &&
	(name[strlen(inboxname)] == '\0' || name[strlen(inboxname)] == '.')) {
	return 1;
    }
    return 0;
}

/*
 * If (internal) mailbox 'name' is a user's mailbox (optionally INBOX),
 * returns a pointer to the userid, otherwise returns NULL.
 */
char *mboxname_isusermailbox(const char *name, int isinbox)
{
    const char *p;

    if (((!strncmp(name, "user.", 5) && (p = name+5)) ||
	 ((p = strstr(name, "!user.")) && (p += 6))) &&
	(!isinbox || !strchr(p, '.')))
	return (char*) p;
    else
	return NULL;
}

/*
 * Apply additional restrictions on netnews mailbox names.
 * Cannot have all-numeric name components.
 */
int mboxname_netnewscheck(char *name)
{
    int c;
    int sawnonnumeric = 0;

    while ((c = *name++)!=0) {
	switch (c) {
	case '.':
	    if (!sawnonnumeric) return IMAP_MAILBOX_BADNAME;
	    sawnonnumeric = 0;
	    break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    break;

	default:
	    sawnonnumeric = 1;
	    break;
	}
    }
    if (!sawnonnumeric) return IMAP_MAILBOX_BADNAME;
    return 0;
}
	    

/*
 * Apply site policy restrictions on mailbox names.
 * Restrictions are hardwired for now.
 */
#define GOODCHARS " +,-.0123456789:=@ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~"
int mboxname_policycheck(char *name)
{
    unsigned i;
    struct glob *g;
    int sawutf7 = 0;
    unsigned c1, c2, c3, c4, c5, c6, c7, c8;
    int ucs4;
    int unixsep;

    unixsep = config_getswitch(IMAPOPT_UNIXHIERARCHYSEP);

    if (strlen(name) > MAX_MAILBOX_NAME) return IMAP_MAILBOX_BADNAME;
    for (i = 0; i < NUM_BADMBOXPATTERNS; i++) {
	g = glob_init(badmboxpatterns[i], 0);
	if (GLOB_TEST(g, name) != -1) {
	    glob_free(&g);
	    return IMAP_MAILBOX_BADNAME;
	}
	glob_free(&g);
    }

    if (*name == '~') return IMAP_MAILBOX_BADNAME;
    while (*name) {
	if (*name == '&') {
	    /* Modified UTF-7 */
	    name++;
	    while (*name != '-') {
		if (sawutf7) {
		    /* Two adjacent utf7 sequences */
		    return IMAP_MAILBOX_BADNAME;
		}

		if ((c1 = CHARMOD64(*name++)) == XX ||
		    (c2 = CHARMOD64(*name++)) == XX ||
		    (c3 = CHARMOD64(*name++)) == XX) {
		    /* Non-base64 character */
		    return IMAP_MAILBOX_BADNAME;
		}
		ucs4 = (c1 << 10) | (c2 << 4) | (c3 >> 2);
		if ((ucs4 & 0xff80) == 0 || (ucs4 & 0xf800) == 0xd800) {
		    /* US-ASCII or multi-word character */
		    return IMAP_MAILBOX_BADNAME;
		}
		if (*name == '-') {
		    /* Trailing bits not zero */
		    if (c3 & 0x03) return IMAP_MAILBOX_BADNAME;

		    /* End of UTF-7 sequence */
		    break;
		}

		if ((c4 = CHARMOD64(*name++)) == XX ||
		    (c5 = CHARMOD64(*name++)) == XX ||
		    (c6 = CHARMOD64(*name++)) == XX) {
		    /* Non-base64 character */
		    return IMAP_MAILBOX_BADNAME;
		}
		ucs4 = ((c3 & 0x03) << 14) | (c4 << 8) | (c5 << 2) | (c6 >> 4);
		if ((ucs4 & 0xff80) == 0 || (ucs4 & 0xf800) == 0xd800) {
		    /* US-ASCII or multi-word character */
		    return IMAP_MAILBOX_BADNAME;
		}
		if (*name == '-') {
		    /* Trailing bits not zero */
		    if (c6 & 0x0f) return IMAP_MAILBOX_BADNAME;

		    /* End of UTF-7 sequence */
		    break;
		}

		if ((c7 = CHARMOD64(*name++)) == XX ||
		    (c8 = CHARMOD64(*name++)) == XX) {
		    /* Non-base64 character */
		    return IMAP_MAILBOX_BADNAME;
		}
		ucs4 = ((c6 & 0x0f) << 12) | (c7 << 6) | c8;
		if ((ucs4 & 0xff80) == 0 || (ucs4 & 0xf800) == 0xd800) {
		    /* US-ASCII or multi-word character */
		    return IMAP_MAILBOX_BADNAME;
		}
	    }

	    if (name[-1] == '&') sawutf7 = 0; /* '&-' is sequence for '&' */
	    else sawutf7 = 1;

	    name++;		/* Skip over terminating '-' */
	}
	else {
	    if (!strchr(GOODCHARS, *name) &&
		/* If we're using unixhierarchysep, DOTCHAR is allowed */
		!(unixsep && *name == DOTCHAR))
		return IMAP_MAILBOX_BADNAME;
	    name++;
	    sawutf7 = 0;
	}
    }
    return 0;
}

