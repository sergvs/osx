divert(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: local_lmtp.m4,v 1.1.1.4 2002/03/12 17:59:48 zarzycki Exp $')
divert(-1)

ifdef(`_MAILER_local_',
	`errprint(`*** FEATURE(local_lmtp) must occur before MAILER(local)
')')dnl

define(`LOCAL_MAILER_PATH',
	ifelse(defn(`_ARG_'), `',
		ifdef(`confEBINDIR', confEBINDIR, `/usr/libexec')`/mail.local',
		_ARG_))
define(`LOCAL_MAILER_FLAGS', `PSXfmnz9')
define(`LOCAL_MAILER_ARGS', `mail.local -l')
define(`LOCAL_MAILER_DSN_DIAGNOSTIC_CODE', `SMTP')
define(`_LOCAL_LMTP_', `1')
