/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20161117-64
 * Copyright (c) 2000 - 2016 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt7.aml, Wed Feb 22 22:30:40 2017
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000290 (656)
 *     Revision         0x01
 *     Checksum         0x92
 *     OEM ID           "LENOVO"
 *     OEM Table ID     "CB-01   "
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "INSY"
 *     Compiler Version 0x0100000D (16777229)
 */
DefinitionBlock ("", "SSDT", 1, "LENOVO", "CB-01   ", 0x00000001)
{
    External (_PR_.CPU0, DeviceObj)
    External (_PSS, IntObj)
    External (CFGD, IntObj)
    External (PDC0, IntObj)

    Scope (\_PR.CPU0)
    {
        Name (_TPC, Zero)  // _TPC: Throttling Present Capabilities
        Method (_PTC, 0, NotSerialized)  // _PTC: Processor Throttling Control
        {
            If ((PDC0 & 0x04))
            {
                Return (Package (0x02)
                {
                    ResourceTemplate ()
                    {
                        Register (FFixedHW, 
                            0x00,               // Bit Width
                            0x00,               // Bit Offset
                            0x0000000000000000, // Address
                            ,)
                    }, 

                    ResourceTemplate ()
                    {
                        Register (FFixedHW, 
                            0x00,               // Bit Width
                            0x00,               // Bit Offset
                            0x0000000000000000, // Address
                            ,)
                    }
                })
            }

            Return (Package (0x02)
            {
                ResourceTemplate ()
                {
                    Register (SystemIO, 
                        0x04,               // Bit Width
                        0x01,               // Bit Offset
                        0x0000000000000410, // Address
                        ,)
                }, 

                ResourceTemplate ()
                {
                    Register (SystemIO, 
                        0x04,               // Bit Width
                        0x01,               // Bit Offset
                        0x0000000000000410, // Address
                        ,)
                }
            })
        }

        Name (TSSI, Package (0x08)
        {
            Package (0x05)
            {
                0x64, 
                0x03E8, 
                Zero, 
                Zero, 
                Zero
            }, 

            Package (0x05)
            {
                0x58, 
                0x036B, 
                Zero, 
                0x0F, 
                Zero
            }, 

            Package (0x05)
            {
                0x4B, 
                0x02EE, 
                Zero, 
                0x0E, 
                Zero
            }, 

            Package (0x05)
            {
                0x3F, 
                0x0271, 
                Zero, 
                0x0D, 
                Zero
            }, 

            Package (0x05)
            {
                0x32, 
                0x01F4, 
                Zero, 
                0x0C, 
                Zero
            }, 

            Package (0x05)
            {
                0x26, 
                0x0177, 
                Zero, 
                0x0B, 
                Zero
            }, 

            Package (0x05)
            {
                0x19, 
                0xFA, 
                Zero, 
                0x0A, 
                Zero
            }, 

            Package (0x05)
            {
                0x0D, 
                0x7D, 
                Zero, 
                0x09, 
                Zero
            }
        })
        Name (TSSM, Package (0x08)
        {
            Package (0x05)
            {
                0x64, 
                0x03E8, 
                Zero, 
                Zero, 
                Zero
            }, 

            Package (0x05)
            {
                0x58, 
                0x036B, 
                Zero, 
                0x1E, 
                Zero
            }, 

            Package (0x05)
            {
                0x4B, 
                0x02EE, 
                Zero, 
                0x1C, 
                Zero
            }, 

            Package (0x05)
            {
                0x3F, 
                0x0271, 
                Zero, 
                0x1A, 
                Zero
            }, 

            Package (0x05)
            {
                0x32, 
                0x01F4, 
                Zero, 
                0x18, 
                Zero
            }, 

            Package (0x05)
            {
                0x26, 
                0x0177, 
                Zero, 
                0x16, 
                Zero
            }, 

            Package (0x05)
            {
                0x19, 
                0xFA, 
                Zero, 
                0x14, 
                Zero
            }, 

            Package (0x05)
            {
                0x0D, 
                0x7D, 
                Zero, 
                0x12, 
                Zero
            }
        })
        Name (TSSF, Zero)
        Method (_TSS, 0, NotSerialized)  // _TSS: Throttling Supported States
        {
            If ((!TSSF && CondRefOf (_PSS)))
            {
                Local0 = _PSS /* External reference */
                Local1 = SizeOf (Local0)
                Local1--
                Local2 = DerefOf (DerefOf (Local0 [Local1]) [One])
                Local3 = Zero
                While ((Local3 < SizeOf (TSSI)))
                {
                    Local4 = ((Local2 * (0x08 - Local3)) / 0x08)
                    DerefOf (TSSI [Local3]) [One] = Local4
                    DerefOf (TSSM [Local3]) [One] = Local4
                    Local3++
                }

                TSSF = Ones
            }

            If ((PDC0 & 0x04))
            {
                Return (TSSM) /* \_PR_.CPU0.TSSM */
            }

            Return (TSSI) /* \_PR_.CPU0.TSSI */
        }

        Method (_TDL, 0, NotSerialized)  // _TDL: T-State Depth Limit
        {
            Debug = "Cpu0: _TDL Called"
            Name (LFMI, Zero)
            LFMI = SizeOf (TSSM)
            LFMI--
            Return (LFMI) /* \_PR_.CPU0._TDL.LFMI */
        }

        Method (_TSD, 0, NotSerialized)  // _TSD: Throttling State Dependencies
        {
            If (((CFGD & 0x00800000) && !(PDC0 & 0x04)))
            {
                Return (Package (0x01)
                {
                    Package (0x05)
                    {
                        0x05, 
                        Zero, 
                        Zero, 
                        0xFD, 
                        0x04
                    }
                })
            }

            If (((CFGD & 0x01000000) && !(PDC0 & 0x04)))
            {
                Return (Package (0x01)
                {
                    Package (0x05)
                    {
                        0x05, 
                        Zero, 
                        Zero, 
                        0xFD, 
                        0x02
                    }
                })
            }

            Return (Package (0x01)
            {
                Package (0x05)
                {
                    0x05, 
                    Zero, 
                    Zero, 
                    0xFC, 
                    One
                }
            })
        }
    }
}

