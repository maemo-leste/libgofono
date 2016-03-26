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

#ifndef GOFONO_MODEM_INTF_PRIVATE_H
#define GOFONO_MODEM_INTF_PRIVATE_H

#include "gofono_modemintf.h"
#include "gofono_object_p.h"

typedef struct ofono_modem_interface_class {
    OfonoObjectClass object;
} OfonoModemInterfaceClass;

GType ofono_modem_interface_get_type(void);
#define OFONO_TYPE_MODEM_INTERFACE (ofono_modem_interface_get_type())
#define OFONO_MODEM_INTERFACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_MODEM_INTERFACE, OfonoModemInterface))
#define OFONO_MODEM_INTERFACE_CLASS(kl) (G_TYPE_CHECK_CLASS_CAST((kl), \
        OFONO_TYPE_MODEM_INTERFACE, OfonoModemInterfaceClass))
#define OFONO_MODEM_INTERFACE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),\
        OFONO_TYPE_MODEM_INTERFACE, OfonoModemInterfaceClass))

void
ofono_modem_interface_initialize(
    OfonoModemInterface* intf,
    const char* intf_name,
    const char* path);

#endif /* GOFONO_MODEM_INTF_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
