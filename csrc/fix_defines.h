/** @file
*   @brief     Needs to be included near the top of socket.h.
*   @details   It would seem TI's CC3000HostDriver conflicts with the macros
*              included with my compiler. This is a quick fix, undefining the
*              system macros before defining TI's.                            */
/*******************************************************************************
* Copyright (c) 2014, Alan Barr
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#ifndef __FIX_DEFINES_H__
#define __FIX_DEFINES_H__

/* timeval structure problem */
#include "cc3000_common.h"

#ifdef FD_SET
#undef FD_SET
#endif

#ifdef FD_CLR
#undef FD_CLR
#endif

#ifdef FD_ISSET
#undef FD_ISSET
#endif

#ifdef FD_ZERO
#undef FD_ZERO
#endif

#ifdef ENOBUFS
#undef ENOBUFS
#endif

#ifdef fd_set
#undef fd_set
#endif

#endif /*__FIX_DEFINES_H__*/

