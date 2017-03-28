/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * SH4 porting by
 * M. Onur Okyay - Vestel Electronics <mehmetonurokyay@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implement shared vtbl methods. */

#include <stdio.h>
#include "xptcprivate.h"

#if !defined(LINUX) || !defined(__sh__)
#error "This code is for Linux SH4 only. Please check if it works for you, too.\nDepends strongly on gcc behaviour."
#endif

#define MAX_REG_PARAMS 3 // in C++ first param is 'this - r4'
#define MAX_FREG_PARAMS 8
#define PARAM_BUFFER_COUNT 16

static nsresult PrepareAndDispatch(nsXPTCStubBase* self, PRUint32 methodIndex,
        PRUint32* reg32, PRUint32* stack)
{
    nsXPTCMiniVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCMiniVariant* dispatchParams = NULL;
    nsIInterfaceInfo* iface_info = NULL;
    const nsXPTMethodInfo* info;
    PRUint8 paramCount;
    PRUint8 i;
    nsresult result = NS_ERROR_FAILURE;

    PRUint32 currentReg = 0;
    PRUint32 currentFReg = 0;
    PRUint32 currentSP = 0;
    PRUint32 *freg32;
    PRUint32* source;
    char *name;

    freg32 = &reg32[MAX_REG_PARAMS];

    NS_ASSERTION(self, "no self");

    self->GetInterfaceInfo(&iface_info);
    NS_ASSERTION(iface_info, "no interface info");

    iface_info->GetName(&name);
    iface_info->GetMethodInfo(PRUint16(methodIndex), &info);
    NS_ASSERTION(info, "no interface info");
    if (!info)
        return NS_ERROR_UNEXPECTED;

    paramCount = info->GetParamCount();

    // setup variant array pointer
    if (paramCount > PARAM_BUFFER_COUNT)
        dispatchParams = new nsXPTCMiniVariant[paramCount];
    else
        dispatchParams = paramBuffer;

    NS_ASSERTION(dispatchParams, "no place for params");
    if (! dispatchParams)
        return NS_ERROR_OUT_OF_MEMORY;

    for (i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTCMiniVariant* dp = &dispatchParams[i];

        // Determine source for this parameter
        if (param.IsOut() || !type.IsArithmetic())
        {
            if (currentReg < MAX_REG_PARAMS)
            {
                source = &reg32[currentReg];
                currentReg++;
            }
            else
            {
                source = &stack[currentSP];
                currentSP++;
            }
        }
        else
        {
            switch (type)
            {
                // 32 bit integers
                default:
                case nsXPTType::T_I8:
                case nsXPTType::T_I16:
                case nsXPTType::T_I32:
                case nsXPTType::T_U8:
                case nsXPTType::T_U16:
                case nsXPTType::T_U32:
                case nsXPTType::T_BOOL:
                case nsXPTType::T_CHAR:
                case nsXPTType::T_WCHAR:
                    if (currentReg < MAX_REG_PARAMS)
                    {
                        source = &reg32[currentReg];
                        currentReg++;
                    }
                    else
                    {
                        source = &stack[currentSP];
                        currentSP++;
                    }
                    break;

                    // 64 bit integers
                case nsXPTType::T_I64:
                case nsXPTType::T_U64:
                    if (currentReg < (MAX_REG_PARAMS -1))
                    {
                        source = &reg32[currentReg];
                        currentReg += 2;
                    }
                    else
                    {
                        source = &stack[currentSP];
                        currentSP += 2;
                    }
                    break;

                    // floats
                case nsXPTType::T_FLOAT:
                    if (currentFReg < MAX_FREG_PARAMS)
                    {
                        source = &freg32[currentFReg];
                        currentFReg++;
                    }
                    else
                    {
                        source = &stack[currentSP];
                        currentSP++;
                    }
                    break;

                    // doubles
                case nsXPTType::T_DOUBLE:
                    if (currentFReg < (MAX_FREG_PARAMS -1))
                    {
                        source = &freg32[currentFReg];
                        currentFReg += 2;
                    }
                    else
                    {
                        source = &stack[currentSP];
                        currentSP += 2;
                    }
                    break;
            } // end of switch(type)
        }

        if (param.IsOut() || !type.IsArithmetic())
        {
            dp->val.p = (void*) *source;
            continue;
        }

        switch (type)
        {
            case nsXPTType::T_I8:
                dp->val.i8 = *((PRUint32*)source);
                break;
            case nsXPTType::T_I16:
                dp->val.i16 = *((PRUint32*)source);
                break;
            case nsXPTType::T_I32:
                dp->val.i32 = *((PRInt32*) source);
                break;
            case nsXPTType::T_I64:
                dp->val.i64 = *((PRInt64*) source);
                break;
            case nsXPTType::T_U8:
                dp->val.u8 = *((PRUint32*)source);
                break;
            case nsXPTType::T_U16:
                dp->val.u16 = *((PRUint32*)source);
                break;
            case nsXPTType::T_U32:
                dp->val.u32 = *((PRUint32*)source);
                break;
            case nsXPTType::T_U64:
                dp->val.u64 = *((PRUint64*)source);
                break;
            case nsXPTType::T_FLOAT:
                dp->val.f = *((float*) source);
                break;
            case nsXPTType::T_DOUBLE:
                dp->val.d = *((double*) source);
                break;
            case nsXPTType::T_BOOL:
                dp->val.b = *((PRBool*) source);
                break;
            case nsXPTType::T_CHAR:
                dp->val.c = *((PRUint32*)source);
                break;
            case nsXPTType::T_WCHAR:
                dp->val.wc = *((wchar_t*) source);
                break;
            default:
                NS_ASSERTION(0, "bad type");
                break;
        }
    } //End of for

    result = self->CallMethod((PRUint16)methodIndex, info, dispatchParams);

    NS_RELEASE(iface_info);

    if (dispatchParams != paramBuffer)
        delete [] dispatchParams;

    return result;
} // End of PrepareAndDispatch


#define STUB_ENTRY(n)                                       \
nsresult nsXPTCStubBase::Stub##n ()                         \
{                                                           \
    register nsresult result asm("r0");                     \
    PRUint32 dispatchFunc /*asm("r1")*/;                    \
    dispatchFunc = (PRUint32)PrepareAndDispatch;            \
    __asm__ __volatile__(                                   \
        /* push registers*/                                 \
        "fmov.s fr10, @-r15     \n\t"                       \
        "fmov.s fr11, @-r15     \n\t"                       \
        "fmov.s fr8, @-r15      \n\t"                       \
        "fmov.s fr9, @-r15      \n\t"                       \
        "fmov.s fr6, @-r15      \n\t"                       \
        "fmov.s fr7, @-r15      \n\t"                       \
        "fmov.s fr4, @-r15      \n\t"                       \
        "fmov.s fr5, @-r15      \n\t"                       \
        "mov.l  r7, @-r15       \n\t"                       \
        "mov.l  r6, @-r15       \n\t"                       \
        "mov.l  r5, @-r15       \n\t"                       \
        /*Prepare call frame*/                              \
        /* 'that' is in r4 already */                       \
        /*"mov.l  %1, r4\n\t"*//* pass that*/               \
        "mov    #"#n", r5       \n\t" /*pass methodIndex*/  \
        "mov    r15, r6         \n\t" /*pass registers*/    \
        "mov    r14, r7         \n\t" /*pass originalstack*/\
        "add    #16, r7         \n\t"                       \
        "mov.l  %1, r0          \n\t"                       \
        /*Save return address*/ 	                    \
        "sts.l  pr,@-r15        \n\t"                       \
        /*Save locals*/				            \
        "mov.l  r1, @-r15       \n\t"                       \
        "mov.l  r2, @-r15       \n\t"                       \
        "mov.l  r3, @-r15       \n\t"                       \
        /*Call PrepareAndDispatch */                        \
        "jsr    @r0             \n\t"                       \
        "nop                    \n\t"                       \
        /*Restore locals*/			            \
        "mov.l  @r15+, r3       \n\t"                       \
        "mov.l  @r15+, r2       \n\t"                       \
        "mov.l  @r15+, r1       \n\t"                       \
        /*Restore return address*/ 	                    \
        "lds.l  @r15+,pr        \n\t"                       \
                                                            \
        : "=g"  (result)        /* %0 */                    \
        : "g"  (dispatchFunc)   /* %1 */                    \
        : "r4", "r5", "r6", "r7", "memory"                  \
    );                                                      \
    return result;                                          \
}

#define SENTINEL_ENTRY(n)                                   \
nsresult nsXPTCStubBase::Sentinel##n()                      \
{                                                           \
    NS_ASSERTION(0,"nsXPTCStubBase::Sentinel called");      \
    return NS_ERROR_NOT_IMPLEMENTED;                        \
}

#include "xptcstubsdef.inc"

