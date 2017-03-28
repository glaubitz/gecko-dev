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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

extern "C"
{

// Remember that these 'words' are 32bit DWORDS
static PRUint32 invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    PRUint32 result = 0;

    for (PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if (s->IsPtrData())
        {
            result++;
            continue;
        }
        switch (s->type)
        {
            case nsXPTType::T_I8:
            case nsXPTType::T_I16:
            case nsXPTType::T_I32:
                result++;
                break;
            case nsXPTType::T_I64:
                result+=2;
                break;
            case nsXPTType::T_U8:
            case nsXPTType::T_U16:
            case nsXPTType::T_U32:
                result++;
                break;
            case nsXPTType::T_U64:
                result+=2;
                break;
            case nsXPTType::T_FLOAT:
                result++;
                break;
            case nsXPTType::T_DOUBLE:
                result+=2;
                break;
            case nsXPTType::T_BOOL:
            case nsXPTType::T_CHAR:
            case nsXPTType::T_WCHAR:
                result++;
                break;
            default:
                // all the others are plain pointer types
                result++;
                break;
        }
    }
    return result;
}

#define MAX_REG_PARAMS 3 // in C++ first param is 'this - r4'
#define MAX_FREG_PARAMS 8

static PRUint32* invoke_copy_to_stack(PRUint32* stack, PRUint32 paramCount,
        nsXPTCVariant* s)
{
    PRUint32 currentReg = 0;
    PRUint32 currentFReg = 0;
    PRUint32 currentSP = 0;
    PRUint32* freg32;
    PRUint32* target;
    PRUint32 reg32[MAX_REG_PARAMS + MAX_FREG_PARAMS]; // r5 - r7

    freg32 = &reg32[MAX_REG_PARAMS];

    for (PRUint32 i = 0; i < paramCount; i++, s++)
    {
        // Determine target for this parameter
        if (s->IsPtrData())
        {
            if (currentReg < MAX_REG_PARAMS)
            {
                target = &reg32[currentReg];
                currentReg++;
            }
            else
            {
                target = &stack[currentSP];
                currentSP++;
            }
        }
        else
            switch (s->type)
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
                        target = &reg32[currentReg];
                        currentReg++;
                    }
                    else
                    {
                        target = &stack[currentSP];
                        currentSP++;
                    }
                    break;

                    // 64 bit integers
                case nsXPTType::T_I64:
                case nsXPTType::T_U64:
                    if (currentReg < (MAX_REG_PARAMS -1))
                    {
                        target = &reg32[currentReg];
                        currentReg += 2;
                    }
                    else
                    {
                        target = &stack[currentSP];
                        currentSP += 2;
                    }
                    break;

                    // floats
                case nsXPTType::T_FLOAT:
                    if (currentFReg < MAX_FREG_PARAMS)
                    {
                        target = &freg32[currentFReg];
                        currentFReg++;
                    }
                    else
                    {
                        target = &stack[currentSP];
                        currentSP++;
                    }
                    break;

                    // doubles
                case nsXPTType::T_DOUBLE:
                    if (currentFReg < (MAX_FREG_PARAMS -1))
                    {
                        target = &freg32[currentFReg];
                        currentFReg += 2;
                    }
                    else
                    {
                        target = &stack[currentSP];
                        currentSP += 2;
                    }
                    break;
            } // end of switch(s->type)

        // Fill target
        if (s->IsPtrData())
        {
            *((void**)target) = s->ptr;
            continue;
        }

        switch (s->type)
        {
            // 8 and 16 bit types should be promoted to 32 bits
            case nsXPTType::T_I8:
                *((PRUint32*)target) = s->val.i8;
                break;
            case nsXPTType::T_I16:
                *((PRUint32*)target) = s->val.i16;
                break;
            case nsXPTType::T_I32:
                *((PRInt32*) target) = s->val.i32;
                break;
            case nsXPTType::T_I64:
                *((PRInt64*) target) = s->val.i64;
                break;
            case nsXPTType::T_U8:
                *((PRUint32*)target) = s->val.u8;
                break;
            case nsXPTType::T_U16:
                *((PRUint32*)target) = s->val.u16;
                break;
            case nsXPTType::T_U32:
                *((PRUint32*)target) = s->val.u32;
                break;
            case nsXPTType::T_U64:
                *((PRUint64*)target) = s->val.u64;
                break;
            case nsXPTType::T_FLOAT:
                *((float*) target) = s->val.f;
                break;
            case nsXPTType::T_DOUBLE:
                *((double*) target) = s->val.d;
                break;
            case nsXPTType::T_BOOL:
                *((PRBool*) target) = s->val.b;
                break;
            case nsXPTType::T_CHAR:
                *((PRUint32*)target) = s->val.c;
                break;
            case nsXPTType::T_WCHAR:
                *((wchar_t*) target) = s->val.wc;
                break;
            default:
                // all the others are plain pointer types
                *((void**)target) = s->val.p;
                break;
        }
    }
    // Although this is local, there is no problem in returning it.
    return reg32;
}

struct my_params_struct
{
    PRUint32 that; //0
    PRUint32 methodIndex; //4
    PRUint32 paramCount; //8
    PRUint32 params; //12
    PRUint32 n; //16
    PRUint32 copyFunc; //20
};

} //extern "C"


XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
        PRUint32 paramCount, nsXPTCVariant* params)
{
    PRUint32 result;
    my_params_struct locals;
    my_params_struct* pLocals = &locals;

    pLocals->that = (PRUint32)that;
    pLocals->methodIndex = (PRUint32)methodIndex;
    pLocals->paramCount = (PRUint32)paramCount;
    pLocals->params = (PRUint32)params;
    pLocals->n = (PRUint32)invoke_count_words(paramCount, params) * 4;
    pLocals->copyFunc = (PRUint32)invoke_copy_to_stack;

    __asm__ __volatile__(
        "mov.l   %1, r8         \n\t" // get locals to r8
        "mov.l   @(16,r8), r0   \n\t" // make space on stack
        "sub     r0, r15        \n\t"

        //Prepare call frame for invoke_copy_to_stack
        "mov     r15, r4        \n\t" // pass stack ptr
        "mov.l   @(8,r8), r5    \n\t" // paramCount
        "mov.l   @(12,r8), r6   \n\t" // params
        "mov.l   @(20,r8), r0   \n\t" // pointer to invoke_copy_to_stack
        "mov     r1, r9         \n\t" // save assembler regs
        "mov     r2, r10        \n\t"
        "mov     r3, r11        \n\t"
        "jsr     @r0            \n\t" // Call invoke_copy_to_stack
        "nop                    \n\t"
        "mov     r9, r1         \n\t" // Restore assembler regs
        "mov     r10, r2        \n\t"
        "mov     r11, r3        \n\t"
        "mov.l   @r0+, r5       \n\t" // Get regs
        "mov.l   @r0+, r6       \n\t"
        "mov.l   @r0+, r7       \n\t"
        "fmov.s  @r0+, fr5      \n\t" // Get FP regs
        "fmov.s  @r0+, fr4      \n\t"
        "fmov.s  @r0+, fr7      \n\t"
        "fmov.s  @r0+, fr6      \n\t"
        "fmov.s  @r0+, fr9      \n\t"
        "fmov.s  @r0+, fr8      \n\t"
        "fmov.s  @r0+, fr11     \n\t"
        "fmov.s  @r0+, fr10     \n\t"

        //Prepare call frame for method
        "mov.l   @r8, r4        \n\t" // get that
        "mov.l   @r4, r4        \n\t" // get vtable
        "mov.l   @(4,r8), r0    \n\t" // method index
        "shll2   r0             \n\t" // method offset
        "add     r0, r4         \n\t"
        "mov.l   @r4, r0        \n\t" // get method
        "mov.l   @r8, r4        \n\t" // get that
        "mov     r1, r9         \n\t" // save assembler regs
        "mov     r2, r10        \n\t"
        "mov     r3, r11        \n\t"
        "jsr	 @r0            \n\t" // Call method
        "nop                    \n\t"
        "mov     r9, r1         \n\t" // restore assembler regs
        "mov     r10, r2        \n\t"
        "mov     r11, r3        \n\t"
        "mov.l   r0, %0         \n\t" // copy result

        : "=g" (result) /* %0 */
        : "g" (pLocals) /* %1 */
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "memory"
    );

    return result;
}

