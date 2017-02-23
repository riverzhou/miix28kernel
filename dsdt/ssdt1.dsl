/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20161117-64
 * Copyright (c) 2000 - 2016 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt1.aml, Wed Feb 22 22:30:40 2017
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x000005FB (1531)
 *     Revision         0x01
 *     Checksum         0x37
 *     OEM ID           "INSYDE"
 *     OEM Table ID     "CpuDptf"
 *     OEM Revision     0x00000005 (5)
 *     Compiler ID      "INSY"
 *     Compiler Version 0x0100000D (16777229)
 */
DefinitionBlock ("", "SSDT", 1, "INSYDE", "CpuDptf", 0x00000005)
{
    External (_PR_.CPU0, UnknownObj)
    External (_PR_.CPU0._PPC, IntObj)
    External (_PR_.CPU0._PSS, IntObj)
    External (_PR_.CPU0._PTC, MethodObj)    // 0 Arguments
    External (_PR_.CPU0._TDL, MethodObj)    // 0 Arguments
    External (_PR_.CPU0._TPC, IntObj)
    External (_PR_.CPU0._TSD, MethodObj)    // 0 Arguments
    External (_PR_.CPU0._TSS, MethodObj)    // 0 Arguments
    External (_PR_.CPU1, UnknownObj)
    External (_PR_.CPU2, UnknownObj)
    External (_PR_.CPU3, UnknownObj)
    External (_SB_.DPTF.CTOK, MethodObj)    // 1 Arguments
    External (_SB_.MBID, UnknownObj)
    External (_SB_.PAGD, UnknownObj)
    External (_SB_.PAGD.IDCN, UnknownObj)
    External (ACTT, UnknownObj)
    External (DLPO, UnknownObj)
    External (DPSR, UnknownObj)
    External (PDBG, IntObj)
    External (PUNB, IntObj)
    External (STEP, UnknownObj)

    Scope (\_SB)
    {
        Device (TCPU)
        {
            Name (_HID, EisaId ("INT3401") /* Intel Extended Thermal Model CPU */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (CTYP, Zero)
            Name (_DEP, Package (0x01)  // _DEP: Dependencies
            {
                \_SB.MBID
            })
            Name (CINT, 0x04)
            Name (LSTM, Zero)
            Name (MED4, 0xE00000D4)
            Name (MED0, 0xE00000D0)
            Name (AEXL, Package (0x04)
            {
                "Svchost.exe", 
                "dllhost.exe", 
                "smss.exe", 
                "WinSAT.exe"
            })
            Name (PPCC, Package (0x03)
            {
                0x02, 
                Package (0x06)
                {
                    Zero, 
                    0x03E8, 
                    0x0FA0, 
                    0x03E8, 
                    0x03E8, 
                    0x64
                }, 

                Package (0x06)
                {
                    One, 
                    0x1F40, 
                    0x1F40, 
                    0x03E8, 
                    0x03E8, 
                    0xC8
                }
            })
            Name (CLPO, Package (0x06)
            {
                One, 
                Zero, 
                One, 
                0x19, 
                One, 
                One
            })
            Method (_INI, 0, NotSerialized)  // _INI: Initialize
            {
                CLPO [One] = DerefOf (DLPO [One])
                CLPO [0x02] = DerefOf (DLPO [0x02])
                CLPO [0x03] = DerefOf (DLPO [0x03])
                CLPO [0x04] = DerefOf (DLPO [0x04])
                CLPO [0x05] = DerefOf (DLPO [0x05])
            }

            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If ((DPSR == Zero))
                {
                    Return (Zero)
                }

                Return (0x0F)
            }

            Method (_CRS, 0, Serialized)  // _CRS: Current Resource Settings
            {
                Name (ABUF, ResourceTemplate ()
                {
                    Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, ,, )
                    {
                        0x00000056,
                    }
                })
                Name (BBUF, ResourceTemplate ()
                {
                    Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, ,, )
                    {
                        0x00000056,
                    }
                    Memory32Fixed (ReadWrite,
                        0xFED05000,         // Address Base
                        0x00000800,         // Address Length
                        _Y00)
                })
                CreateDWordField (BBUF, \_SB.TCPU._CRS._Y00._BAS, PUNI)  // _BAS: Base Address
                PUNI = PUNB /* External reference */
                If ((STEP <= 0x04))
                {
                    Return (ABUF) /* \_SB_.TCPU._CRS.ABUF */
                }
                Else
                {
                    Return (BBUF) /* \_SB_.TCPU._CRS.BBUF */
                }
            }

            Method (MBIW, 4, Serialized)
            {
                MED4 = Arg3
                If ((Arg2 == Zero))
                {
                    Local1 = 0x10
                }
                ElseIf ((Arg2 == One))
                {
                    Local1 = 0x30
                }
                Else
                {
                    Local1 = 0xF0
                }

                Local0 = ((Arg0 << 0x10) | (Arg1 << 0x08))
                Local0 |= Local1
                Local0 |= 0x11000000
                MED0 = Local0
            }

            Method (MBIR, 4, Serialized)
            {
                If ((Arg2 == Zero))
                {
                    Local1 = 0x10
                }
                ElseIf ((Arg2 == One))
                {
                    Local1 = 0x30
                }
                Else
                {
                    Local1 = 0xF0
                }

                Local0 = ((Arg0 << 0x10) | (Arg1 << 0x08))
                Local0 |= Local1
                Local0 |= 0x10000000
                MED0 = Local0
                Arg3 = MED4 /* \_SB_.TCPU.MED4 */
            }

            Method (_PPC, 0, NotSerialized)  // _PPC: Performance Present Capabilities
            {
                Debug = "cpudptf: _PPC Called"
                If (CondRefOf (\_PR.CPU0._PPC, Local0))
                {
                    Return (\_PR.CPU0._PPC) /* External reference */
                }
                Else
                {
                    Return (Zero)
                }

                Return (Zero)
            }

            Method (SPPC, 1, Serialized)
            {
                Debug = "cpudptf: SPPC Called"
                \_PR.CPU0._PPC = Arg0
                Notify (\_PR.CPU0, 0x80) // Status Change
                Notify (\_PR.CPU1, 0x80) // Status Change
                Notify (\_PR.CPU2, 0x80) // Status Change
                Notify (\_PR.CPU3, 0x80) // Status Change
                Return (Zero)
            }

            Name (PURE, One)
            Method (SPUR, 1, Serialized)
            {
                Name (_DEP, Package (0x01)  // _DEP: Dependencies
                {
                    \_SB.PAGD
                })
                \_SB.PAGD.IDCN = Arg0
                Notify (\_SB.PAGD, 0x80) // Status Change
                Return (Zero)
            }

            Method (_DTI, 1, NotSerialized)  // _DTI: Device Temperature Indication
            {
                LSTM = Arg0
                If (CondRefOf (\_SB.TCPU._PSV, Local0))
                {
                    Notify (TCPU, 0x91) // Device-Specific
                }
            }

            Method (_NTT, 0, NotSerialized)  // _NTT: Notification Temperature Threshold
            {
                Return (0x0ADE)
            }

            Method (_PSS, 0, NotSerialized)  // _PSS: Performance Supported States
            {
                Debug = "cpudptf: _PSS Called"
                If (CondRefOf (\_PR.CPU0._PSS, Local0))
                {
                    Return (\_PR.CPU0._PSS) /* External reference */
                }
                Else
                {
                    Return (Package (0x01)
                    {
                        Zero
                    })
                }

                Return (Package (0x01)
                {
                    Zero
                })
            }

            Method (_TSS, 0, NotSerialized)  // _TSS: Throttling Supported States
            {
                Debug = "cpudptf: _TSS Called"
                If (CondRefOf (\_PR.CPU0._TSS, Local0))
                {
                    Return (\_PR.CPU0._TSS ())
                }
                Else
                {
                    Return (Package (0x01)
                    {
                        Zero
                    })
                }

                Return (Package (0x01)
                {
                    Zero
                })
            }

            Method (_TPC, 0, NotSerialized)  // _TPC: Throttling Present Capabilities
            {
                Debug = "cpudptf: _TPC Called"
                If (CondRefOf (\_PR.CPU0._TPC, Local0))
                {
                    Return (\_PR.CPU0._TPC) /* External reference */
                }
                Else
                {
                    Return (Zero)
                }

                Return (Zero)
            }

            Method (_PTC, 0, NotSerialized)  // _PTC: Processor Throttling Control
            {
                Debug = "cpudptf: _PTC Called"
                If (CondRefOf (\_PR.CPU0._PTC, Local0))
                {
                    Return (\_PR.CPU0._PTC ())
                }
                Else
                {
                    Return (Package (0x01)
                    {
                        Zero
                    })
                }

                Return (Package (0x01)
                {
                    Zero
                })
            }

            Method (_TSD, 0, NotSerialized)  // _TSD: Throttling State Dependencies
            {
                Debug = "cpudptf: _TSD Called"
                If (CondRefOf (\_PR.CPU0._TSD, Local0))
                {
                    Return (\_PR.CPU0._TSD ())
                }
                Else
                {
                    Return (Package (0x01)
                    {
                        Zero
                    })
                }

                Return (Package (0x01)
                {
                    Zero
                })
            }

            Method (_TDL, 0, NotSerialized)  // _TDL: T-State Depth Limit
            {
                Debug = "cpudptf: _TDL Called"
                If (CondRefOf (\_PR.CPU0._TDL, Local0))
                {
                    Return (\_PR.CPU0._TDL ())
                }
                Else
                {
                    Return (Zero)
                }

                Return (Zero)
            }

            Method (_PDL, 0, NotSerialized)  // _PDL: P-state Depth Limit
            {
                Debug = "cpudptf: _PDL Called"
                If (CondRefOf (\_PR.CPU0._PSS, Local0))
                {
                    Name (LFMI, Zero)
                    LFMI = SizeOf (\_PR.CPU0._PSS)
                    LFMI--
                    Return (LFMI) /* \_SB_.TCPU._PDL.LFMI */
                }
                Else
                {
                    Return (Zero)
                }

                Return (Zero)
            }

            Method (_PSV, 0, Serialized)  // _PSV: Passive Temperature
            {
                Return (\_SB.DPTF.CTOK (ACTT))
            }

            Method (_SCP, 3, Serialized)  // _SCP: Set Cooling Policy
            {
                If (((Arg0 == Zero) || (Arg0 == One)))
                {
                    CTYP = Arg0
                }
            }

            Name (GTSH, 0x14)
            Method (SDBG, 0, NotSerialized)
            {
                Return (PDBG) /* External reference */
            }
        }
    }
}

