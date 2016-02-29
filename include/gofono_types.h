/*
 * Copyright (C) 2014-2016 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the Jolla Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GOFONO_TYPES_H
#define GOFONO_TYPES_H

#include <gutil_types.h>
#include <gio/gio.h>

#define OFONO_LOG_MODULE gofono_log

G_BEGIN_DECLS

typedef struct ofono_connctx            OfonoConnCtx;
typedef struct ofono_connmgr            OfonoConnMgr;
typedef struct ofono_manager            OfonoManager;
typedef struct ofono_modem              OfonoModem;
typedef struct ofono_netreg             OfonoNetReg;
typedef struct ofono_simmgr             OfonoSimMgr;

typedef enum ofono_connctx_type {
    OFONO_CONNCTX_TYPE_UNKNOWN = -1,
    OFONO_CONNCTX_TYPE_NONE,
    OFONO_CONNCTX_TYPE_INTERNET,        /* internet */
    OFONO_CONNCTX_TYPE_MMS,             /* mms */
    OFONO_CONNCTX_TYPE_WAP,             /* wap */
    OFONO_CONNCTX_TYPE_IMS              /* ims */
} OFONO_CONNCTX_TYPE;

extern GLogModule OFONO_LOG_MODULE;

#define OFONO_INLINE static inline

G_END_DECLS

#endif /* GOFONO_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
