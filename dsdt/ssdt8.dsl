/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20161117-64
 * Copyright (c) 2000 - 2016 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt8.aml, Wed Feb 22 22:30:40 2017
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x0000017A (378)
 *     Revision         0x01
 *     Checksum         0x42
 *     OEM ID           "LENOVO"
 *     OEM Table ID     "CB-01   "
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "INSY"
 *     Compiler Version 0x0100000D (16777229)
 */
DefinitionBlock ("", "SSDT", 1, "LENOVO", "CB-01   ", 0x00000001)
{
    External (_PR_.CPU0._PTC, MethodObj)    // 0 Arguments
    External (_PR_.CPU0._TSS, MethodObj)    // 0 Arguments
    External (_PR_.CPU1, DeviceObj)
    External (_PR_.CPU2, DeviceObj)
    External (_PR_.CPU3, DeviceObj)
    External (MPEN, UnknownObj)
    External (PDC0, IntObj)

    Scope (\_PR.CPU1)
    {
        Name (_TPC, Zero)  // _TPC: Throttling Present Capabilities
        Method (_PTC, 0, NotSerialized)  // _PTC: Processor Throttling Control
        {
            Return (\_PR.CPU0._PTC ())
        }

        Method (_TSS, 0, NotSerialized)  // _TSS: Throttling Supported States
        {
            Return (\_PR.CPU0._TSS ())
        }

        Method (_TSD, 0, NotSerialized)  // _TSD: Throttling State Dependencies
        {
            If (!(PDC0 & 0x04))
            {
                Return (Package (0x01)
                {
                    Package (0x05)
                    {
                        0x05, 
                        Zero, 
                        Zero, 
                        0xFD, 
                        MPEN
                    }
                })
            }

            Return (Package (0x01)
            {
                Package (0x05)
                {
                    0x05, 
                    Zero, 
                    One, 
                    0xFC, 
                    One
                }
            })
        }
    }

    Scope (\_PR.CPU2)
    {
        Name (_TPC, Zero)  // _TPC: Throttling Present Capabilities
        Method (_PTC, 0, NotSerialized)  // _PTC: Processor Throttling Control
        {
            Return (\_PR.CPU0._PTC ())
        }

        Method (_TSS, 0, NotSerialized)  // _TSS: Throttling Supported States
        {
            Return (\_PR.CPU0._TSS ())
        }

        Method (_TSD, 0, NotSerialized)  // _TSD: Throttling State Dependencies
        {
            If (!(PDC0 & 0x04))
            {
                Return (Package (0x01)
                {
                    Package (0x05)
                    {
                        0x05, 
                        Zero, 
                        Zero, 
                        0xFD, 
                        MPEN
                    }
                })
            }

            Return (Package (0x01)
            {
                Package (0x05)
                {
                    0x05, 
                    Zero, 
                    One, 
                    0xFC, 
                    One
                }
            })
        }
    }

    Scope (\_PR.CPU3)
    {
        Name (_TPC, Zero)  // _TPC: Throttling Present Capabilities
        Method (_PTC, 0, NotSerialized)  // _PTC: Processor Throttling Control
        {
            Return (\_PR.CPU0._PTC ())
        }

        Method (_TSS, 0, NotSerialized)  // _TSS: Throttling Supported States
        {
            Return (\_PR.CPU0._TSS ())
        }

        Method (_TSD, 0, NotSerialized)  // _TSD: Throttling State Dependencies
        {
            If (!(PDC0 & 0x04))
            {
                Return (Package (0x01)
                {
                    Package (0x05)
                    {
                        0x05, 
                        Zero, 
                        Zero, 
                        0xFD, 
                        MPEN
                    }
                })
            }

            Return (Package (0x01)
            {
                Package (0x05)
                {
                    0x05, 
                    Zero, 
                    One, 
                    0xFC, 
                    One
                }
            })
        }
    }
}

