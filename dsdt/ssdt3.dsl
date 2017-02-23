/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20161117-64
 * Copyright (c) 2000 - 2016 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt3.aml, Wed Feb 22 22:30:40 2017
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000058 (88)
 *     Revision         0x01
 *     Checksum         0x1D
 *     OEM ID           "INSYDE"
 *     OEM Table ID     "LowPwrM"
 *     OEM Revision     0x00000005 (5)
 *     Compiler ID      "INSY"
 *     Compiler Version 0x0100000D (16777229)
 */
DefinitionBlock ("", "SSDT", 1, "INSYDE", "LowPwrM", 0x00000005)
{
    External (_SB_.DPTF, DeviceObj)
    External (LPMV, IntObj)

    Scope (\_SB.DPTF)
    {
        Name (LPSP, Package (0x01)
        {
            ToUUID ("b9455b06-7949-40c6-abf2-363a70c8706c")
        })
        Method (CLPM, 0, NotSerialized)
        {
            Return (LPMV) /* External reference */
        }
    }
}

